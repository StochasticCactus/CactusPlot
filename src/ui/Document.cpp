// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/Document.h"

#include <QDateTime>
#include <QFileInfo>

#include "core/Presets.h"

namespace cactus::ui {

namespace {

/// Shape of the project (which graphs/sets/objects exist) to detect structural changes.
QString structureKey(const Project& p)
{
    QString k;
    for (int g = 0; g < p.graphs.size(); ++g) {
        const Graph& gr = p.graphs[g];
        k += gr.active ? QLatin1Char('G') : QLatin1Char('g');
        for (const DataSet& s : gr.sets) {
            k += s.active ? QLatin1Char('S') : QLatin1Char('s');
            if (s.active) k += s.hidden ? QLatin1Char('h') : QLatin1Char('v');
        }
        k += QLatin1Char('|');
    }
    k += QString::number(p.objects.size());
    return k;
}

}  // namespace

class SnapshotCommand : public QUndoCommand {
public:
    SnapshotCommand(Document* doc, const QString& text, Project before, Project after, QString key)
        : QUndoCommand(text), m_doc(doc), m_before(std::move(before)), m_after(std::move(after)), m_key(std::move(key)),
          m_time(QDateTime::currentMSecsSinceEpoch())
    {
    }
    void undo() override { m_doc->restore(m_before); }
    void redo() override
    {
        if (m_first) {
            m_first = false;  // the change is already applied
            return;
        }
        m_doc->restore(m_after);
    }
    int id() const override { return m_key.isEmpty() ? -1 : 4711; }
    bool mergeWith(const QUndoCommand* other) override
    {
        const auto* o = static_cast<const SnapshotCommand*>(other);
        if (o->m_key != m_key || o->m_time - m_time > 1500) return false;
        m_after = o->m_after;
        m_time = o->m_time;
        return true;
    }

private:
    Document* m_doc;
    Project m_before, m_after;
    QString m_key;
    qint64 m_time;
    bool m_first = true;
};

Document::Document(QObject* parent) : QObject(parent)
{
    m_project = presets::newProject();
    connect(&m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { emit modifiedChanged(!clean); });
}

Document::~Document()
{
    // the undo stack emits cleanChanged() while being destroyed
    m_undo.blockSignals(true);
    blockSignals(true);
}

void Document::setProject(const Project& p, const QString& path)
{
    m_project = p;
    if (!m_project.graph(m_project.currentGraph)) {
        m_project.currentGraph = 0;
        for (int i = 0; i < m_project.graphs.size(); ++i)
            if (m_project.graphs[i].active) { m_project.currentGraph = i; break; }
    }
    m_undo.clear();
    m_undo.setClean();
    m_path = path;
    m_sel = Selection{};  // nothing selected: no markers over a freshly opened plot
    emit filePathChanged();
    emit structureChanged();
    emit changed();
    emit selectionChanged();
}

void Document::modify(const QString& description, const std::function<void(Project&)>& fn, const QString& mergeKey)
{
    Project before = m_project;
    const QString keyBefore = structureKey(m_project);
    fn(m_project);
    m_undo.push(new SnapshotCommand(this, description, before, m_project, mergeKey));
    if (structureKey(m_project) != keyBefore) emit structureChanged();
    emit changed();
}

void Document::modifySilently(const std::function<void(Project&)>& fn)
{
    fn(m_project);
    emit changed();
}

void Document::restore(const Project& p)
{
    const QString keyBefore = structureKey(m_project);
    m_project = p;
    // keep the selection valid
    if (m_sel.graph >= 0 && !m_project.graph(m_sel.graph)) m_sel = Selection{Selection::Page, -1, -1};
    if (m_sel.kind == Selection::Set && (!m_project.graph(m_sel.graph) || !m_project.graph(m_sel.graph)->set(m_sel.index)))
        m_sel = Selection{Selection::Graph, m_sel.graph, -1};
    if (m_sel.kind == Selection::Object && (m_sel.index < 0 || m_sel.index >= m_project.objects.size()))
        m_sel = Selection{Selection::Page, -1, -1};
    if (structureKey(m_project) != keyBefore) emit structureChanged();
    emit changed();
    emit selectionChanged();
}

void Document::setFilePath(const QString& p)
{
    m_path = p;
    emit filePathChanged();
}

QString Document::displayName() const
{
    return m_path.isEmpty() ? tr("Untitled") : QFileInfo(m_path).fileName();
}

void Document::select(const Selection& s)
{
    if (s == m_sel) return;
    m_sel = s;
    if (s.graph >= 0 && m_project.graph(s.graph)) m_project.currentGraph = s.graph;
    emit selectionChanged();
}

int Document::currentGraph() const
{
    if (m_project.graph(m_project.currentGraph)) return m_project.currentGraph;
    for (int i = 0; i < m_project.graphs.size(); ++i)
        if (m_project.graphs[i].active) return i;
    return -1;
}

void Document::setCurrentGraph(int g)
{
    if (!m_project.graph(g)) return;
    m_project.currentGraph = g;
    emit changed();
}

}  // namespace cactus::ui
