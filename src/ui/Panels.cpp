// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/Panels.h"

#include <QApplication>
#include <QSet>
#include <climits>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include "core/Presets.h"
#include "io/DataReader.h"
#include "math/Transforms.h"
#include "plot/RichText.h"
#include "ui/Document.h"
#include "ui/Theme.h"

namespace cactus::ui {

// ====================================================================== tree

ProjectTree::ProjectTree(Document* doc, QWidget* parent) : QTreeWidget(parent), m_doc(doc)
{
    setHeaderHidden(true);
    setColumnCount(1);
    setUniformRowHeights(true);
    setIconSize(QSize(16, 16));
    setSelectionMode(QAbstractItemView::SingleSelection);
    connect(doc, &Document::structureChanged, this, &ProjectTree::rebuild);
    connect(doc, &Document::changed, this, &ProjectTree::rebuild);
    connect(doc, &Document::selectionChanged, this, &ProjectTree::syncSelection);
    connect(this, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* it) {
        if (m_updating || !it) return;
        const QVariantList v = it->data(0, Qt::UserRole).toList();
        if (v.size() != 3) return;
        m_doc->select(Selection{Selection::Kind(v[0].toInt()), v[1].toInt(), v[2].toInt()});
    });
    connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* it) {
        if (m_updating) return;
        const QVariantList v = it->data(0, Qt::UserRole).toList();
        if (v.size() != 3) return;
        const bool visible = it->checkState(0) == Qt::Checked;
        const int kind = v[0].toInt(), g = v[1].toInt(), i = v[2].toInt();
        if (kind == Selection::Set)
            m_doc->modify(visible ? tr("Show set") : tr("Hide set"), [=](Project& p) {
                if (Graph* gr = p.graph(g)) if (DataSet* s = gr->set(i)) s->hidden = !visible;
            });
        else if (kind == Selection::Graph)
            m_doc->modify(visible ? tr("Show graph") : tr("Hide graph"), [=](Project& p) {
                if (Graph* gr = p.graph(g)) gr->hidden = !visible;
            });
        else if (kind == Selection::Object)
            m_doc->modify(visible ? tr("Show annotation") : tr("Hide annotation"), [=](Project& p) {
                if (i >= 0 && i < p.objects.size()) p.objects[i].active = visible;
            });
    });
    connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it) {
        const QVariantList v = it->data(0, Qt::UserRole).toList();
        if (v.size() == 3 && v[0].toInt() == Selection::Set) emit editDataRequested(v[1].toInt(), v[2].toInt());
        else emit propertiesRequested();
    });
    rebuild();
}

void ProjectTree::rebuild()
{
    m_updating = true;
    // remember expansion
    QSet<QString> collapsed;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem* top = topLevelItem(i);
        for (int k = 0; k < top->childCount(); ++k)
            if (!top->child(k)->isExpanded()) collapsed.insert(top->child(k)->text(0));
    }
    clear();
    const Project& p = m_doc->project();
    auto mk = [](QTreeWidgetItem* parent, const QString& text, const QIcon& icon, Selection::Kind k, int g, int i) {
        auto* it = new QTreeWidgetItem(parent, QStringList{text});
        it->setIcon(0, icon);
        it->setData(0, Qt::UserRole, QVariantList{int(k), g, i});
        return it;
    };
    auto* page = new QTreeWidgetItem(this, QStringList{tr("Page (%1 × %2 pt)").arg(p.page.width).arg(p.page.height)});
    page->setIcon(0, icons::get(QStringLiteral("page")));
    page->setData(0, Qt::UserRole, QVariantList{int(Selection::Page), -1, -1});
    for (int g = 0; g < p.graphs.size(); ++g) {
        const Graph& gr = p.graphs[g];
        if (!gr.active) continue;
        QString name = QStringLiteral("G%1").arg(g);
        const QString t = richtext::plainText(gr.title.text);
        if (!t.isEmpty()) name += QStringLiteral("  ") + t;
        auto* gi = mk(page, name, icons::get(QStringLiteral("graph")), Selection::Graph, g, -1);
        gi->setFlags(gi->flags() | Qt::ItemIsUserCheckable);
        gi->setCheckState(0, gr.hidden ? Qt::Unchecked : Qt::Checked);
        if (g == p.currentGraph) {
            QFont f = gi->font(0);
            f.setBold(true);
            gi->setFont(0, f);
        }
        static const char* axisNames[] = {"X axis", "Y axis", "Alt X axis", "Alt Y axis"};
        auto* axes = new QTreeWidgetItem(gi, QStringList{tr("Axes")});
        axes->setIcon(0, icons::get(QStringLiteral("axis")));
        axes->setData(0, Qt::UserRole, QVariantList{int(Selection::Axis), g, 0});
        for (int a = 0; a < 4; ++a) {
            QString an = tr(axisNames[a]);
            if (!gr.axes[a].label.isEmpty()) an += QStringLiteral(": ") + richtext::plainText(gr.axes[a].label);
            if (!gr.axes[a].active) an += tr(" (off)");
            mk(axes, an, icons::get(QStringLiteral("axis")), Selection::Axis, g, a);
        }
        axes->setExpanded(false);
        mk(gi, tr("Legend"), icons::get(QStringLiteral("legend")), Selection::Legend, g, -1);
        for (int s = 0; s < gr.sets.size(); ++s) {
            const DataSet& d = gr.sets[s];
            if (!d.active) continue;
            QString sn = QStringLiteral("S%1").arg(s);
            const QString leg = richtext::plainText(d.legend);
            if (!leg.isEmpty()) sn += QStringLiteral("  ") + leg;
            else if (!d.comment.isEmpty()) sn += QStringLiteral("  ") + d.comment;
            sn += QStringLiteral("  [%1, %2]").arg(setTypeName(d.type)).arg(d.size());
            QColor c = d.line.visible() && d.lineType != LineType::None ? d.line.color : d.symbol.fill.color;
            if (d.type == SetType::BAR || d.type == SetType::BARDY || d.type == SetType::BARDYDY) c = d.symbol.fill.color;
            auto* si = mk(gi, sn, icons::swatch(c, 14), Selection::Set, g, s);
            si->setFlags(si->flags() | Qt::ItemIsUserCheckable);
            si->setCheckState(0, d.hidden ? Qt::Unchecked : Qt::Checked);
            si->setToolTip(0, d.comment);
        }
        gi->setExpanded(!collapsed.contains(gi->text(0)));
    }
    if (!p.objects.isEmpty()) {
        auto* objs = new QTreeWidgetItem(page, QStringList{tr("Annotations")});
        objs->setIcon(0, icons::get(QStringLiteral("text")));
        for (int i = 0; i < p.objects.size(); ++i) {
            const DrawObject& o = p.objects[i];
            QString n;
            QString icon;
            switch (o.kind) {
            case DrawObjectKind::Text: n = QStringLiteral("“%1”").arg(richtext::plainText(o.text).left(30)); icon = QStringLiteral("text"); break;
            case DrawObjectKind::Line: n = o.arrow ? tr("Arrow") : tr("Line"); icon = o.arrow ? QStringLiteral("arrow") : QStringLiteral("line"); break;
            case DrawObjectKind::Box: n = tr("Box"); icon = QStringLiteral("box"); break;
            case DrawObjectKind::Ellipse: n = tr("Ellipse"); icon = QStringLiteral("ellipse"); break;
            }
            auto* it = mk(objs, n, icons::get(icon), Selection::Object, o.graph, i);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(0, o.active ? Qt::Checked : Qt::Unchecked);
        }
        objs->setExpanded(!collapsed.contains(objs->text(0)));
    }
    page->setExpanded(true);
    m_updating = false;
    syncSelection();
}

void ProjectTree::syncSelection()
{
    m_updating = true;
    const Selection& s = m_doc->selection();
    QTreeWidgetItemIterator it(this);
    while (*it) {
        const QVariantList v = (*it)->data(0, Qt::UserRole).toList();
        if (v.size() == 3) {
            Selection::Kind k = Selection::Kind(v[0].toInt());
            const bool same = k == s.kind && v[1].toInt() == s.graph &&
                              (k == Selection::Graph || k == Selection::Legend || k == Selection::Page || v[2].toInt() == s.index);
            if (same || (s.kind == Selection::Title && k == Selection::Graph && v[1].toInt() == s.graph)) {
                setCurrentItem(*it);
                scrollToItem(*it);
                break;
            }
        }
        ++it;
    }
    m_updating = false;
}

void ProjectTree::contextMenuEvent(QContextMenuEvent* e)
{
    QTreeWidgetItem* it = itemAt(e->pos());
    if (!it) return;
    const QVariantList v = it->data(0, Qt::UserRole).toList();
    if (v.size() != 3) return;
    const int kind = v[0].toInt(), g = v[1].toInt(), i = v[2].toInt();
    QMenu menu(this);
    menu.addAction(icons::get(QStringLiteral("sliders")), tr("Properties"), this, [this] { emit propertiesRequested(); });
    if (kind == Selection::Set) {
        menu.addAction(icons::get(QStringLiteral("table")), tr("Edit data..."), this, [this, g, i] { emit editDataRequested(g, i); });
        menu.addAction(icons::get(QStringLiteral("copy")), tr("Duplicate"), this, [this, g, i] {
            m_doc->modify(tr("Duplicate set"), [g, i](Project& p) {
                Graph* gr = p.graph(g);
                if (!gr || !gr->set(i)) return;
                const DataSet copy = gr->sets[i];
                const int ni = gr->nextFreeSet();
                gr->ensureSet(ni) = copy;
            });
        });
        menu.addAction(icons::get(QStringLiteral("autoscale")), tr("Autoscale to this set"), this, [this, g, i] {
            m_doc->modify(tr("Autoscale"), [g, i](Project& p) { if (Graph* gr = p.graph(g)) gr->autoscale(3, i); });
        });
        menu.addSeparator();
        menu.addAction(icons::get(QStringLiteral("trash")), tr("Delete set"), this, [this, g, i] {
            m_doc->modify(tr("Delete set"), [g, i](Project& p) { if (Graph* gr = p.graph(g)) if (i < gr->sets.size()) gr->sets[i] = DataSet(); });
        });
    } else if (kind == Selection::Graph) {
        menu.addAction(icons::get(QStringLiteral("autoscale")), tr("Autoscale"), this, [this, g] {
            m_doc->modify(tr("Autoscale"), [g](Project& p) { if (Graph* gr = p.graph(g)) gr->autoscale(); });
        });
        menu.addAction(tr("Make current"), this, [this, g] { m_doc->setCurrentGraph(g); });
        menu.addAction(icons::get(QStringLiteral("copy")), tr("Duplicate graph"), this, [this, g] {
            m_doc->modify(tr("Duplicate graph"), [g](Project& p) {
                if (!p.graph(g)) return;
                Graph copy = p.graphs[g];
                copy.vxmin += 0.05; copy.vxmax += 0.05; copy.vymin -= 0.05; copy.vymax -= 0.05;
                p.graphs.push_back(copy);
            });
        });
        menu.addSeparator();
        menu.addAction(icons::get(QStringLiteral("trash")), tr("Delete graph"), this, [this, g] {
            m_doc->modify(tr("Delete graph"), [g](Project& p) { if (g < p.graphs.size()) p.graphs[g] = Graph(); });
            m_doc->select(Selection{Selection::Page, -1, -1});
        });
    } else if (kind == Selection::Object) {
        menu.addAction(icons::get(QStringLiteral("copy")), tr("Duplicate"), this, [this, i] {
            m_doc->modify(tr("Duplicate annotation"), [i](Project& p) {
                if (i < 0 || i >= p.objects.size()) return;
                DrawObject o = p.objects[i];
                o.pos += QPointF(0.02, -0.02);
                o.p1 += QPointF(0.02, -0.02);
                o.p2 += QPointF(0.02, -0.02);
                p.objects.push_back(o);
            });
        });
        menu.addAction(icons::get(QStringLiteral("trash")), tr("Delete"), this, [this, i] {
            m_doc->modify(tr("Delete annotation"), [i](Project& p) { if (i >= 0 && i < p.objects.size()) p.objects.remove(i); });
            m_doc->select(Selection{Selection::Page, -1, -1});
        });
    }
    menu.exec(e->globalPos());
}

// ====================================================================== console

Console::Console(Document* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);
    m_out = new QPlainTextEdit(this);
    m_out->setReadOnly(true);
    m_out->setMaximumBlockCount(5000);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_out->setFont(mono);
    lay->addWidget(m_out, 1);
    m_in = new QLineEdit(this);
    m_in->setFont(mono);
    m_in->setPlaceholderText(tr("Grace command or expression, e.g.  s0.y = s0.y * 2   |   autoscale   |   fit formula \"y = a0*exp(-a1*x)\""));
    m_in->setClearButtonEnabled(true);
    m_in->installEventFilter(this);
    lay->addWidget(m_in);
    connect(m_in, &QLineEdit::returnPressed, this, [this] {
        const QString cmd = m_in->text().trimmed();
        if (cmd.isEmpty()) return;
        m_history << cmd;
        m_histPos = m_history.size();
        m_in->clear();
        runCommand(cmd);
    });
    appendMessage(tr("CactusPlot command console. Accepts the Grace command language (the same as .agr/.par/batch files) "
                     "and expressions. Up/Down browse the history."));
    m_fit.formula = QStringLiteral("y = a0 + a1*x");
}

void Console::appendMessage(const QString& text, bool error)
{
    const QString esc = text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    m_out->appendHtml(error ? QStringLiteral("<span style='color:#d9534f'>%1</span>").arg(esc) : esc);
    m_out->verticalScrollBar()->setValue(m_out->verticalScrollBar()->maximum());
}

void Console::runCommand(const QString& cmd)
{
    m_out->appendHtml(QStringLiteral("<b>&gt; %1</b>").arg(cmd.toHtmlEscaped()));
    QStringList msgs, errs, warns;
    bool ok = true;
    m_doc->modify(tr("Command: %1").arg(cmd.left(40)), [&](Project& p) {
        Interpreter in(&p);
        in.variables() = m_vars;
        in.fitOptions() = m_fit;
        in.setCurrentGraph(std::max(0, m_doc->currentGraph()));
        in.safeMode = false;
        in.messageHandler = [&](const QString& m) { if (!m.startsWith(QLatin1String("Error:"))) msgs << m; };
        ok = in.execute(cmd);
        errs = in.errors();
        warns = in.warnings();
        m_vars = in.variables();
        m_fit = in.fitOptions();
    });
    for (const QString& m : msgs) appendMessage(m);
    for (const QString& w : warns) appendMessage(w, true);
    for (const QString& e : errs) appendMessage(e, true);
    if (!ok && errs.isEmpty() && warns.isEmpty()) appendMessage(tr("Command not understood."), true);
}

bool Console::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_in && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        if (k->key() == Qt::Key_Up && !m_history.isEmpty()) {
            m_histPos = std::max(0, m_histPos - 1);
            m_in->setText(m_history.value(m_histPos));
            return true;
        }
        if (k->key() == Qt::Key_Down && !m_history.isEmpty()) {
            m_histPos = std::min(int(m_history.size()), m_histPos + 1);
            m_in->setText(m_history.value(m_histPos));
            return true;
        }
    }
    return QWidget::eventFilter(o, e);
}

// ====================================================================== data sheet

SetTableModel::SetTableModel(Document* doc, QObject* parent) : QAbstractTableModel(parent), m_doc(doc) {}

const DataSet* SetTableModel::ds() const
{
    const Graph* g = m_doc->project().graph(m_graph);
    return g ? g->set(m_set) : nullptr;
}

void SetTableModel::setTarget(int graph, int set)
{
    beginResetModel();
    m_graph = graph;
    m_set = set;
    endResetModel();
}

void SetTableModel::reload()
{
    beginResetModel();
    endResetModel();
}

int SetTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    const DataSet* s = ds();
    return s ? s->size() : 0;
}

int SetTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    const DataSet* s = ds();
    return s ? s->columns() + 1 : 0;
}

QVariant SetTableModel::data(const QModelIndex& index, int role) const
{
    const DataSet* s = ds();
    if (!s || !index.isValid()) return QVariant();
    const int c = index.column(), r = index.row();
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (c == s->columns()) return s->strings.value(r);
        const double v = s->cols[c].value(r);
        return role == Qt::EditRole ? QVariant(QString::number(v, 'g', 17)) : QVariant(QString::number(v, 'g', 10));
    }
    if (role == Qt::TextAlignmentRole) return int((c == s->columns() ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
    return QVariant();
}

bool SetTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !ds()) return false;
    const int c = index.column(), r = index.row();
    const int g = m_graph, si = m_set;
    if (c == ds()->columns()) {
        const QString t = value.toString();
        m_doc->modify(tr("Edit label"), [=](Project& p) {
            if (Graph* gr = p.graph(g)) if (DataSet* s = gr->set(si)) {
                while (s->strings.size() < s->size()) s->strings << QString();
                if (r < s->strings.size()) s->strings[r] = t;
            }
        });
        return true;
    }
    QVector<double> vals;
    QString label;
    if (!datareader::parseLine(value.toString(), &vals, &label, m_doc->project().referenceDate) || vals.isEmpty()) {
        // allow expressions like "pi/2"
        expr::Value v;
        expr::Context ctx;
        if (!expr::evaluate(value.toString(), ctx, &v)) return false;
        vals = {v.scalar()};
    }
    const double v = vals[0];
    m_doc->modify(tr("Edit value"), [=](Project& p) {
        if (Graph* gr = p.graph(g)) if (DataSet* s = gr->set(si)) if (r < s->size()) s->cols[c][r] = v;
    });
    emit dataChanged(index, index);
    return true;
}

QVariant SetTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (o == Qt::Vertical) return section;
    const DataSet* s = ds();
    if (!s) return QVariant();
    if (section == s->columns()) return tr("Label");
    return setTypeColumnNames(s->type).value(section);
}

Qt::ItemFlags SetTableModel::flags(const QModelIndex& index) const
{
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

DataSheet::DataSheet(Document* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);
    auto* top = new QHBoxLayout;
    m_setCombo = new QComboBox(this);
    m_setCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    top->addWidget(new QLabel(tr("Set:"), this));
    top->addWidget(m_setCombo, 1);
    auto tb = [this, top](const QString& icon, const QString& tip, auto fn) {
        auto* b = new QToolButton(this);
        b->setIcon(icons::get(icon));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        connect(b, &QToolButton::clicked, this, fn);
        top->addWidget(b);
    };
    tb(QStringLiteral("plus"), tr("Add rows"), [this] { addRows(); });
    tb(QStringLiteral("trash"), tr("Delete selected rows"), [this] { deleteRows(); });
    tb(QStringLiteral("copy"), tr("Copy selection"), [this] { copySelection(); });
    tb(QStringLiteral("clipboard"), tr("Paste (replaces the data from the selected row on)"), [this] { pasteData(); });
    lay->addLayout(top);
    m_model = new SetTableModel(doc, this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setAlternatingRowColors(true);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_view->verticalHeader()->setDefaultSectionSize(22);
    m_view->installEventFilter(this);
    lay->addWidget(m_view, 1);
    m_stats = new QLabel(this);
    m_stats->setWordWrap(true);
    lay->addWidget(m_stats);
    connect(m_setCombo, &QComboBox::activated, this, [this](int i) {
        const QPoint gs = m_setCombo->itemData(i).toPoint();
        m_model->setTarget(gs.x(), gs.y());
        m_doc->select(Selection{Selection::Set, gs.x(), gs.y()});
    });
    connect(doc, &Document::structureChanged, this, &DataSheet::fillCombo);
    connect(doc, &Document::changed, this, [this] {
        m_model->reload();
        const Graph* g = m_doc->project().graph(m_model->graph());
        const DataSet* s = g ? g->set(m_model->set()) : nullptr;
        if (!s) { m_stats->clear(); return; }
        const auto sx = transforms::statistics(s->cols[0]), sy = transforms::statistics(s->cols[1]);
        m_stats->setText(tr("%1 points.  X: %2 … %3,  mean %4.   Y: %5 … %6,  mean %7,  sd %8")
                             .arg(s->size()).arg(sx.min, 0, 'g', 6).arg(sx.max, 0, 'g', 6).arg(sx.mean, 0, 'g', 6)
                             .arg(sy.min, 0, 'g', 6).arg(sy.max, 0, 'g', 6).arg(sy.mean, 0, 'g', 6).arg(sy.sd, 0, 'g', 6));
    });
    connect(doc, &Document::selectionChanged, this, [this] {
        const Selection& s = m_doc->selection();
        if (s.kind == Selection::Set) showSet(s.graph, s.index);
    });
    fillCombo();
}

void DataSheet::fillCombo()
{
    m_setCombo->clear();
    const Project& p = m_doc->project();
    for (int g = 0; g < p.graphs.size(); ++g) {
        if (!p.graphs[g].active) continue;
        for (int s = 0; s < p.graphs[g].sets.size(); ++s) {
            const DataSet& d = p.graphs[g].sets[s];
            if (!d.active) continue;
            QString n = QStringLiteral("G%1.S%2").arg(g).arg(s);
            const QString leg = richtext::plainText(d.legend);
            if (!leg.isEmpty()) n += QStringLiteral("  ") + leg;
            m_setCombo->addItem(icons::swatch(d.line.color, 12), n, QPoint(g, s));
        }
    }
    const int i = m_setCombo->findData(QPoint(m_model->graph(), m_model->set()));
    if (i >= 0) m_setCombo->setCurrentIndex(i);
    else if (m_setCombo->count() > 0) {
        const QPoint gs = m_setCombo->itemData(0).toPoint();
        m_model->setTarget(gs.x(), gs.y());
    } else {
        m_model->setTarget(-1, -1);
    }
}

void DataSheet::showSet(int graph, int set)
{
    m_model->setTarget(graph, set);
    const int i = m_setCombo->findData(QPoint(graph, set));
    if (i >= 0) m_setCombo->setCurrentIndex(i);
    emit m_doc->changed();
}

void DataSheet::addRows()
{
    const int g = m_model->graph(), s = m_model->set();
    m_doc->modify(tr("Add rows"), [=](Project& p) {
        if (Graph* gr = p.graph(g)) if (DataSet* d = gr->set(s)) {
            const int n = d->size();
            d->resize(n + 10);
            const double step = n >= 2 ? d->cols[0][n - 1] - d->cols[0][n - 2] : 1.0;
            for (int i = n; i < n + 10; ++i) d->cols[0][i] = (n > 0 ? d->cols[0][n - 1] : 0.0) + step * (i - n + 1);
        }
    });
}

void DataSheet::deleteRows()
{
    QList<int> rows;
    for (const QModelIndex& i : m_view->selectionModel()->selectedIndexes()) if (!rows.contains(i.row())) rows << i.row();
    if (rows.isEmpty()) return;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    const int g = m_model->graph(), s = m_model->set();
    m_doc->modify(tr("Delete rows"), [=](Project& p) {
        if (Graph* gr = p.graph(g)) if (DataSet* d = gr->set(s))
            for (int r : rows) {
                for (int c = 0; c < d->columns(); ++c) if (r < d->cols[c].size()) d->cols[c].remove(r);
                if (r < d->strings.size()) d->strings.removeAt(r);
            }
    });
}

void DataSheet::copySelection()
{
    const QModelIndexList sel = m_view->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) return;
    int r0 = INT_MAX, r1 = -1, c0 = INT_MAX, c1 = -1;
    for (const QModelIndex& i : sel) {
        r0 = std::min(r0, i.row()); r1 = std::max(r1, i.row());
        c0 = std::min(c0, i.column()); c1 = std::max(c1, i.column());
    }
    QString out;
    for (int r = r0; r <= r1; ++r) {
        QStringList row;
        for (int c = c0; c <= c1; ++c) row << m_model->data(m_model->index(r, c), Qt::EditRole).toString();
        out += row.join(QLatin1Char('\t')) + QLatin1Char('\n');
    }
    QApplication::clipboard()->setText(out);
}

void DataSheet::pasteData()
{
    const QString text = QApplication::clipboard()->text();
    const auto tables = datareader::parseText(text, nullptr, m_doc->project().referenceDate);
    if (tables.isEmpty()) return;
    const DataTable t = tables[0];
    int row = 0, col = 0;
    const QModelIndex cur = m_view->currentIndex();
    if (cur.isValid()) { row = cur.row(); col = cur.column(); }
    const int g = m_model->graph(), s = m_model->set();
    m_doc->modify(tr("Paste data"), [=](Project& p) {
        if (Graph* gr = p.graph(g)) if (DataSet* d = gr->set(s)) {
            if (row + t.rows() > d->size()) d->resize(row + t.rows());
            for (int c = 0; c < t.columns() && col + c < d->columns(); ++c)
                for (int r = 0; r < t.rows(); ++r) d->cols[col + c][row + r] = t.cols[c][r];
        }
    });
}

bool DataSheet::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_view && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        if (k->matches(QKeySequence::Copy)) { copySelection(); return true; }
        if (k->matches(QKeySequence::Paste)) { pasteData(); return true; }
        if (k->key() == Qt::Key_Delete && (k->modifiers() & Qt::ShiftModifier)) { deleteRows(); return true; }
    }
    return QWidget::eventFilter(o, e);
}

}  // namespace cactus::ui
