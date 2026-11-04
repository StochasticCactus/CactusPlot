// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/Inspector.h"

#include <QComboBox>
#include <QPainter>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/Presets.h"
#include "core/color/Colors.h"
#include "plot/RichText.h"
#include "ui/Document.h"
#include "ui/Form.h"
#include "ui/Theme.h"

// Binding helpers: expression evaluated on the selected element (or a default).
#define PGET(T, expr) [this](const Project& p) -> T { Q_UNUSED(this); return T(expr); }
#define PSET(T, stmt) [this](Project& p, T v) { Q_UNUSED(this); stmt; }
#define GGET(T, expr) [this](const Project& p) -> T { const Graph* g = p.graph(m_g); if (!g) return T(); return T(expr); }
#define GSET(T, stmt) [this](Project& p, T v) { Graph* g = p.graph(m_g); if (!g) return; stmt; }
#define AGET(T, expr) [this](const Project& p) -> T { const Graph* g = p.graph(m_g); if (!g) return T(); const Axis& a = g->axes[m_a]; return T(expr); }
#define ASET(T, stmt) [this](Project& p, T v) { Graph* g = p.graph(m_g); if (!g) return; Axis& a = g->axes[m_a]; stmt; }
#define SGET(T, expr) [this](const Project& p) -> T { const Graph* g = p.graph(m_g); const DataSet* s = g ? g->set(m_s) : nullptr; if (!s) return T(); return T(expr); }
#define SSET(T, stmt) [this](Project& p, T v) { Graph* g = p.graph(m_g); DataSet* s = g ? g->set(m_s) : nullptr; if (!s) return; stmt; }
#define OGET(T, expr) [this](const Project& p) -> T { if (m_o < 0 || m_o >= p.objects.size()) return T(); const DrawObject& o = p.objects[m_o]; return T(expr); }
#define OSET(T, stmt) [this](Project& p, T v) { if (m_o < 0 || m_o >= p.objects.size()) return; DrawObject& o = p.objects[m_o]; stmt; }

namespace cactus::ui {

namespace {

QWidget* scrolled(QWidget* inner)
{
    auto* sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(inner);
    return sa;
}

QStringList setTypeNames()
{
    QStringList l;
    for (int i = 0; i < int(SetType::Count); ++i) l << setTypeName(SetType(i)).toUpper();
    return l;
}

QStringList symbolNames()
{
    QStringList l;
    for (int i = 0; i < int(Symbol::Count); ++i) l << symbolName(Symbol(i));
    return l;
}

QStringList formatNames()
{
    QStringList l;
    for (int i = 0; i < int(LabelFormat::Count); ++i) l << labelFormatName(LabelFormat(i));
    return l;
}

const QStringList kSides{QObject::tr("Normal side"), QObject::tr("Opposite side"), QObject::tr("Both sides")};

}  // namespace

Inspector::Inspector(Document* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    m_title = new QLabel(this);
    m_title->setContentsMargins(10, 8, 10, 6);
    QFont f = m_title->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.08);
    m_title->setFont(f);
    lay->addWidget(m_title);
    m_stack = new QStackedWidget(this);
    lay->addWidget(m_stack, 1);

    m_emptyW = new QLabel(tr("Select something in the plot or in the project tree to edit its properties."), this);
    static_cast<QLabel*>(m_emptyW)->setWordWrap(true);
    static_cast<QLabel*>(m_emptyW)->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    static_cast<QLabel*>(m_emptyW)->setContentsMargins(12, 12, 12, 12);
    m_stack->addWidget(m_emptyW);
    m_pageW = buildPage();
    m_graphW = buildGraph();
    m_axisW = buildAxis();
    m_setW = buildSet();
    m_textW = buildText();
    m_lineW = buildLine();
    m_shapeW = buildShape();
    for (QWidget* w : {m_pageW, m_graphW, m_axisW, m_setW, m_textW, m_lineW, m_shapeW}) m_stack->addWidget(w);

    connect(doc, &Document::selectionChanged, this, &Inspector::onSelection);
    connect(doc, &Document::changed, this, &Inspector::refresh);
    onSelection();
}

void Inspector::onSelection()
{
    const Selection& s = m_doc->selection();
    const Project& p = m_doc->project();
    m_g = s.graph >= 0 ? s.graph : m_doc->currentGraph();
    QWidget* show = m_emptyW;
    QString title;
    switch (s.kind) {
    case Selection::None:
    case Selection::Page:
        show = m_pageW;
        title = tr("Page");
        break;
    case Selection::Graph:
    case Selection::Title:
    case Selection::Legend:
        show = m_graphW;
        title = tr("Graph G%1").arg(m_g);
        if (m_graphTabs) m_graphTabs->setCurrentIndex(s.kind == Selection::Legend ? 1 : 0);
        break;
    case Selection::Axis:
        m_a = std::clamp(s.index, 0, 3);
        show = m_axisW;
        title = tr("Axes of G%1").arg(m_g);
        if (m_axisChooser) m_axisChooser->setCurrentIndex(m_a);
        break;
    case Selection::Set: {
        m_s = s.index;
        show = m_setW;
        const Graph* g = p.graph(m_g);
        const DataSet* d = g ? g->set(m_s) : nullptr;
        QString name = d ? richtext::plainText(d->legend) : QString();
        title = tr("Set G%1.S%2").arg(m_g).arg(m_s) + (name.isEmpty() ? QString() : QStringLiteral(" — ") + name);
        break;
    }
    case Selection::Object: {
        m_o = s.index;
        if (m_o >= 0 && m_o < p.objects.size()) {
            switch (p.objects[m_o].kind) {
            case DrawObjectKind::Text: show = m_textW; title = tr("Text"); break;
            case DrawObjectKind::Line: show = m_lineW; title = tr("Line / arrow"); break;
            default: show = m_shapeW; title = p.objects[m_o].kind == DrawObjectKind::Box ? tr("Box") : tr("Ellipse"); break;
            }
        }
        break;
    }
    }
    m_title->setText(title);
    m_stack->setCurrentWidget(show);
    refresh();
}

void Inspector::refresh()
{
    for (Form* f : m_forms) f->refresh();
    if (m_editData) {
        const Graph* g = m_doc->project().graph(m_g);
        const DataSet* s = g ? g->set(m_s) : nullptr;
        m_editData->setText(s ? tr("Edit data (%1 points)...").arg(s->size()) : tr("Edit data..."));
    }
}

// ====================================================================== page

QWidget* Inspector::buildPage()
{
    auto* f = new Form(m_doc);
    m_forms << f;
    f->section(tr("Page size"));
    struct Size { const char* name; double w, h; };
    static const Size sizes[] = {{"Letter landscape (11 × 8.5 in)", 792, 612}, {"Letter portrait", 612, 792},
                                 {"A4 landscape", 842, 595},   {"A4 portrait", 595, 842},
                                 {"Screen 4:3 (800 × 600)", 800, 600}, {"Wide 16:9 (1280 × 720)", 960, 540},
                                 {"Square", 600, 600},         {"Journal, 1 column (3.5 × 2.6 in)", 252, 187},
                                 {"Journal, 2 columns (7 × 4.3 in)", 504, 310}, {"Custom", 0, 0}};
    QStringList names;
    for (const Size& s : sizes) names << tr(s.name);
    f->combo(tr("Preset"), names,
             PGET(int, [&] {
                 for (int i = 0; i < 9; ++i)
                     if (std::fabs(p.page.width - sizes[i].w) < 0.5 && std::fabs(p.page.height - sizes[i].h) < 0.5) return i;
                 return 9;
             }()),
             PSET(int, if (v < 9) { p.page.width = sizes[v].w; p.page.height = sizes[v].h; }));
    f->real(tr("Width"), PGET(double, p.page.width), PSET(double, p.page.width = v), 36, 20000, 1, 1, tr("pt"));
    f->real(tr("Height"), PGET(double, p.page.height), PSET(double, p.page.height = v), 36, 20000, 1, 1, tr("pt"));
    f->section(tr("Background"));
    f->check(tr("Fill the page background"), PGET(bool, p.page.backgroundFill), PSET(bool, p.page.backgroundFill = v));
    f->color(tr("Colour"), PGET(QColor, p.page.background.color), PSET(QColor, p.page.background.color = v));
    f->combo(tr("Gradient"), {tr("None"), tr("Vertical"), tr("Horizontal"), tr("Radial")}, PGET(int, int(p.page.background.gradient)),
             PSET(int, p.page.background.gradient = GradientType(v)));
    f->color(tr("Gradient end"), PGET(QColor, p.page.background.color2), PSET(QColor, p.page.background.color2 = v));

    f->section(tr("Style"));
    auto* presetBox = new QComboBox(f);
    presetBox->addItems(presets::names());
    for (int i = 0; i < presetBox->count(); ++i) presetBox->setItemData(i, presets::description(presetBox->itemText(i)), Qt::ToolTipRole);
    auto* applyPreset = new QPushButton(tr("Apply"), f);
    auto* row = new QWidget(f);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->addWidget(presetBox, 1);
    rl->addWidget(applyPreset);
    f->row(tr("Preset"), row);
    connect(applyPreset, &QPushButton::clicked, this, [this, presetBox] { emit applyPresetRequested(presetBox->currentText()); });
    auto* palBox = new QComboBox(f);
    palBox->setMaxVisibleItems(25);
    palBox->setIconSize(QSize(96, 12));
    for (const Palette& pal : colors::palettes()) {
        QPixmap pm(192, 24);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        const int n = std::min(12, int(pal.colors.size()));
        for (int i = 0; i < n; ++i) pp.fillRect(QRectF(i * 96.0 / n, 0, 96.0 / n, 12), pal.colors[i]);
        pp.end();
        palBox->addItem(QIcon(pm), pal.name);
    }
    auto* applyPal = new QPushButton(tr("Apply"), f);
    auto* row2 = new QWidget(f);
    auto* rl2 = new QHBoxLayout(row2);
    rl2->setContentsMargins(0, 0, 0, 0);
    rl2->addWidget(palBox, 1);
    rl2->addWidget(applyPal);
    f->row(tr("Colours"), row2);
    connect(applyPal, &QPushButton::clicked, this, [this, palBox] {
        const QString name = palBox->currentText();
        m_doc->modify(tr("Apply palette %1").arg(name), [name](Project& p) { presets::applyPalette(p, name); });
    });
    f->check(tr("Typographic numbers (−, ×10ⁿ)"), PGET(bool, p.prettyNumbers), PSET(bool, p.prettyNumbers = v));

    f->section(tr("Time stamp"));
    f->check(tr("Show time stamp"), PGET(bool, p.timestamp.active), PSET(bool, p.timestamp.active = v));
    f->text(tr("Text"), PGET(QString, p.timestamp.value), PSET(QString, p.timestamp.value = v));
    f->section(tr("Dates"));
    f->real(tr("Reference date"), PGET(double, p.referenceDate), PSET(double, p.referenceDate = v), -1e9, 1e9, 1, 3, tr("JD"));
    f->check(tr("Two-digit years"), PGET(bool, p.dateWrap), PSET(bool, p.dateWrap = v));
    f->section(tr("Description"));
    f->multiText(QString(), PGET(QString, p.description), PSET(QString, p.description = v));
    return scrolled(f);
}

// ====================================================================== graph

QWidget* Inspector::buildGraph()
{
    m_graphTabs = new QTabWidget;
    m_graphTabs->setDocumentMode(true);
    auto valid = [this](const Project& p) { return p.graph(m_g) != nullptr; };

    auto* f = new Form(m_doc);
    m_forms << f;
    f->setValidity(valid);
    f->section(tr("Titles"));
    f->text(tr("Title"), GGET(QString, g->title.text), GSET(QString, g->title.text = v), tr("e.g. Spectrum of $\\alpha$-Fe"));
    f->text(tr("Subtitle"), GGET(QString, g->subtitle.text), GSET(QString, g->subtitle.text = v));
    f->font(tr("Font"), GGET(QString, g->title.style.font), GSET(QString, g->title.style.font = v; g->subtitle.style.font = v));
    f->real(tr("Title size"), GGET(double, g->title.style.size), GSET(double, g->title.style.size = v), 0.1, 10, 0.05, 2);
    f->real(tr("Subtitle size"), GGET(double, g->subtitle.style.size), GSET(double, g->subtitle.style.size = v), 0.1, 10, 0.05, 2);
    f->color(tr("Title colour"), GGET(QColor, g->title.style.color), GSET(QColor, g->title.style.color = v));
    f->section(tr("Type and scales"));
    f->combo(tr("Graph type"), {tr("XY"), tr("Chart"), tr("Polar"), tr("Smith"), tr("Fixed aspect"), tr("Pie")}, GGET(int, int(g->type)),
             GSET(int, g->type = GraphType(v); g->autoscale()));
    f->check(tr("Stacked (charts)"), GGET(bool, g->stacked), GSET(bool, g->stacked = v; g->autoscale(2)));
    f->real(tr("Bar gap"), GGET(double, g->barHGap), GSET(double, g->barHGap = v), 0, 5, 0.05, 2);
    const QStringList scales{tr("Linear"), tr("Logarithmic"), tr("Reciprocal"), tr("Logit")};
    f->combo(tr("X scale"), scales, GGET(int, int(g->xscale)), GSET(int, g->xscale = ScaleType(v); g->autoscale(1)));
    f->combo(tr("Y scale"), scales, GGET(int, int(g->yscale)), GSET(int, g->yscale = ScaleType(v); g->autoscale(2)));
    f->check(tr("Invert X"), GGET(bool, g->xinvert), GSET(bool, g->xinvert = v));
    f->check(tr("Invert Y"), GGET(bool, g->yinvert), GSET(bool, g->yinvert = v));
    f->section(tr("World (data range)"));
    f->real(tr("X min"), GGET(double, g->wxmin), GSET(double, g->wxmin = v; g->autoticks()), -1e300, 1e300, 0.1, 6);
    f->real(tr("X max"), GGET(double, g->wxmax), GSET(double, g->wxmax = v; g->autoticks()), -1e300, 1e300, 0.1, 6);
    f->real(tr("Y min"), GGET(double, g->wymin), GSET(double, g->wymin = v; g->autoticks()), -1e300, 1e300, 0.1, 6);
    f->real(tr("Y max"), GGET(double, g->wymax), GSET(double, g->wymax = v; g->autoticks()), -1e300, 1e300, 0.1, 6);
    auto* auto_ = new QPushButton(icons::get(QStringLiteral("autoscale")), tr("Autoscale"), f);
    connect(auto_, &QPushButton::clicked, this, [this] {
        m_doc->modify(tr("Autoscale"), [this](Project& p) { if (Graph* g = p.graph(m_g)) g->autoscale(); });
    });
    f->widget(auto_);
    f->section(tr("Viewport (position on the page)"));
    f->real(tr("Left"), GGET(double, g->vxmin), GSET(double, g->vxmin = v), -1, 5, 0.01, 3);
    f->real(tr("Right"), GGET(double, g->vxmax), GSET(double, g->vxmax = v), -1, 5, 0.01, 3);
    f->real(tr("Bottom"), GGET(double, g->vymin), GSET(double, g->vymin = v), -1, 5, 0.01, 3);
    f->real(tr("Top"), GGET(double, g->vymax), GSET(double, g->vymax = v), -1, 5, 0.01, 3);
    f->check(tr("Hidden"), GGET(bool, g->hidden), GSET(bool, g->hidden = v));
    f->real(tr("Size normalisation (znorm)"), GGET(double, g->znorm), GSET(double, g->znorm = v), 1e-12, 1e12, 0.1, 4);
    m_graphTabs->addTab(scrolled(f), tr("Graph"));

    auto* L = new Form(m_doc);
    m_forms << L;
    L->setValidity(valid);
    L->section(tr("Legend"));
    L->check(tr("Show legend"), GGET(bool, g->legend.active), GSET(bool, g->legend.active = v));
    L->check(tr("Place automatically (avoid data)"), GGET(bool, g->legend.autoPlace), GSET(bool, g->legend.autoPlace = v));
    L->combo(tr("Coordinates"), {tr("World"), tr("Viewport")}, GGET(int, int(g->legend.loctype)), GSET(int, g->legend.loctype = LocType(v)));
    L->real(tr("X"), GGET(double, g->legend.pos.x()), GSET(double, g->legend.pos.setX(v); g->legend.autoPlace = false), -1e300, 1e300, 0.01, 4);
    L->real(tr("Y"), GGET(double, g->legend.pos.y()), GSET(double, g->legend.pos.setY(v); g->legend.autoPlace = false), -1e300, 1e300, 0.01, 4);
    L->font(tr("Font"), GGET(QString, g->legend.text.font), GSET(QString, g->legend.text.font = v));
    L->real(tr("Text size"), GGET(double, g->legend.text.size), GSET(double, g->legend.text.size = v), 0.1, 10, 0.05, 2);
    L->color(tr("Text colour"), GGET(QColor, g->legend.text.color), GSET(QColor, g->legend.text.color = v));
    L->integer(tr("Sample length"), GGET(int, g->legend.length), GSET(int, g->legend.length = v), 0, 20);
    L->integer(tr("Row gap"), GGET(int, g->legend.vgap), GSET(int, g->legend.vgap = v), 0, 20);
    L->integer(tr("Text gap"), GGET(int, g->legend.hgap), GSET(int, g->legend.hgap = v), 0, 20);
    L->check(tr("Reverse order"), GGET(bool, g->legend.invert), GSET(bool, g->legend.invert = v));
    L->section(tr("Box"));
    L->lineStyle(tr("Border"), GGET(int, g->legend.box.style), GSET(int, g->legend.box.style = v));
    L->color(tr("Border colour"), GGET(QColor, g->legend.box.color), GSET(QColor, g->legend.box.color = v));
    L->real(tr("Border width"), GGET(double, g->legend.box.width), GSET(double, g->legend.box.width = v), 0, 20, 0.25, 2);
    L->color(tr("Fill"), GGET(QColor, g->legend.boxFill.color), GSET(QColor, g->legend.boxFill.color = v; if (g->legend.boxFill.pattern == 0) g->legend.boxFill.pattern = 1));
    L->real(tr("Corner radius"), GGET(double, g->legend.cornerRadius), GSET(double, g->legend.cornerRadius = v), 0, 0.1, 0.002, 3);
    m_graphTabs->addTab(scrolled(L), tr("Legend"));

    auto* F = new Form(m_doc);
    m_forms << F;
    F->setValidity(valid);
    F->section(tr("Frame"));
    F->combo(tr("Type"), {tr("Closed"), tr("Half open"), tr("Break top"), tr("Break bottom"), tr("Break left"), tr("Break right")},
             GGET(int, int(g->frame.type)), GSET(int, g->frame.type = FrameType(v)));
    F->lineStyle(tr("Line"), GGET(int, g->frame.line.style), GSET(int, g->frame.line.style = v));
    F->color(tr("Colour"), GGET(QColor, g->frame.line.color), GSET(QColor, g->frame.line.color = v; if (g->frame.line.style == 0) g->frame.line.style = 1));
    F->real(tr("Width"), GGET(double, g->frame.line.width), GSET(double, g->frame.line.width = v; if (g->frame.line.style == 0) g->frame.line.style = 1), 0, 20, 0.25, 2);
    F->section(tr("Plot area background"));
    F->pattern(tr("Pattern"), GGET(int, g->frame.background.pattern), GSET(int, g->frame.background.pattern = v));
    F->color(tr("Colour"), GGET(QColor, g->frame.background.color), GSET(QColor, g->frame.background.color = v; if (g->frame.background.pattern == 0) g->frame.background.pattern = 1));
    F->combo(tr("Gradient"), {tr("None"), tr("Vertical"), tr("Horizontal"), tr("Radial")}, GGET(int, int(g->frame.background.gradient)),
             GSET(int, g->frame.background.gradient = GradientType(v)));
    F->color(tr("Gradient end"), GGET(QColor, g->frame.background.color2), GSET(QColor, g->frame.background.color2 = v));
    m_graphTabs->addTab(scrolled(F), tr("Frame"));
    return m_graphTabs;
}

// ====================================================================== axis

QWidget* Inspector::buildAxis()
{
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(6, 0, 6, 0);
    m_axisChooser = new QComboBox(w);
    m_axisChooser->addItems({tr("X axis"), tr("Y axis"), tr("Alternate X axis"), tr("Alternate Y axis")});
    lay->addWidget(m_axisChooser);
    connect(m_axisChooser, &QComboBox::activated, this, [this](int i) { m_doc->select(Selection{Selection::Axis, m_g, i}); });
    auto* tabs = new QTabWidget(w);
    tabs->setDocumentMode(true);
    lay->addWidget(tabs, 1);
    auto valid = [this](const Project& p) { return p.graph(m_g) != nullptr; };

    auto* f = new Form(m_doc);
    m_forms << f;
    f->setValidity(valid);
    f->check(tr("Axis visible"), AGET(bool, a.active), ASET(bool, a.active = v));
    f->section(tr("Label"));
    f->text(tr("Label"), AGET(QString, a.label), ASET(QString, a.label = v), tr("e.g. Time $t$ (s)"));
    f->font(tr("Font"), AGET(QString, a.labelText.font), ASET(QString, a.labelText.font = v));
    f->real(tr("Size"), AGET(double, a.labelText.size), ASET(double, a.labelText.size = v), 0.1, 10, 0.05, 2);
    f->color(tr("Colour"), AGET(QColor, a.labelText.color), ASET(QColor, a.labelText.color = v));
    f->combo(tr("Orientation"), {tr("Parallel"), tr("Perpendicular")}, AGET(int, int(a.labelLayout)), ASET(int, a.labelLayout = LabelLayout(v)));
    f->combo(tr("Side"), kSides, AGET(int, int(a.labelSide)), ASET(int, a.labelSide = SideOp(v)));
    f->check(tr("Automatic placement"), AGET(bool, a.labelPlaceAuto), ASET(bool, a.labelPlaceAuto = v));
    f->real(tr("Offset along"), AGET(double, a.labelOffset.x()), ASET(double, a.labelOffset.setX(v)), -2, 2, 0.005, 3);
    f->real(tr("Offset away"), AGET(double, a.labelOffset.y()), ASET(double, a.labelOffset.setY(v)), -2, 2, 0.005, 3);
    f->section(tr("Axis line"));
    f->check(tr("Draw axis line (over the frame)"), AGET(bool, a.barOn), ASET(bool, a.barOn = v));
    f->color(tr("Colour"), AGET(QColor, a.bar.color), ASET(QColor, a.bar.color = v; a.barOn = true));
    f->real(tr("Width"), AGET(double, a.bar.width), ASET(double, a.bar.width = v; a.barOn = true), 0, 20, 0.25, 2);
    f->check(tr("Place at zero of the other axis"), AGET(bool, a.zero), ASET(bool, a.zero = v; if (v) a.barOn = true));
    f->real(tr("Offset (normal side)"), AGET(double, a.offset.x()), ASET(double, a.offset.setX(v); if (v > 0) a.barOn = true), 0, 2, 0.005, 3);
    f->real(tr("Offset (opposite side)"), AGET(double, a.offset.y()), ASET(double, a.offset.setY(v); if (v > 0) a.barOn = true), 0, 2, 0.005, 3);
    tabs->addTab(scrolled(f), tr("Axis"));

    auto* t = new Form(m_doc);
    m_forms << t;
    t->setValidity(valid);
    t->check(tr("Draw ticks"), AGET(bool, a.tickOn), ASET(bool, a.tickOn = v));
    t->section(tr("Spacing"));
    t->real(tr("Major spacing"), AGET(double, a.major), ASET(double, a.major = v), 0, 1e300, 0.1, 6);
    t->integer(tr("Minor ticks"), AGET(int, a.minorTicks), ASET(int, a.minorTicks = v), 0, 50);
    t->integer(tr("Automatic count"), AGET(int, a.autoTickCount), ASET(int, a.autoTickCount = v), 2, 40);
    auto* autot = new QPushButton(tr("Choose spacing automatically"), t);
    connect(autot, &QPushButton::clicked, this, [this] {
        m_doc->modify(tr("Automatic ticks"), [this](Project& p) { if (Graph* g = p.graph(m_g)) g->autoticks(); });
    });
    t->widget(autot);
    t->check(tr("Round tick positions"), AGET(bool, a.placeRounded), ASET(bool, a.placeRounded = v));
    t->section(tr("Appearance"));
    t->combo(tr("Direction"), {tr("In"), tr("Out"), tr("Both")}, AGET(int, int(a.tickDir)), ASET(int, a.tickDir = TickDirection(v)));
    t->combo(tr("Sides"), kSides, AGET(int, int(a.tickSide)), ASET(int, a.tickSide = SideOp(v)));
    t->real(tr("Major length"), AGET(double, a.majorProps.size), ASET(double, a.majorProps.size = v; if (a.majorProps.line.style == 0) a.majorProps.line.style = 1), 0, 5, 0.05, 2);
    t->real(tr("Minor length"), AGET(double, a.minorProps.size), ASET(double, a.minorProps.size = v; if (a.minorProps.line.style == 0) a.minorProps.line.style = 1), 0, 5, 0.05, 2);
    t->color(tr("Major colour"), AGET(QColor, a.majorProps.line.color), ASET(QColor, a.majorProps.line.color = v; if (a.majorProps.line.style == 0) a.majorProps.line.style = 1));
    t->real(tr("Major width"), AGET(double, a.majorProps.line.width), ASET(double, a.majorProps.line.width = v; if (a.majorProps.line.style == 0) a.majorProps.line.style = 1), 0, 20, 0.25, 2);
    t->color(tr("Minor colour"), AGET(QColor, a.minorProps.line.color), ASET(QColor, a.minorProps.line.color = v; if (a.minorProps.line.style == 0) a.minorProps.line.style = 1));
    t->real(tr("Minor width"), AGET(double, a.minorProps.line.width), ASET(double, a.minorProps.line.width = v; if (a.minorProps.line.style == 0) a.minorProps.line.style = 1), 0, 20, 0.25, 2);
    t->section(tr("Grid"));
    t->check(tr("Major grid lines"), AGET(bool, a.majorProps.grid), ASET(bool, a.majorProps.grid = v));
    t->color(tr("Major grid colour"), AGET(QColor, a.majorProps.gridLine.color), ASET(QColor, a.majorProps.gridLine.color = v));
    t->real(tr("Major grid width"), AGET(double, a.majorProps.gridLine.width), ASET(double, a.majorProps.gridLine.width = v), 0, 20, 0.25, 2);
    t->lineStyle(tr("Major grid style"), AGET(int, a.majorProps.gridLine.style), ASET(int, a.majorProps.gridLine.style = v));
    t->check(tr("Minor grid lines"), AGET(bool, a.minorProps.grid), ASET(bool, a.minorProps.grid = v));
    t->color(tr("Minor grid colour"), AGET(QColor, a.minorProps.gridLine.color), ASET(QColor, a.minorProps.gridLine.color = v));
    t->lineStyle(tr("Minor grid style"), AGET(int, a.minorProps.gridLine.style), ASET(int, a.minorProps.gridLine.style = v));
    tabs->addTab(scrolled(t), tr("Ticks"));

    auto* l = new Form(m_doc);
    m_forms << l;
    l->setValidity(valid);
    l->check(tr("Show tick labels"), AGET(bool, a.ticklabelOn), ASET(bool, a.ticklabelOn = v));
    l->section(tr("Format"));
    l->combo(tr("Format"), formatNames(), AGET(int, int(a.format)), ASET(int, a.format = LabelFormat(v)));
    l->integer(tr("Precision"), AGET(int, a.precision), ASET(int, a.precision = v), 0, 17);
    l->text(tr("Prefix"), AGET(QString, a.prepend), ASET(QString, a.prepend = v));
    l->text(tr("Suffix"), AGET(QString, a.append), ASET(QString, a.append = v));
    l->text(tr("Transform"), AGET(QString, a.ticklabelFormula), ASET(QString, a.ticklabelFormula = v), tr("e.g. $t - 273.15"));
    l->combo(tr("Sign"), {tr("Normal"), tr("Absolute value"), tr("Negate")}, AGET(int, int(a.ticklabelSign)), ASET(int, a.ticklabelSign = TickSign(v)));
    l->section(tr("Text"));
    l->font(tr("Font"), AGET(QString, a.ticklabelText.font), ASET(QString, a.ticklabelText.font = v));
    l->real(tr("Size"), AGET(double, a.ticklabelText.size), ASET(double, a.ticklabelText.size = v), 0.1, 10, 0.05, 2);
    l->color(tr("Colour"), AGET(QColor, a.ticklabelText.color), ASET(QColor, a.ticklabelText.color = v));
    l->real(tr("Angle"), AGET(double, a.ticklabelAngle), ASET(double, a.ticklabelAngle = v), -360, 360, 15, 0, tr("°"));
    l->section(tr("Placement"));
    l->combo(tr("Side"), kSides, AGET(int, int(a.ticklabelSide)), ASET(int, a.ticklabelSide = SideOp(v)));
    l->integer(tr("Skip"), AGET(int, a.ticklabelSkip), ASET(int, a.ticklabelSkip = v), 0, 50);
    l->integer(tr("Stagger"), AGET(int, a.ticklabelStagger), ASET(int, a.ticklabelStagger = v), 0, 10);
    l->check(tr("Automatic offset"), AGET(bool, a.ticklabelOffsetAuto), ASET(bool, a.ticklabelOffsetAuto = v));
    l->real(tr("Offset along"), AGET(double, a.ticklabelOffset.x()), ASET(double, a.ticklabelOffset.setX(v)), -1, 1, 0.005, 3);
    l->real(tr("Offset away"), AGET(double, a.ticklabelOffset.y()), ASET(double, a.ticklabelOffset.setY(v)), -1, 1, 0.005, 3);
    tabs->addTab(scrolled(l), tr("Labels"));
    return w;
}

// ====================================================================== set

QWidget* Inspector::buildSet()
{
    auto* tabs = new QTabWidget;
    tabs->setDocumentMode(true);
    auto valid = [this](const Project& p) {
        const Graph* g = p.graph(m_g);
        return g && g->set(m_s);
    };

    auto* m = new Form(m_doc);
    m_forms << m;
    m->setValidity(valid);
    m->text(tr("Legend"), SGET(QString, s->legend), SSET(QString, s->legend = v), tr("Text, Grace escapes or $math$"));
    m->text(tr("Comment"), SGET(QString, s->comment), SSET(QString, s->comment = v));
    m->combo(tr("Set type"), setTypeNames(), SGET(int, int(s->type)), SSET(int, s->setType(SetType(v))));
    m->check(tr("Hidden"), SGET(bool, s->hidden), SSET(bool, s->hidden = v));
    m_editData = new QPushButton(icons::get(QStringLiteral("table")), tr("Edit data..."), m);
    connect(m_editData, &QPushButton::clicked, this, [this] { emit editDataRequested(m_g, m_s); });
    m->widget(m_editData);
    m->section(tr("Line"));
    m->combo(tr("Connection"), {tr("None"), tr("Straight"), tr("Left stairs"), tr("Right stairs"), tr("Centre stairs"), tr("Segments"), tr("Three-segments")},
             SGET(int, int(s->lineType)), SSET(int, s->lineType = LineType(v)));
    m->lineStyle(tr("Style"), SGET(int, s->line.style), SSET(int, s->line.style = v));
    m->real(tr("Width"), SGET(double, s->line.width), SSET(double, s->line.width = v), 0, 50, 0.25, 2);
    m->color(tr("Colour"), SGET(QColor, s->line.color), SSET(QColor, s->line.color = v));
    m->check(tr("Drop lines"), SGET(bool, s->dropLines), SSET(bool, s->dropLines = v));
    m->check(tr("Draw baseline"), SGET(bool, s->baselineShown), SSET(bool, s->baselineShown = v));
    tabs->addTab(scrolled(m), tr("Set"));

    auto* y = new Form(m_doc);
    m_forms << y;
    y->setValidity(valid);
    y->combo(tr("Symbol"), symbolNames(), SGET(int, int(s->symbol.type)), SSET(int, s->symbol.type = Symbol(v)));
    y->real(tr("Size"), SGET(double, s->symbol.size), SSET(double, s->symbol.size = v), 0, 20, 0.05, 2);
    y->color(tr("Fill colour"), SGET(QColor, s->symbol.fill.color), SSET(QColor, s->symbol.fill.color = v; if (s->symbol.fill.pattern == 0) s->symbol.fill.pattern = 1));
    y->pattern(tr("Fill pattern"), SGET(int, s->symbol.fill.pattern), SSET(int, s->symbol.fill.pattern = v));
    y->color(tr("Edge colour"), SGET(QColor, s->symbol.line.color), SSET(QColor, s->symbol.line.color = v));
    y->real(tr("Edge width"), SGET(double, s->symbol.line.width), SSET(double, s->symbol.line.width = v), 0, 20, 0.1, 2);
    y->lineStyle(tr("Edge style"), SGET(int, s->symbol.line.style), SSET(int, s->symbol.line.style = v));
    y->integer(tr("Draw every"), SGET(int, s->symbol.skip + 1), SSET(int, s->symbol.skip = v - 1), 1, 100000, tr("th point"));
    y->integer(tr("Character code"), SGET(int, s->symbol.ch), SSET(int, s->symbol.ch = v), 32, 0xFFFF);
    y->font(tr("Character font"), SGET(QString, s->symbol.charFont), SSET(QString, s->symbol.charFont = v));
    tabs->addTab(scrolled(y), tr("Symbols"));

    auto* fl = new Form(m_doc);
    m_forms << fl;
    fl->setValidity(valid);
    fl->combo(tr("Fill"), {tr("None"), tr("As polygon"), tr("To baseline")}, SGET(int, int(s->fillType)), SSET(int, s->fillType = FillType(v)));
    fl->combo(tr("Baseline"), {tr("y = 0"), tr("Set minimum"), tr("Set maximum"), tr("Graph minimum"), tr("Graph maximum"), tr("Set average")},
              SGET(int, int(s->baselineType)), SSET(int, s->baselineType = BaselineType(v)));
    fl->color(tr("Colour"), SGET(QColor, s->fill.color), SSET(QColor, s->fill.color = v));
    fl->pattern(tr("Pattern"), SGET(int, s->fill.pattern), SSET(int, s->fill.pattern = v));
    fl->combo(tr("Gradient"), {tr("None"), tr("Vertical"), tr("Horizontal"), tr("Radial")}, SGET(int, int(s->fill.gradient)),
              SSET(int, s->fill.gradient = GradientType(v)));
    fl->color(tr("Gradient end"), SGET(QColor, s->fill.color2), SSET(QColor, s->fill.color2 = v));
    fl->combo(tr("Rule"), {tr("Winding"), tr("Even-odd")}, SGET(int, int(s->fillRule)), SSET(int, s->fillRule = FillRule(v)));
    tabs->addTab(scrolled(fl), tr("Fill"));

    auto* e = new Form(m_doc);
    m_forms << e;
    e->setValidity(valid);
    e->note(tr("Error bars are drawn for set types with error columns (XYDX, XYDY, XYDXDY, BARDY, ...). "
               "Change the set type on the first tab."));
    e->check(tr("Show error bars"), SGET(bool, s->errorbar.active), SSET(bool, s->errorbar.active = v));
    e->combo(tr("Placement"), {tr("Both"), tr("Positive only"), tr("Negative only")}, SGET(int, int(s->errorbar.placement)),
             SSET(int, s->errorbar.placement = ErrorBarPlacement(v)));
    e->color(tr("Colour"), SGET(QColor, s->errorbar.color), SSET(QColor, s->errorbar.color = v));
    e->real(tr("Cap size"), SGET(double, s->errorbar.size), SSET(double, s->errorbar.size = v), 0, 10, 0.1, 2);
    e->real(tr("Cap width"), SGET(double, s->errorbar.width), SSET(double, s->errorbar.width = v), 0, 20, 0.25, 2);
    e->real(tr("Riser width"), SGET(double, s->errorbar.riserWidth), SSET(double, s->errorbar.riserWidth = v), 0, 20, 0.25, 2);
    e->lineStyle(tr("Riser style"), SGET(int, s->errorbar.riserStyle), SSET(int, s->errorbar.riserStyle = v));
    e->check(tr("Clip long risers (arrow)"), SGET(bool, s->errorbar.riserClip), SSET(bool, s->errorbar.riserClip = v));
    e->real(tr("Clip length"), SGET(double, s->errorbar.riserClipLength), SSET(double, s->errorbar.riserClipLength = v), 0, 2, 0.01, 3);
    tabs->addTab(scrolled(e), tr("Errors"));

    auto* av = new Form(m_doc);
    m_forms << av;
    av->setValidity(valid);
    av->check(tr("Annotate points with values"), SGET(bool, s->avalue.active), SSET(bool, s->avalue.active = v));
    av->combo(tr("Show"), {tr("Nothing"), tr("X"), tr("Y"), tr("X, Y"), tr("Point label"), tr("Z")}, SGET(int, s->avalue.type), SSET(int, s->avalue.type = v));
    av->combo(tr("Format"), formatNames(), SGET(int, int(s->avalue.format)), SSET(int, s->avalue.format = LabelFormat(v)));
    av->integer(tr("Precision"), SGET(int, s->avalue.precision), SSET(int, s->avalue.precision = v), 0, 17);
    av->text(tr("Prefix"), SGET(QString, s->avalue.prepend), SSET(QString, s->avalue.prepend = v));
    av->text(tr("Suffix"), SGET(QString, s->avalue.append), SSET(QString, s->avalue.append = v));
    av->font(tr("Font"), SGET(QString, s->avalue.text.font), SSET(QString, s->avalue.text.font = v));
    av->real(tr("Size"), SGET(double, s->avalue.text.size), SSET(double, s->avalue.text.size = v), 0.1, 10, 0.05, 2);
    av->color(tr("Colour"), SGET(QColor, s->avalue.text.color), SSET(QColor, s->avalue.text.color = v));
    av->real(tr("Angle"), SGET(double, s->avalue.angle), SSET(double, s->avalue.angle = v), -360, 360, 15, 0, tr("°"));
    av->real(tr("Offset X"), SGET(double, s->avalue.offset.x()), SSET(double, s->avalue.offset.setX(v)), -1, 1, 0.005, 3);
    av->real(tr("Offset Y"), SGET(double, s->avalue.offset.y()), SSET(double, s->avalue.offset.setY(v)), -1, 1, 0.005, 3);
    tabs->addTab(scrolled(av), tr("Values"));

    auto* cm = new Form(m_doc);
    m_forms << cm;
    cm->setValidity(valid);
    cm->note(tr("Colour mapping applies to XYCOLOR sets (third column = value). Use set type XYCOLOR on the first tab."));
    cm->colormap(tr("Colormap"), SGET(QString, s->colormap), SSET(QString, s->colormap = v; s->colorFromMap = true));
    cm->check(tr("Reverse"), SGET(bool, s->cmapReversed), SSET(bool, s->cmapReversed = v));
    cm->check(tr("Automatic range"), SGET(bool, s->cmapAuto), SSET(bool, s->cmapAuto = v));
    cm->real(tr("Minimum"), SGET(double, s->cmapMin), SSET(double, s->cmapMin = v; s->cmapAuto = false), -1e300, 1e300, 0.1, 6);
    cm->real(tr("Maximum"), SGET(double, s->cmapMax), SSET(double, s->cmapMax = v; s->cmapAuto = false), -1e300, 1e300, 0.1, 6);
    cm->check(tr("Show colour bar"), SGET(bool, s->showColorBar), SSET(bool, s->showColorBar = v));
    cm->check(tr("Third column is a Grace colour index"), SGET(bool, !s->colorFromMap), SSET(bool, s->colorFromMap = !v));
    tabs->addTab(scrolled(cm), tr("Colormap"));
    return tabs;
}

// ====================================================================== objects

QWidget* Inspector::buildText()
{
    auto* f = new Form(m_doc);
    m_forms << f;
    f->setValidity([this](const Project& p) { return m_o >= 0 && m_o < p.objects.size(); });
    f->multiText(tr("Text"), OGET(QString, o.text), OSET(QString, o.text = v));
    f->note(tr("Use $...$ for math (x^2, \\alpha, \\sqrt{x}) or Grace escapes (\\S super, \\s sub, \\x Symbol, \\f{font})."));
    f->font(tr("Font"), OGET(QString, o.textStyle.font), OSET(QString, o.textStyle.font = v));
    f->real(tr("Size"), OGET(double, o.textStyle.size), OSET(double, o.textStyle.size = v), 0.1, 20, 0.05, 2);
    f->color(tr("Colour"), OGET(QColor, o.textStyle.color), OSET(QColor, o.textStyle.color = v));
    f->real(tr("Angle"), OGET(double, o.angle), OSET(double, o.angle = v), -360, 360, 5, 1, tr("°"));
    f->combo(tr("Alignment"), {tr("Left"), tr("Right"), tr("Centre")}, OGET(int, o.just & 3), OSET(int, o.just = (o.just & ~3) | v));
    f->combo(tr("Vertical"), {tr("Baseline"), tr("Bottom"), tr("Top"), tr("Middle")}, OGET(int, (o.just & 12) / 4), OSET(int, o.just = (o.just & 3) | (v * 4)));
    f->section(tr("Position"));
    f->combo(tr("Coordinates"), {tr("World (moves with data)"), tr("Viewport (fixed on page)")}, OGET(int, int(o.loctype)), OSET(int, o.loctype = LocType(v)));
    f->real(tr("X"), OGET(double, o.pos.x()), OSET(double, o.pos.setX(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("Y"), OGET(double, o.pos.y()), OSET(double, o.pos.setY(v)), -1e300, 1e300, 0.01, 4);
    return scrolled(f);
}

QWidget* Inspector::buildLine()
{
    auto* f = new Form(m_doc);
    m_forms << f;
    f->setValidity([this](const Project& p) { return m_o >= 0 && m_o < p.objects.size(); });
    f->lineStyle(tr("Style"), OGET(int, o.line.style), OSET(int, o.line.style = v));
    f->real(tr("Width"), OGET(double, o.line.width), OSET(double, o.line.width = v), 0, 50, 0.25, 2);
    f->color(tr("Colour"), OGET(QColor, o.line.color), OSET(QColor, o.line.color = v));
    f->section(tr("Arrow heads"));
    f->combo(tr("Arrows"), {tr("None"), tr("At start"), tr("At end"), tr("Both ends")}, OGET(int, o.arrow), OSET(int, o.arrow = v));
    f->combo(tr("Shape"), {tr("Line"), tr("Filled"), tr("Opaque")}, OGET(int, o.arrowType), OSET(int, o.arrowType = v));
    f->real(tr("Length"), OGET(double, o.arrowLength), OSET(double, o.arrowLength = v), -20, 20, 0.25, 2);
    f->real(tr("Width (d/L)"), OGET(double, o.arrowDL), OSET(double, o.arrowDL = v), 0, 5, 0.05, 2);
    f->real(tr("Inset (l/L)"), OGET(double, o.arrowLL), OSET(double, o.arrowLL = v), -2, 2, 0.05, 2);
    f->section(tr("Position"));
    f->combo(tr("Coordinates"), {tr("World"), tr("Viewport")}, OGET(int, int(o.loctype)), OSET(int, o.loctype = LocType(v)));
    f->real(tr("X1"), OGET(double, o.p1.x()), OSET(double, o.p1.setX(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("Y1"), OGET(double, o.p1.y()), OSET(double, o.p1.setY(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("X2"), OGET(double, o.p2.x()), OSET(double, o.p2.setX(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("Y2"), OGET(double, o.p2.y()), OSET(double, o.p2.setY(v)), -1e300, 1e300, 0.01, 4);
    return scrolled(f);
}

QWidget* Inspector::buildShape()
{
    auto* f = new Form(m_doc);
    m_forms << f;
    f->setValidity([this](const Project& p) { return m_o >= 0 && m_o < p.objects.size(); });
    f->lineStyle(tr("Border"), OGET(int, o.line.style), OSET(int, o.line.style = v));
    f->real(tr("Border width"), OGET(double, o.line.width), OSET(double, o.line.width = v), 0, 50, 0.25, 2);
    f->color(tr("Border colour"), OGET(QColor, o.line.color), OSET(QColor, o.line.color = v));
    f->pattern(tr("Fill pattern"), OGET(int, o.fill.pattern), OSET(int, o.fill.pattern = v));
    f->color(tr("Fill colour"), OGET(QColor, o.fill.color), OSET(QColor, o.fill.color = v; if (o.fill.pattern == 0) o.fill.pattern = 1));
    f->real(tr("Rotation"), OGET(double, o.rotation), OSET(double, o.rotation = v), -360, 360, 5, 1, tr("°"));
    f->section(tr("Position"));
    f->combo(tr("Coordinates"), {tr("World"), tr("Viewport")}, OGET(int, int(o.loctype)), OSET(int, o.loctype = LocType(v)));
    f->real(tr("X1"), OGET(double, o.p1.x()), OSET(double, o.p1.setX(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("Y1"), OGET(double, o.p1.y()), OSET(double, o.p1.setY(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("X2"), OGET(double, o.p2.x()), OSET(double, o.p2.setX(v)), -1e300, 1e300, 0.01, 4);
    f->real(tr("Y2"), OGET(double, o.p2.y()), OSET(double, o.p2.setY(v)), -1e300, 1e300, 0.01, 4);
    return scrolled(f);
}

}  // namespace cactus::ui
