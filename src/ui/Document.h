// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - the open document: project + file + undo stack + selection.
#pragma once

#include <QObject>
#include <QString>
#include <QUndoStack>
#include <functional>

#include "core/Model.h"

namespace cactus::ui {

struct Selection {
    enum Kind { None, Page, Graph, Axis, Set, Legend, Title, Object };
    Kind kind = None;
    int graph = -1;
    int index = -1;  ///< axis index, set index or object index
    bool operator==(const Selection& o) const { return kind == o.kind && graph == o.graph && index == o.index; }
    bool operator!=(const Selection& o) const { return !(*this == o); }
};

class Document : public QObject {
    Q_OBJECT
public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    const Project& project() const { return m_project; }
    /// Replace the whole project (clears undo history).
    void setProject(const Project& p, const QString& path = QString());

    /// Apply an undoable change. Changes with the same non-empty mergeKey made
    /// in quick succession (e.g. while dragging a slider) merge into one step.
    void modify(const QString& description, const std::function<void(Project&)>& fn, const QString& mergeKey = QString());
    /// Change without recording undo (view-only state such as the current graph).
    void modifySilently(const std::function<void(Project&)>& fn);

    QUndoStack* undoStack() { return &m_undo; }

    QString filePath() const { return m_path; }
    void setFilePath(const QString& p);
    QString displayName() const;
    bool isModified() const { return !m_undo.isClean(); }
    void markSaved() { m_undo.setClean(); }

    const Selection& selection() const { return m_sel; }
    void select(const Selection& s);
    int currentGraph() const;
    void setCurrentGraph(int g);

signals:
    void changed();            ///< the project changed (redraw)
    void structureChanged();   ///< graphs/sets/objects added or removed
    void selectionChanged();
    void filePathChanged();
    void modifiedChanged(bool);

private:
    friend class SnapshotCommand;
    void restore(const Project& p);
    Project m_project;
    QString m_path;
    QUndoStack m_undo;
    Selection m_sel;
};

}  // namespace cactus::ui
