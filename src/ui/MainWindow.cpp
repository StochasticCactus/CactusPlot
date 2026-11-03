// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

#include "Version.h"
#include "core/Presets.h"
#include "core/color/Colors.h"
#include "grace/Interpreter.h"
#include "grace/ProjectIO.h"
#include "io/DataReader.h"
#include "render/Export.h"
#include "ui/Dialogs.h"
#include "ui/Document.h"
#include "ui/Inspector.h"
#include "ui/Panels.h"
#include "ui/Theme.h"

namespace cactus::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowIcon(icons::appIcon());
    setAcceptDrops(true);
    m_doc = new Document(this);
    QSettings st;
    m_doc->setProject(presets::newProject(st.value(QStringLiteral("new/preset"), presets::defaultName()).toString()));

    m_view = new PlotView(m_doc, this);
    setCentralWidget(m_view);
    buildDocks();
    buildActions();
    buildMenus();
    buildToolbars();

    // Grace-style locator: coordinates under the cursor, shown in the menu bar
    auto* corner = new QWidget(this);
    auto* cl = new QHBoxLayout(corner);
    cl->setContentsMargins(0, 0, 10, 0);
    cl->setSpacing(14);
    m_locator = new QLabel(corner);
    m_locator->setMinimumWidth(300);
    m_locator->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_locator->setForegroundRole(QPalette::PlaceholderText);
    m_zoomLabel = new QLabel(QStringLiteral("100%"), corner);
    m_zoomLabel->setForegroundRole(QPalette::PlaceholderText);
    cl->addWidget(m_locator);
    cl->addWidget(m_zoomLabel);
    menuBar()->setCornerWidget(corner, Qt::TopRightCorner);
    statusBar()->hide();
    m_flashTimer = new QTimer(this);
    m_flashTimer->setSingleShot(true);
    connect(m_view, &PlotView::locatorChanged, this, [this](const QString& t) {
        if (!m_flashTimer->isActive()) m_locator->setText(t);
    });
    connect(m_view, &PlotView::zoomChanged, this, [this](double z) { m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(z * 100))); });
    connect(m_view, &PlotView::propertiesRequested, this, [this] { showAppearance(); });
    connect(m_view, &PlotView::contextMenuRequested, this, &MainWindow::showContextMenu);
    connect(m_view, &PlotView::filesDropped, this, [this](const QStringList& files) { for (const QString& f : files) openFile(f); });
    connect(m_view, &PlotView::toolChanged, this, [this](PlotView::Tool t) {
        for (QAction* a : m_toolGroup->actions())
            if (a->data().toInt() == int(t)) a->setChecked(true);
    });
    connect(m_doc, &Document::filePathChanged, this, &MainWindow::updateTitle);
    connect(m_doc, &Document::modifiedChanged, this, &MainWindow::updateTitle);

    m_autosaveTimer = new QTimer(this);
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::autosave);
    const int minutes = st.value(QStringLiteral("autosave/minutes"), 3).toInt();
    if (minutes > 0) m_autosaveTimer->start(minutes * 60 * 1000);

    resize(1440, 900);
    restoreGeometry(st.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(st.value(QStringLiteral("window/state2")).toByteArray());
    updateTitle();
    updateRecentMenu();
}

// ---------------------------------------------------------------- docks

QDialog* MainWindow::makeToolWindow(const QString& title, QWidget* content, const QString& name, const QSize& size)
{
    auto* d = new QDialog(this, Qt::Tool);
    d->setObjectName(name);
    d->setWindowTitle(title);
    auto* l = new QVBoxLayout(d);
    l->setContentsMargins(0, 0, 0, 0);
    content->setParent(d);
    l->addWidget(content);
    d->resize(size);
    const QByteArray geo = QSettings().value(QStringLiteral("windows/") + name).toByteArray();
    if (!geo.isEmpty()) d->restoreGeometry(geo);
    return d;
}

void MainWindow::buildDocks()
{
    // Like Grace, the main window only shows the plot. Appearance, data editing, the command window and
    // the object explorer open as separate windows from the menus or by double-clicking.
    m_tree = new ProjectTree(m_doc, this);
    m_treeDock = makeToolWindow(tr("Explorer"), m_tree, QStringLiteral("Explorer"), QSize(300, 460));
    m_inspector = new Inspector(m_doc, this);
    m_inspectorDock = makeToolWindow(tr("Appearance"), m_inspector, QStringLiteral("Appearance"), QSize(400, 640));
    m_sheet = new DataSheet(m_doc, this);
    m_sheetDock = makeToolWindow(tr("Set Editor"), m_sheet, QStringLiteral("SetEditor"), QSize(560, 480));
    m_console = new Console(m_doc, this);
    m_consoleDock = makeToolWindow(tr("Commands"), m_console, QStringLiteral("Commands"), QSize(640, 340));

    connect(m_inspector, &Inspector::editDataRequested, this, [this](int g, int s) {
        m_sheet->showSet(g, s);
        showWindow(m_sheetDock);
    });
    connect(m_inspector, &Inspector::applyPresetRequested, this, &MainWindow::applyPreset);
    connect(m_tree, &ProjectTree::editDataRequested, this, [this](int g, int s) {
        m_sheet->showSet(g, s);
        showWindow(m_sheetDock);
    });
    connect(m_tree, &ProjectTree::propertiesRequested, this, [this] { showAppearance(); });
}

void MainWindow::showWindow(QDialog* d)
{
    if (!d->isVisible()) {
        // first opening: place next to the main window
        if (QSettings().value(QStringLiteral("windows/") + d->objectName()).toByteArray().isEmpty() && !d->property("placed").toBool()) {
            const QRect g = frameGeometry();
            d->move(g.right() - d->width() - 40, g.top() + 90);
            d->setProperty("placed", true);
        }
        d->show();
    }
    d->raise();
    d->activateWindow();
}

void MainWindow::showAppearance(int kind)
{
    // Grace-style entry points: Plot/Graph/Set appearance and Axis properties
    const int g = std::max(0, m_doc->currentGraph());
    const Selection cur = m_doc->selection();
    switch (kind) {
    case Selection::Page: m_doc->select(Selection{Selection::Page, -1, -1}); break;
    case Selection::Graph: m_doc->select(Selection{Selection::Graph, g, -1}); break;
    case Selection::Axis:
        if (cur.kind != Selection::Axis) m_doc->select(Selection{Selection::Axis, g, 0});
        break;
    case Selection::Set:
        if (cur.kind != Selection::Set) {
            const Graph* gr = m_doc->project().graph(g);
            int si = -1;
            if (gr) for (int i = 0; i < gr->sets.size(); ++i) if (gr->sets[i].active) { si = i; break; }
            if (si >= 0) m_doc->select(Selection{Selection::Set, g, si});
        }
        break;
    default:
        if (cur.kind == Selection::None) m_doc->select(Selection{Selection::Graph, g, -1});
        break;
    }
    showWindow(m_inspectorDock);
}

QWidget* MainWindow::openWindow(const QString& which)
{
    if (which == QLatin1String("appearance")) { showAppearance(Selection::Set); return m_inspectorDock; }
    if (which == QLatin1String("seteditor")) { showWindow(m_sheetDock); return m_sheetDock; }
    if (which == QLatin1String("commands")) { showWindow(m_consoleDock); return m_consoleDock; }
    if (which == QLatin1String("explorer")) { showWindow(m_treeDock); return m_treeDock; }
    return this;
}

void MainWindow::flash(const QString& msg, int ms)
{
    m_locator->setText(msg);
    m_flashTimer->start(ms);
}

// ---------------------------------------------------------------- actions

void MainWindow::buildActions()
{
    auto act = [this](const QString& id, const QString& text, const QString& icon, const QKeySequence& key, auto fn) {
        auto* a = new QAction(icon.isEmpty() ? QIcon() : icons::get(icon), text, this);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, fn);
        m_actions.insert(id, a);
        return a;
    };
    act(QStringLiteral("new"), tr("&New"), QStringLiteral("new"), QKeySequence::New,
        [this] { newProject(QSettings().value(QStringLiteral("new/preset"), presets::defaultName()).toString()); });
    act(QStringLiteral("open"), tr("&Open..."), QStringLiteral("open"), QKeySequence::Open, [this] { openDialog(); });
    act(QStringLiteral("save"), tr("&Save"), QStringLiteral("save"), QKeySequence::Save, [this] { save(); });
    act(QStringLiteral("saveas"), tr("Save &As..."), QString(), QKeySequence::SaveAs, [this] { saveAs(); });
    act(QStringLiteral("import"), tr("&Import Data..."), QStringLiteral("import"), QKeySequence(Qt::CTRL | Qt::Key_I), [this] { importDialog(); });
    act(QStringLiteral("importclip"), tr("Import from &Clipboard"), QStringLiteral("clipboard"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V),
        [this] { importClipboard(); });
    act(QStringLiteral("export"), tr("&Export..."), QStringLiteral("export"), QKeySequence(Qt::CTRL | Qt::Key_E), [this] { exportDialog(); });
    act(QStringLiteral("reexport"), tr("Export &Again"), QString(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), [this] {
        if (ExportDialog::exportWithLastSettings(m_doc, this)) flash(tr("Exported to %1").arg(QSettings().value(QStringLiteral("export/last")).toString()), 4000);
        else exportDialog();
    });
    act(QStringLiteral("print"), tr("&Print..."), QStringLiteral("print"), QKeySequence::Print, [this] { printPlot(); });
    act(QStringLiteral("quit"), tr("&Quit"), QString(), QKeySequence::Quit, [this] { close(); });
    m_actions.value(QStringLiteral("quit"))->setMenuRole(QAction::QuitRole);

    QAction* undo = m_doc->undoStack()->createUndoAction(this, tr("&Undo"));
    undo->setIcon(icons::get(QStringLiteral("undo")));
    undo->setShortcut(QKeySequence::Undo);
    m_actions.insert(QStringLiteral("undo"), undo);
    QAction* redo = m_doc->undoStack()->createRedoAction(this, tr("&Redo"));
    redo->setIcon(icons::get(QStringLiteral("redo")));
    redo->setShortcut(QKeySequence::Redo);
    m_actions.insert(QStringLiteral("redo"), redo);
    act(QStringLiteral("copyimg"), tr("Copy Plot as &Image"), QStringLiteral("image"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), [this] { copyImage(false); });
    act(QStringLiteral("copysvg"), tr("Copy Plot as &SVG"), QStringLiteral("copy"), QKeySequence(), [this] { copyImage(true); });
    act(QStringLiteral("prefs"), tr("&Preferences..."), QStringLiteral("settings"), QKeySequence::Preferences, [this] {
        PreferencesDialog d(this);
        connect(&d, &PreferencesDialog::themeChanged, this, &MainWindow::applyTheme);
        d.exec();
    });
    m_actions.value(QStringLiteral("prefs"))->setMenuRole(QAction::PreferencesRole);
    // some platforms have no standard key for these; use the common ones there
    if (QKeySequence::keyBindings(QKeySequence::SaveAs).isEmpty())
        m_actions.value(QStringLiteral("saveas"))->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    if (QKeySequence::keyBindings(QKeySequence::Preferences).isEmpty())
        m_actions.value(QStringLiteral("prefs"))->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));

    act(QStringLiteral("sheet"), tr("Edit &Set (Spreadsheet)..."), QStringLiteral("table"), QKeySequence(Qt::CTRL | Qt::Key_D), [this] { showWindow(m_sheetDock); });
    act(QStringLiteral("transform"), tr("&Transformations..."), QStringLiteral("transform"), QKeySequence(Qt::CTRL | Qt::Key_T), [this] {
        auto* d = new TransformDialog(m_doc, this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });
    act(QStringLiteral("evaluate"), tr("&Evaluate Expression / New Set..."), QStringLiteral("function"), QKeySequence(Qt::CTRL | Qt::Key_R), [this] {
        auto* d = new TransformDialog(m_doc, this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->showPage(tr("Evaluate expression"));
        d->show();
    });
    act(QStringLiteral("fit"), tr("Curve &Fitting..."), QStringLiteral("fit"), QKeySequence(Qt::CTRL | Qt::Key_F), [this] {
        auto* d = new FitDialog(m_doc, this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->show();
    });
    act(QStringLiteral("stats"), tr("&Statistics..."), QStringLiteral("stats"), QKeySequence(), [this] {
        auto* d = new TransformDialog(m_doc, this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->showPage(tr("Statistics"));
        d->show();
    });
    act(QStringLiteral("appearance"), tr("&Appearance..."), QStringLiteral("sliders"), QKeySequence(Qt::CTRL | Qt::Key_P | Qt::SHIFT), [this] { showAppearance(); });
    act(QStringLiteral("plotapp"), tr("&Plot Appearance..."), QStringLiteral("page"), QKeySequence(), [this] { showAppearance(Selection::Page); });
    act(QStringLiteral("graphapp"), tr("&Graph Appearance..."), QStringLiteral("graph"), QKeySequence(), [this] { showAppearance(Selection::Graph); });
    act(QStringLiteral("setapp"), tr("&Set Appearance..."), QStringLiteral("set"), QKeySequence(), [this] { showAppearance(Selection::Set); });
    act(QStringLiteral("axisapp"), tr("A&xis Properties..."), QStringLiteral("axis"), QKeySequence(), [this] { showAppearance(Selection::Axis); });
    act(QStringLiteral("explorer"), tr("&Explorer"), QStringLiteral("layers"), QKeySequence(), [this] { showWindow(m_treeDock); });
    act(QStringLiteral("console"), tr("&Commands..."), QStringLiteral("console"), QKeySequence(Qt::CTRL | Qt::Key_K), [this] { showWindow(m_consoleDock); });
    act(QStringLiteral("clearregions"), tr("Clear &Regions"), QString(), QKeySequence(), [this] {
        m_doc->modify(tr("Clear regions"), [](Project& p) { for (Region& r : p.regions) r.active = false; });
    });

    act(QStringLiteral("autoscale"), tr("&Autoscale"), QStringLiteral("autoscale"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), [this] {
        const int g = m_doc->currentGraph();
        m_doc->modify(tr("Autoscale"), [g](Project& p) { if (Graph* gr = p.graph(g)) gr->autoscale(); });
    });
    act(QStringLiteral("autoscalex"), tr("Autoscale &X"), QString(), QKeySequence(), [this] {
        const int g = m_doc->currentGraph();
        m_doc->modify(tr("Autoscale X"), [g](Project& p) { if (Graph* gr = p.graph(g)) gr->autoscale(1); });
    });
    act(QStringLiteral("autoscaley"), tr("Autoscale &Y"), QString(), QKeySequence(), [this] {
        const int g = m_doc->currentGraph();
        m_doc->modify(tr("Autoscale Y"), [g](Project& p) { if (Graph* gr = p.graph(g)) gr->autoscale(2); });
    });
    act(QStringLiteral("autoscaleall"), tr("Autoscale All Graphs"), QString(), QKeySequence(), [this] {
        m_doc->modify(tr("Autoscale all"), [](Project& p) { for (Graph& g : p.graphs) if (g.active) g.autoscale(); });
    });
    act(QStringLiteral("addgraph"), tr("Add &Graph"), QStringLiteral("graph-add"), QKeySequence(), [this] { addGraph(false); });
    act(QStringLiteral("addinset"), tr("Add &Inset Graph"), QString(), QKeySequence(), [this] { addGraph(true); });
    act(QStringLiteral("arrange"), tr("A&rrange Graphs..."), QStringLiteral("grid"), QKeySequence(), [this] { ArrangeDialog(m_doc, this).exec(); });
    act(QStringLiteral("grid"), tr("Toggle &Grid Lines"), QStringLiteral("grid"), QKeySequence(Qt::CTRL | Qt::Key_G), [this] {
        const int g = m_doc->currentGraph();
        m_doc->modify(tr("Toggle grid"), [g](Project& p) {
            Graph* gr = p.graph(g);
            if (!gr) return;
            const bool on = !gr->axes[0].majorProps.grid;
            for (int a = 0; a < 2; ++a) gr->axes[a].majorProps.grid = on;
        });
    });
    act(QStringLiteral("legend"), tr("Toggle &Legend"), QStringLiteral("legend"), QKeySequence(Qt::CTRL | Qt::Key_L), [this] {
        const int g = m_doc->currentGraph();
        m_doc->modify(tr("Toggle legend"), [g](Project& p) { if (Graph* gr = p.graph(g)) gr->legend.active = !gr->legend.active; });
    });

    act(QStringLiteral("zoomin"), tr("Zoom &In"), QStringLiteral("zoom-in"), QKeySequence::ZoomIn, [this] { m_view->zoomIn(); });
    act(QStringLiteral("zoomout"), tr("Zoom &Out"), QStringLiteral("zoom-out"), QKeySequence::ZoomOut, [this] { m_view->zoomOut(); });
    act(QStringLiteral("fitpage"), tr("&Fit Page"), QStringLiteral("zoom-fit"), QKeySequence(Qt::CTRL | Qt::Key_0), [this] { m_view->fitPage(); });
    auto* regions = act(QStringLiteral("showregions"), tr("Show &Regions"), QStringLiteral("region"), QKeySequence(), [this](bool on) { m_view->setShowRegions(on); });
    regions->setCheckable(true);

    act(QStringLiteral("help"), tr("CactusPlot &Documentation"), QStringLiteral("help"), QKeySequence::HelpContents,
        [] { QDesktopServices::openUrl(QUrl(QStringLiteral(CACTUS_HOMEPAGE "/blob/main/docs/index.md"))); });
    act(QStringLiteral("cmdref"), tr("&Command Reference"), QString(), QKeySequence(), [this] { showCommandReference(); });
    act(QStringLiteral("shortcuts"), tr("&Mouse and Keyboard"), QString(), QKeySequence(), [this] { showShortcuts(); });
    act(QStringLiteral("about"), tr("&About CactusPlot"), QString(), QKeySequence(), [this] { about(); });
    m_actions.value(QStringLiteral("about"))->setMenuRole(QAction::AboutRole);
    act(QStringLiteral("aboutqt"), tr("About &Qt"), QString(), QKeySequence(), [] { QApplication::aboutQt(); });

    // tools
    m_toolGroup = new QActionGroup(this);
    struct ToolDef { PlotView::Tool tool; const char* icon; const char* text; const char* key; };
    static const ToolDef tools[] = {
        {PlotView::Tool::Select, "select", "Select, move and pan (wheel zooms)", "V"},
        {PlotView::Tool::Pan, "pan", "Pan", "H"},
        {PlotView::Tool::ZoomBox, "zoom-box", "Zoom to rectangle", "Z"},
        {PlotView::Tool::ZoomX, "zoom-x", "Zoom X only", "Shift+X"},
        {PlotView::Tool::ZoomY, "zoom-y", "Zoom Y only", "Shift+Y"},
        {PlotView::Tool::Crosshair, "crosshair", "Crosshair / read values", "C"},
        {PlotView::Tool::EditPoints, "edit-point", "Edit points (drag; Ctrl+click deletes, Shift+click adds)", "P"},
        {PlotView::Tool::Text, "text", "Add text", "T"},
        {PlotView::Tool::Line, "line", "Draw line", "L"},
        {PlotView::Tool::Arrow, "arrow", "Draw arrow", "A"},
        {PlotView::Tool::Box, "box", "Draw box", "B"},
        {PlotView::Tool::Ellipse, "ellipse", "Draw ellipse", "E"},
        {PlotView::Tool::Region, "region", "Define region (click vertices, double-click to finish)", "R"},
    };
    for (const ToolDef& t : tools) {
        auto* a = new QAction(icons::get(QString::fromLatin1(t.icon)), tr(t.text), this);
        a->setCheckable(true);
        a->setData(int(t.tool));
        a->setShortcut(QKeySequence(QString::fromLatin1(t.key)));
        a->setToolTip(QStringLiteral("%1  (%2)").arg(tr(t.text), QString::fromLatin1(t.key)));
        m_toolGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, t] { m_view->setTool(t.tool); });
        if (t.tool == PlotView::Tool::Select) a->setChecked(true);
    }
}

void MainWindow::buildMenus()
{
    auto* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_actions.value(QStringLiteral("new")));
    auto* newFrom = file->addMenu(tr("New with &Style"));
    for (const QString& p : presets::names()) {
        QAction* a = newFrom->addAction(p, this, [this, p] { newProject(p); });
        a->setToolTip(presets::description(p));
    }
    file->addAction(m_actions.value(QStringLiteral("open")));
    m_recentMenu = file->addMenu(tr("Open &Recent"));
    file->addSeparator();
    file->addAction(m_actions.value(QStringLiteral("save")));
    file->addAction(m_actions.value(QStringLiteral("saveas")));
    file->addSeparator();
    file->addAction(m_actions.value(QStringLiteral("import")));
    file->addAction(m_actions.value(QStringLiteral("importclip")));
    file->addSeparator();
    file->addAction(m_actions.value(QStringLiteral("export")));
    file->addAction(m_actions.value(QStringLiteral("reexport")));
    file->addAction(m_actions.value(QStringLiteral("print")));
    file->addSeparator();
    file->addAction(m_actions.value(QStringLiteral("quit")));

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_actions.value(QStringLiteral("undo")));
    edit->addAction(m_actions.value(QStringLiteral("redo")));
    edit->addSeparator();
    edit->addAction(m_actions.value(QStringLiteral("copyimg")));
    edit->addAction(m_actions.value(QStringLiteral("copysvg")));
    edit->addAction(m_actions.value(QStringLiteral("importclip")));
    edit->addSeparator();
    edit->addAction(m_actions.value(QStringLiteral("prefs")));

    auto* data = menuBar()->addMenu(tr("&Data"));
    data->addAction(m_actions.value(QStringLiteral("import")));
    data->addAction(m_actions.value(QStringLiteral("sheet")));
    data->addAction(m_actions.value(QStringLiteral("evaluate")));
    data->addSeparator();
    data->addAction(m_actions.value(QStringLiteral("transform")));
    data->addAction(m_actions.value(QStringLiteral("fit")));
    data->addAction(m_actions.value(QStringLiteral("stats")));
    data->addSeparator();
    auto* reg = data->addMenu(icons::get(QStringLiteral("region")), tr("&Regions"));
    reg->addAction(tr("Draw Polygon Region"), this, [this] { m_view->setTool(PlotView::Tool::Region); });
    reg->addAction(m_actions.value(QStringLiteral("showregions")));
    reg->addAction(m_actions.value(QStringLiteral("clearregions")));
    data->addAction(m_actions.value(QStringLiteral("console")));

    auto* plot = menuBar()->addMenu(tr("&Plot"));
    plot->addAction(m_actions.value(QStringLiteral("plotapp")));
    plot->addAction(m_actions.value(QStringLiteral("graphapp")));
    plot->addAction(m_actions.value(QStringLiteral("setapp")));
    plot->addAction(m_actions.value(QStringLiteral("axisapp")));
    plot->addSeparator();
    plot->addAction(m_actions.value(QStringLiteral("autoscale")));
    plot->addAction(m_actions.value(QStringLiteral("autoscalex")));
    plot->addAction(m_actions.value(QStringLiteral("autoscaley")));
    plot->addAction(m_actions.value(QStringLiteral("autoscaleall")));
    plot->addSeparator();
    plot->addAction(m_actions.value(QStringLiteral("addgraph")));
    plot->addAction(m_actions.value(QStringLiteral("addinset")));
    plot->addAction(m_actions.value(QStringLiteral("arrange")));
    plot->addSeparator();
    auto* type = plot->addMenu(tr("Graph &Type"));
    const QStringList types{tr("XY"), tr("Chart"), tr("Polar"), tr("Smith"), tr("Fixed aspect"), tr("Pie")};
    for (int i = 0; i < types.size(); ++i)
        type->addAction(types[i], this, [this, i] {
            const int g = m_doc->currentGraph();
            m_doc->modify(tr("Graph type"), [g, i](Project& p) { if (Graph* gr = p.graph(g)) { gr->type = GraphType(i); gr->autoscale(); } });
        });
    const QStringList scales{tr("Linear"), tr("Logarithmic"), tr("Reciprocal"), tr("Logit")};
    for (int axis = 0; axis < 2; ++axis) {
        auto* sm = plot->addMenu(axis == 0 ? tr("&X Scale") : tr("&Y Scale"));
        for (int i = 0; i < scales.size(); ++i)
            sm->addAction(scales[i], this, [this, i, axis] {
                const int g = m_doc->currentGraph();
                m_doc->modify(tr("Axis scale"), [g, i, axis](Project& p) {
                    if (Graph* gr = p.graph(g)) {
                        (axis == 0 ? gr->xscale : gr->yscale) = ScaleType(i);
                        gr->autoscale(axis == 0 ? 1 : 2);
                    }
                });
            });
    }
    plot->addAction(m_actions.value(QStringLiteral("grid")));
    plot->addAction(m_actions.value(QStringLiteral("legend")));
    plot->addSeparator();
    auto* style = plot->addMenu(icons::get(QStringLiteral("palette")), tr("Style &Preset"));
    for (const QString& p : presets::names()) {
        QAction* a = style->addAction(p, this, [this, p] { applyPreset(p); });
        a->setToolTip(presets::description(p));
    }
    style->setToolTipsVisible(true);
    auto* pal = plot->addMenu(icons::get(QStringLiteral("palette")), tr("Colour &Palette"));
    QHash<QString, QMenu*> groups;
    for (const Palette& p : colors::palettes()) {
        QMenu* gm = groups.value(p.group);
        if (!gm) {
            gm = pal->addMenu(p.group);
            groups.insert(p.group, gm);
        }
        QPixmap pm(160, 24);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        const int n = std::min(10, int(p.colors.size()));
        for (int i = 0; i < n; ++i) pp.fillRect(QRectF(i * 8.0, 2, 8, 8), p.colors[i]);
        pp.end();
        const QString name = p.name;
        gm->addAction(QIcon(pm), name, this, [this, name] {
            m_doc->modify(tr("Apply palette %1").arg(name), [name](Project& pr) { presets::applyPalette(pr, name); });
        });
    }

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_actions.value(QStringLiteral("zoomin")));
    m_viewMenu->addAction(m_actions.value(QStringLiteral("zoomout")));
    m_viewMenu->addAction(m_actions.value(QStringLiteral("fitpage")));
    m_viewMenu->addAction(m_actions.value(QStringLiteral("showregions")));
    m_viewMenu->addSeparator();
    auto* tools = m_viewMenu->addMenu(tr("&Tools"));
    tools->addActions(m_toolGroup->actions());
    m_viewMenu->addSeparator();
    auto* theme = m_viewMenu->addMenu(icons::get(QStringLiteral("theme")), tr("T&heme"));
    const QStringList themes{tr("Follow the System"), tr("Light"), tr("Dark")};
    auto* tg = new QActionGroup(this);
    const int curTheme = QSettings().value(QStringLiteral("ui/theme"), 0).toInt();
    for (int i = 0; i < themes.size(); ++i) {
        QAction* a = theme->addAction(themes[i], this, [this, i] {
            QSettings().setValue(QStringLiteral("ui/theme"), i);
            applyTheme();
        });
        a->setCheckable(true);
        a->setChecked(i == curTheme);
        tg->addAction(a);
    }

    auto* win = menuBar()->addMenu(tr("&Window"));
    win->addAction(m_actions.value(QStringLiteral("appearance")));
    win->addAction(m_actions.value(QStringLiteral("sheet")));
    win->addAction(m_actions.value(QStringLiteral("console")));
    win->addAction(m_actions.value(QStringLiteral("explorer")));

    auto* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_actions.value(QStringLiteral("help")));
    help->addAction(m_actions.value(QStringLiteral("cmdref")));
    help->addAction(m_actions.value(QStringLiteral("shortcuts")));
    help->addSeparator();
    help->addAction(m_actions.value(QStringLiteral("about")));
    help->addAction(m_actions.value(QStringLiteral("aboutqt")));
}

void MainWindow::buildToolbars()
{
    // One slim button column on the left, as in xmgrace: navigation first, then drawing tools.
    auto* bar = new QToolBar(tr("Buttons"), this);
    bar->setObjectName(QStringLiteral("ButtonColumn"));
    bar->setIconSize(QSize(18, 18));
    bar->setOrientation(Qt::Vertical);
    bar->setMovable(false);
    bar->setFloatable(false);
    const QList<QAction*> tools = m_toolGroup->actions();
    for (int i = 0; i < tools.size(); ++i) {
        if (i == 7) {  // before the drawing tools
            bar->addSeparator();
            for (const char* id : {"autoscale", "zoomin", "zoomout", "fitpage"}) bar->addAction(m_actions.value(QString::fromLatin1(id)));
            bar->addSeparator();
        }
        bar->addAction(tools[i]);
    }
    bar->addSeparator();
    for (const char* id : {"undo", "redo"}) bar->addAction(m_actions.value(QString::fromLatin1(id)));
    addToolBar(Qt::LeftToolBarArea, bar);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(bar->toggleViewAction());
}

// ---------------------------------------------------------------- files

void MainWindow::updateTitle()
{
    setWindowTitle(QStringLiteral("%1[*] — " CACTUS_APP_NAME).arg(m_doc->displayName()));
    setWindowModified(m_doc->isModified());
    setWindowFilePath(m_doc->filePath());
}

void MainWindow::updateRecentMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    const QStringList files = QSettings().value(QStringLiteral("recent/files")).toStringList();
    for (const QString& f : files) {
        if (!QFileInfo::exists(f)) continue;
        m_recentMenu->addAction(QFileInfo(f).fileName(), this, [this, f] { openFile(f); })->setToolTip(f);
    }
    if (m_recentMenu->isEmpty()) m_recentMenu->addAction(tr("(none)"))->setEnabled(false);
    else {
        m_recentMenu->addSeparator();
        m_recentMenu->addAction(tr("Clear List"), this, [this] {
            QSettings().remove(QStringLiteral("recent/files"));
            updateRecentMenu();
        });
    }
}

void MainWindow::addRecent(const QString& path)
{
    QSettings st;
    QStringList files = st.value(QStringLiteral("recent/files")).toStringList();
    const QString abs = QFileInfo(path).absoluteFilePath();
    files.removeAll(abs);
    files.prepend(abs);
    while (files.size() > st.value(QStringLiteral("recent/max"), 12).toInt()) files.removeLast();
    st.setValue(QStringLiteral("recent/files"), files);
    updateRecentMenu();
}

bool MainWindow::maybeSave()
{
    if (!m_doc->isModified()) return true;
    const auto r = QMessageBox::warning(this, tr("Unsaved changes"), tr("Save the changes to %1?").arg(m_doc->displayName()),
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Save) return save();
    return r == QMessageBox::Discard;
}

void MainWindow::setProject(const Project& p, const QString& path)
{
    m_doc->setProject(p, path);
    m_view->fitPage();
    updateTitle();
}

void MainWindow::newProject(const QString& preset)
{
    if (!maybeSave()) return;
    setProject(presets::newProject(preset), QString());
}

bool MainWindow::openFile(const QString& path)
{
    QString text, err;
    if (!datareader::readFile(path, &text, &err)) {
        QMessageBox::critical(this, tr("Open failed"), err);
        return false;
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool project = suffix == QLatin1String("cplot") || suffix == QLatin1String("agr") || suffix == QLatin1String("xvgr") ||
                         suffix == QLatin1String("par") || projectio::isProjectText(text);
    if (project) {
        if (!maybeSave()) return false;
        Project p;
        QStringList warnings;
        if (!projectio::loadText(text, &p, QFileInfo(path).fileName(), &err, &warnings)) {
            QMessageBox::critical(this, tr("Open failed"), err);
            return false;
        }
        // .agr files are opened as new documents to avoid overwriting them in the native format by accident
        const bool native = suffix == QLatin1String("cplot");
        setProject(p, native || suffix == QLatin1String("agr") ? path : QString());
        addRecent(path);
        if (!warnings.isEmpty())
            flash(tr("Opened with %n unsupported statement(s) ignored", nullptr, warnings.size()), 8000);
        else
            flash(tr("Opened %1").arg(QFileInfo(path).fileName()), 4000);
        return true;
    }
    // Data files that carry their own Grace commands (GROMACS .xvg output: titles, axis labels,
    // legends, "@TYPE xydy") are read directly, as xmgrace does, so error bars and labels come along.
    if (suffix == QLatin1String("xvg") || datareader::hasGraceCommands(text)) {
        const QString name = QFileInfo(path).fileName();
        QStringList errs;
        int gi = std::max(0, m_doc->currentGraph());
        if (datareader::parseText(text).isEmpty()) {
            QMessageBox::warning(this, tr("Open failed"), tr("%1 contains no numeric data.").arg(name));
            return false;
        }
        m_doc->modify(tr("Open %1").arg(name), [&](Project& p) {
            if (!p.graph(gi) || !p.graph(gi)->active) {
                Graph& g = p.ensureGraph(gi);
                g.active = true;
                presets::styleGraph(g, p.stylePreset);
            }
            Interpreter in(&p);
            in.autoscaleOnRead = 0;
            in.setCurrentGraph(gi);
            in.readDataText(text, Interpreter::ReadMode::Single, SetType::XY, gi, name);
            errs = in.errors();
            Graph& g = p.graphs[gi];
            // several curves on one graph: name unlabelled ones after their file
            int shown = 0;
            for (const DataSet& d : g.sets) if (d.active && d.size() > 0) ++shown;
            if (shown > 1)
                for (DataSet& d : g.sets)
                    if (d.active && d.legend.isEmpty() && !d.comment.isEmpty()) d.legend = QFileInfo(d.comment).completeBaseName();
            g.autoscale();
        });
        m_doc->setCurrentGraph(gi);
        addRecent(path);
        if (!errs.isEmpty()) flash(tr("Opened %1 with %n unsupported statement(s) ignored", nullptr, errs.size()).arg(name), 8000);
        else flash(tr("Opened %1").arg(name), 4000);
        return true;
    }
    ImportDialog d(m_doc, path, text, this);
    if (d.exec() == QDialog::Accepted) {
        addRecent(path);
        return true;
    }
    return false;
}

void MainWindow::openDialog()
{
    const QString dir = QSettings().value(QStringLiteral("files/dir"), QDir::homePath()).toString();
    const QString f = QFileDialog::getOpenFileName(this, tr("Open project or data"), dir,
                                                   tr("All supported (*.cplot *.agr *.xvgr *.par *.xvg *.dat *.txt *.csv *.tsv *.xy *.nxy);;"
                                                      "CactusPlot projects (*.cplot);;Grace projects (*.agr *.xvgr *.par);;"
                                                      "GROMACS / Grace data (*.xvg);;"
                                                      "Data files (*.dat *.txt *.csv *.tsv *.xy *.nxy);;All files (*)"));
    if (f.isEmpty()) return;
    QSettings().setValue(QStringLiteral("files/dir"), QFileInfo(f).absolutePath());
    openFile(f);
}

bool MainWindow::save()
{
    if (m_doc->filePath().isEmpty()) return saveAs();
    QString err;
    if (!projectio::save(m_doc->project(), m_doc->filePath(), &err)) {
        QMessageBox::critical(this, tr("Save failed"), err);
        return false;
    }
    m_doc->markSaved();
    addRecent(m_doc->filePath());
    flash(tr("Saved %1").arg(m_doc->filePath()), 4000);
    QFile::remove(autosavePath());
    return true;
}

bool MainWindow::saveAs()
{
    QString start = m_doc->filePath();
    if (start.isEmpty()) start = QDir(QSettings().value(QStringLiteral("files/dir"), QDir::homePath()).toString()).filePath(tr("untitled.cplot"));
    QString selected;
    QString f = QFileDialog::getSaveFileName(this, tr("Save project"), start, tr("CactusPlot project (*.cplot);;Grace project (*.agr)"), &selected);
    if (f.isEmpty()) return false;
    if (QFileInfo(f).suffix().isEmpty()) f += selected.contains(QLatin1String("agr")) ? QStringLiteral(".agr") : QStringLiteral(".cplot");
    if (f.endsWith(QLatin1String(".agr"), Qt::CaseInsensitive)) {
        const auto r = QMessageBox::information(this, tr("Save in Grace format"),
                                                tr("The Grace format cannot store transparency, colormaps, gradients, rounded "
                                                   "legends and the extra symbols. Save as .agr anyway?"),
                                                QMessageBox::Save | QMessageBox::Cancel);
        if (r != QMessageBox::Save) return false;
    }
    m_doc->setFilePath(f);
    QSettings().setValue(QStringLiteral("files/dir"), QFileInfo(f).absolutePath());
    return save();
}

void MainWindow::importDialog()
{
    const QString dir = QSettings().value(QStringLiteral("files/dir"), QDir::homePath()).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Import data"), dir,
                                                            tr("Data files (*.dat *.txt *.csv *.tsv *.xy *.nxy *.out *.xvg);;All files (*)"));
    for (const QString& f : files) {
        QString text, err;
        if (!datareader::readFile(f, &text, &err)) {
            QMessageBox::critical(this, tr("Import failed"), err);
            continue;
        }
        QSettings().setValue(QStringLiteral("files/dir"), QFileInfo(f).absolutePath());
        ImportDialog d(m_doc, f, text, this);
        if (d.exec() == QDialog::Accepted) addRecent(f);
    }
}

void MainWindow::importClipboard()
{
    const QString text = QApplication::clipboard()->text();
    if (datareader::parseText(text).isEmpty()) {
        QMessageBox::information(this, tr("Paste data"), tr("The clipboard does not contain numeric data."));
        return;
    }
    ImportDialog d(m_doc, QString(), text, this);
    d.exec();
}

void MainWindow::exportDialog()
{
    ExportDialog d(m_doc, this);
    if (d.exec() == QDialog::Accepted) flash(tr("Exported to %1").arg(QSettings().value(QStringLiteral("export/last")).toString()), 5000);
}

void MainWindow::printPlot()
{
    QPrinter printer(QPrinter::HighResolution);
    const Page& pg = m_doc->project().page;
    printer.setPageOrientation(pg.width > pg.height ? QPageLayout::Landscape : QPageLayout::Portrait);
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QPainter p(&printer);
    const QRectF page = printer.pageLayout().paintRectPixels(printer.resolution());
    const double s = std::min(page.width() / pg.width, page.height() / pg.height);
    const QRectF target(0, 0, pg.width * s, pg.height * s);
    PlotRenderer r(m_doc->project());
    r.render(&p, target);
}

void MainWindow::copyImage(bool svg)
{
    auto* mime = new QMimeData;
    exporter::Options o;
    o.format = QStringLiteral("svg");
    const QByteArray svgData = exporter::exportToBytes(m_doc->project(), o);
    mime->setData(QStringLiteral("image/svg+xml"), svgData);
    if (svg) mime->setText(QString::fromUtf8(svgData));
    const QImage img = renderToImage(m_doc->project(), int(m_doc->project().page.width * 300 / 72), false, 300);
    mime->setImageData(img);
    QApplication::clipboard()->setMimeData(mime);
    flash(svg ? tr("Plot copied as SVG and image") : tr("Plot copied as image (300 dpi) and SVG"), 4000);
}

void MainWindow::addGraph(bool inset)
{
    const int cur = m_doc->currentGraph();
    int created = -1;
    m_doc->modify(inset ? tr("Add inset graph") : tr("Add graph"), [&](Project& p) {
        Graph g;
        if (const Graph* c = p.graph(cur)) {
            g = *c;
            g.sets.clear();
            g.title.text.clear();
            g.subtitle.text.clear();
            for (Axis& a : g.axes) a.label.clear();
        } else {
            presets::styleGraph(g, p.stylePreset);
        }
        g.active = true;
        g.hidden = false;
        g.wxmin = 0; g.wxmax = 1; g.wymin = 0; g.wymax = 1;
        g.autoticks();
        if (inset && p.graph(cur)) {
            const Graph* c = p.graph(cur);
            const double w = c->vxmax - c->vxmin, h = c->vymax - c->vymin;
            g.vxmax = c->vxmax - 0.05 * w;
            g.vxmin = g.vxmax - 0.38 * w;
            g.vymax = c->vymax - 0.06 * h;
            g.vymin = g.vymax - 0.38 * h;
            for (Axis& a : g.axes) { a.ticklabelText.size *= 0.8; a.labelText.size *= 0.8; }
            g.frame.background = FillStyle{p.page.background.color, 1};
            p.graphs.push_back(g);
            created = p.graphs.size() - 1;
        } else {
            p.graphs.push_back(g);
            created = p.graphs.size() - 1;
            arrangeGraphs(p, p.activeGraphCount(), 1, 0.1, 0.2, 0.3);
        }
        p.currentGraph = created;
    });
    m_doc->select(Selection{Selection::Graph, created, -1});
}

void MainWindow::applyPreset(const QString& name)
{
    m_doc->modify(tr("Apply style %1").arg(name), [name](Project& p) { presets::applyToProject(p, name); });
    flash(tr("Style “%1” applied").arg(name), 3000);
}

void MainWindow::showContextMenu(const QPoint& pos)
{
    const Selection sel = m_doc->selection();
    QMenu menu(this);
    menu.addAction(icons::get(QStringLiteral("sliders")), tr("Properties"), this, [this] { showAppearance(); });
    if (sel.kind == Selection::Set) {
        menu.addAction(icons::get(QStringLiteral("table")), tr("Edit Data"), this, [this, sel] {
            m_sheet->showSet(sel.graph, sel.index);
            showWindow(m_sheetDock);
        });
        menu.addAction(icons::get(QStringLiteral("fit")), tr("Fit Curve..."), this, [this] { m_actions.value(QStringLiteral("fit"))->trigger(); });
        menu.addAction(icons::get(QStringLiteral("transform")), tr("Transform..."), this, [this] { m_actions.value(QStringLiteral("transform"))->trigger(); });
        menu.addAction(icons::get(QStringLiteral("eye-off")), tr("Hide"), this, [this, sel] {
            m_doc->modify(tr("Hide set"), [sel](Project& p) { if (Graph* g = p.graph(sel.graph)) if (DataSet* s = g->set(sel.index)) s->hidden = true; });
        });
        menu.addAction(icons::get(QStringLiteral("trash")), tr("Delete"), this, [this, sel] {
            m_doc->modify(tr("Delete set"), [sel](Project& p) { if (Graph* g = p.graph(sel.graph)) if (sel.index < g->sets.size()) g->sets[sel.index] = DataSet(); });
        });
    } else if (sel.kind == Selection::Object) {
        menu.addAction(icons::get(QStringLiteral("trash")), tr("Delete"), this, [this, sel] {
            m_doc->modify(tr("Delete annotation"), [sel](Project& p) { if (sel.index >= 0 && sel.index < p.objects.size()) p.objects.remove(sel.index); });
            m_doc->select(Selection{Selection::Page, -1, -1});
        });
    }
    if (sel.graph >= 0) {
        menu.addSeparator();
        menu.addAction(m_actions.value(QStringLiteral("autoscale")));
        menu.addAction(m_actions.value(QStringLiteral("grid")));
        menu.addAction(m_actions.value(QStringLiteral("legend")));
    }
    menu.addSeparator();
    menu.addAction(m_actions.value(QStringLiteral("copyimg")));
    menu.addAction(m_actions.value(QStringLiteral("export")));
    menu.exec(pos);
}

QString MainWindow::autosavePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("autosave.cplot"));
}

void MainWindow::autosave()
{
    if (!m_doc->isModified()) return;
    QString err;
    if (projectio::saveAs(m_doc->project(), autosavePath(), projectio::Dialect::Native, &err)) {
        QSettings().setValue(QStringLiteral("autosave/original"), m_doc->filePath());
        flash(tr("Autosaved"), 2000);
    }
}

void MainWindow::checkAutosaveRecovery()
{
    const QString path = autosavePath();
    if (!QFileInfo::exists(path)) return;
    const auto r = QMessageBox::question(this, tr("Recover unsaved work"),
                                         tr("CactusPlot found an autosaved project from %1 that was not saved. Recover it?")
                                             .arg(QLocale().toString(QFileInfo(path).lastModified(), QLocale::ShortFormat)),
                                         QMessageBox::Yes | QMessageBox::Discard, QMessageBox::Yes);
    if (r == QMessageBox::Yes) {
        Project p;
        if (projectio::load(path, &p)) {
            setProject(p, QString());
            m_doc->modify(tr("Recovered project"), [](Project&) {});
        }
    }
    QFile::remove(path);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (!maybeSave()) {
        e->ignore();
        return;
    }
    QSettings st;
    st.setValue(QStringLiteral("window/geometry"), saveGeometry());
    st.setValue(QStringLiteral("window/state2"), saveState());
    for (QDialog* d : {m_inspectorDock, m_treeDock, m_consoleDock, m_sheetDock})
        st.setValue(QStringLiteral("windows/") + d->objectName(), d->saveGeometry());
    QFile::remove(autosavePath());
    e->accept();
}

void MainWindow::applyTheme()
{
    QSettings st;
    theme::apply(*qApp, ThemeMode(st.value(QStringLiteral("ui/theme"), 0).toInt()), st.value(QStringLiteral("ui/native"), false).toBool());
    // refresh icons
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it) {
        (void)it;
    }
    m_view->update();
    QMessageBox::information(this, tr("Theme"), tr("Some icons update after restarting CactusPlot."));
}

void MainWindow::showCommandReference()
{
    auto* d = new QDialog(this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("Command reference"));
    d->resize(760, 640);
    auto* lay = new QVBoxLayout(d);
    auto* b = new QTextBrowser(d);
    b->setOpenExternalLinks(true);
    b->setHtml(tr(
        "<h2>Command language</h2>"
        "<p>The console, batch files (<code>-batch</code>), parameter files (<code>-param</code>) and project files all use the Grace "
        "command language. Statements are case-insensitive and may be separated by <code>;</code>.</p>"
        "<h3>Data</h3><pre>"
        "s0.x = mesh(0, 10, 200)        create/replace columns\n"
        "s0.y = sin(s0.x) * exp(-x/5)   x, y refer to the current set\n"
        "g1.s2.y = s0.y - avg(s0.y)     sets of other graphs\n"
        "read \"data.dat\"  |  read xydy \"err.dat\"  |  read nxy \"many.dat\"\n"
        "read block \"table.dat\"; block xydy \"0:2:3\"\n"
        "copy s0 to s1  |  move g0.s1 to g1.s0  |  swap s0 and s1  |  kill s3\n"
        "sort s0 y descending  |  reverse s0  |  join s0, s1  |  split s0 100\n"
        "write s0 file \"out.dat\" format \"%.6g\"</pre>"
        "<h3>Transformations</h3><pre>"
        "runavg(s0, 5)  runmed  runmin  runmax  runstd\n"
        "interpolate(s0, mesh(0, 1, 101), spline, on)      linear | spline | aspline\n"
        "histogram(s0, mesh(0, 1, 21), off, on)            cumulative, normalise\n"
        "integrate(s0)  difference(s0, centered)  seasonaldiff(s0, 12)\n"
        "fft(s0, magnitude)  xcor(s0, s1, 50, off)  linconv(s0, s1)\n"
        "regress(s0, 2)  restrict(s0, s0.x > 0)  restrict(s0, r0, off)</pre>"
        "<h3>Fitting</h3><pre>"
        "fit formula \"y = a0*exp(-a1*x) + a2\"\n"
        "fit with 3 parameters; a0 = 1; a1 = 0.1; a2 = 0; a2 constraints on; a2min = 0\n"
        "nonlfit(s0, 200)</pre>"
        "<h3>Graphs and appearance</h3><pre>"
        "with g0  |  focus g1  |  arrange(2, 2, 0.1, 0.15, 0.2)\n"
        "world 0, -1, 10, 1   view 0.15, 0.15, 1.15, 0.85   autoscale   autoticks\n"
        "xaxes scale logarithmic   yaxis tick major 0.5   xaxis label \"Time (s)\"\n"
        "title \"My plot\"   legend on   legend 0.8, 0.8   s0 legend \"Data\"\n"
        "s0 line color \"tomato\"   s0 symbol 1   s0 symbol fill color \"#0072b2\"\n"
        "s0 colormap \"viridis\"   s0 colormap bar on   s0 type xycolor\n"
        "page size 792, 612   print to \"out.pdf\"   hardcopy device \"pdf\"   print</pre>"
        "<h3>Expressions</h3><p>Operators <code>+ - * / % ^ == != &lt; &lt;= &gt; &gt;= &amp;&amp; || ! ?:</code>; "
        "functions: %1.</p>"
        "<h3>Text</h3><p>Use <code>$...$</code> for math (<code>$\\alpha^2_i$</code>, <code>$\\sqrt{x}$</code>) or Grace escapes: "
        "<code>\\S</code> superscript, <code>\\s</code> subscript, <code>\\N</code> normal, <code>\\x</code> Symbol font, "
        "<code>\\f{Helvetica-Bold}</code>, <code>\\R{red}</code>, <code>\\u</code>/<code>\\U</code> underline, "
        "<code>\\o</code>/<code>\\O</code> overline, <code>\\+</code>/<code>\\-</code> size.</p>")
                   .arg(expr::functionNames().join(QStringLiteral(", "))));
    lay->addWidget(b);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, d);
    connect(bb, &QDialogButtonBox::rejected, d, &QDialog::close);
    lay->addWidget(bb);
    d->show();
}

void MainWindow::showShortcuts()
{
    QMessageBox::information(
        this, tr("Mouse and keyboard"),
        tr("<table>"
           "<tr><td><b>Mouse wheel</b></td><td>zoom the graph under the cursor (Shift: X only, Alt: Y only)</td></tr>"
           "<tr><td><b>Ctrl + wheel</b></td><td>zoom the page view</td></tr>"
           "<tr><td><b>Drag in a graph</b></td><td>pan (Select tool) &mdash; middle button pans with any tool</td></tr>"
           "<tr><td><b>Drag legend / annotation</b></td><td>move it; arrow keys nudge (Shift: more)</td></tr>"
           "<tr><td><b>Double-click</b></td><td>open the properties of the clicked element</td></tr>"
           "<tr><td><b>Delete</b></td><td>delete the selected set or annotation</td></tr>"
           "<tr><td><b>Esc</b></td><td>back to the Select tool</td></tr>"
           "<tr><td><b>V H Z C P T L A B E R</b></td><td>select, pan, zoom box, crosshair, edit points, text, line, arrow, box, ellipse, region</td></tr>"
           "<tr><td><b>Ctrl+Shift+A</b></td><td>autoscale &nbsp; <b>Ctrl+G</b> grid &nbsp; <b>Ctrl+L</b> legend</td></tr>"
           "<tr><td><b>Ctrl+I / Ctrl+Shift+V</b></td><td>import data / from clipboard</td></tr>"
           "<tr><td><b>Ctrl+E / Ctrl+Shift+E</b></td><td>export / export again</td></tr>"
           "<tr><td><b>Ctrl+T / Ctrl+F / Ctrl+R</b></td><td>transformations / curve fitting / evaluate expression</td></tr>"
           "<tr><td><b>Ctrl+K</b></td><td>command console</td></tr>"
           "</table>"));
}

void MainWindow::about()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("About CactusPlot"));
    box.setIconPixmap(icons::appIcon().pixmap(96, 96));
    box.setText(tr("<h2>CactusPlot %1</h2><p>Modern, cross-platform 2D scientific plotting, compatible with Grace/xmgrace "
                   "projects and batch scripts.</p><p>Built with Qt %2. Licensed under the GNU GPL version 2 or later.</p>"
                   "<p><a href='%3'>%3</a></p><p>Includes TeX Gyre fonts (GUST Font License) and colour data from "
                   "matplotlib, ColorBrewer, cmocean, Scientific Colour Maps (F. Crameri), colorcet, Paul Tol and xkcd.</p>")
                    .arg(QStringLiteral(CACTUS_VERSION), QString::fromLatin1(qVersion()), QStringLiteral(CACTUS_HOMEPAGE)));
    box.exec();
}

}  // namespace cactus::ui
