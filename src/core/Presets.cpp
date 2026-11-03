// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/Presets.h"

#include "core/color/Colors.h"

namespace cactus::presets {

namespace {

struct Spec {
    QString font = QStringLiteral("Helvetica");
    QColor ink{34, 34, 34};          // text
    QColor axis{51, 51, 51};         // frame, bars, ticks
    QColor page{Qt::white};
    QColor gridColor{230, 230, 230};
    bool grid = true;
    bool minorGrid = false;
    TickDirection tickDir = TickDirection::Out;
    SideOp tickSide = SideOp::Normal;
    double majorSize = 0.55, minorSize = 0.3;
    double tickLabelSize = 0.78, labelSize = 0.9;
    double titleSize = 1.15, subtitleSize = 0.85;
    double frameWidth = 1.0;
    FrameType frame = FrameType::Closed;
    double lineWidth = 2.0;
    double symbolSize = 0.6;
    bool symbolEdgeContrast = true;  // thin page-coloured edge around filled symbols
    bool legendBox = false;
    double legendRadius = 0.008;
    QColor legendFill{255, 255, 255, 215};
    bool pretty = true;
    bool smartMinor = true;
    int fillAlpha = 70;
    QString palette = QStringLiteral("CactusPlot Modern");
};

Spec spec(const QString& name)
{
    Spec s;
    const QString n = name.toLower();
    if (n == QLatin1String("publication")) {
        s.font = QStringLiteral("Times-Roman");
        s.ink = QColor(Qt::black);
        s.axis = QColor(Qt::black);
        s.grid = false;
        s.tickDir = TickDirection::In;
        s.tickSide = SideOp::Both;
        s.majorSize = 0.6;
        s.minorSize = 0.32;
        s.tickLabelSize = 0.85;
        s.labelSize = 0.95;
        s.lineWidth = 1.6;
        s.symbolEdgeContrast = false;
        s.legendBox = false;
        s.legendRadius = 0.0;
        s.legendFill = QColor(255, 255, 255, 0);
        s.palette = QStringLiteral("Okabe-Ito");
    } else if (n == QLatin1String("presentation")) {
        s.tickLabelSize = 1.0;
        s.labelSize = 1.2;
        s.titleSize = 1.5;
        s.subtitleSize = 1.05;
        s.lineWidth = 3.4;
        s.symbolSize = 0.85;
        s.frameWidth = 1.6;
        s.majorSize = 0.7;
        s.minorSize = 0.4;
        s.palette = QStringLiteral("Tol Bright");
    } else if (n == QLatin1String("dark")) {
        s.ink = QColor(230, 230, 235);
        s.axis = QColor(190, 190, 200);
        s.page = QColor(30, 31, 38);
        s.gridColor = QColor(58, 60, 72);
        s.legendFill = QColor(40, 42, 52, 220);
        s.palette = QStringLiteral("Tol Vibrant");
    } else if (n == QLatin1String("minimal")) {
        s.grid = false;
        s.frame = FrameType::HalfOpen;
        s.axis = QColor(110, 110, 110);
        s.ink = QColor(60, 60, 60);
        s.majorSize = 0.45;
        s.minorSize = 0.0;
        s.legendFill = QColor(255, 255, 255, 0);
        s.palette = QStringLiteral("Tableau 10");
    } else if (n == QLatin1String("journal (single column)")) {
        s.font = QStringLiteral("Helvetica");
        s.grid = false;
        s.tickDir = TickDirection::In;
        s.tickSide = SideOp::Both;
        s.tickLabelSize = 1.0;
        s.labelSize = 1.1;
        s.lineWidth = 2.2;
        s.symbolEdgeContrast = false;
        s.legendFill = QColor(255, 255, 255, 0);
        s.palette = QStringLiteral("Okabe-Ito");
    }
    return s;
}

bool isGrace(const QString& name) { return name.compare(QLatin1String("Grace Classic"), Qt::CaseInsensitive) == 0; }

}  // namespace

QStringList names()
{
    return {QStringLiteral("Modern"),       QStringLiteral("Publication"), QStringLiteral("Presentation"),
            QStringLiteral("Dark"),         QStringLiteral("Minimal"),     QStringLiteral("Journal (single column)"),
            QStringLiteral("Grace Classic")};
}

QString defaultName() { return QStringLiteral("Modern"); }

QString description(const QString& name)
{
    const QString n = name.toLower();
    if (n == QLatin1String("modern")) return QStringLiteral("Clean sans-serif look, light grid, colour-blind safe colours");
    if (n == QLatin1String("publication")) return QStringLiteral("Serif fonts, inward ticks on all sides, no grid");
    if (n == QLatin1String("presentation")) return QStringLiteral("Large fonts and thick lines for slides");
    if (n == QLatin1String("dark")) return QStringLiteral("Light-on-dark styling for screens and dark slides");
    if (n == QLatin1String("minimal")) return QStringLiteral("Open frame, no grid, understated axes");
    if (n == QLatin1String("journal (single column)")) return QStringLiteral("Large relative text for figures shrunk to one column");
    if (isGrace(name)) return QStringLiteral("Reproduces the xmgrace default look");
    return QString();
}

QVector<QColor> presetPalette(const QString& preset)
{
    if (isGrace(preset)) {
        QVector<QColor> v;
        for (int i = 1; i < 16; ++i) v.push_back(colors::graceColor(i));
        return v;
    }
    const Palette* p = colors::palette(spec(preset).palette);
    return p ? p->colors : colors::defaultCycle();
}

void graceGraphDefaults(Graph& g)
{
    const bool active = g.active;
    auto sets = g.sets;
    g = Graph();
    g.active = active;
    g.sets = sets;
    g.smartMinorTicks = false;
    g.title.style = TextStyle{QStringLiteral("Times-Roman"), 1.5, QColor(Qt::black)};
    g.subtitle.style = TextStyle{QStringLiteral("Times-Roman"), 1.0, QColor(Qt::black)};
    for (int a = 0; a < 4; ++a) {
        Axis& ax = g.axes[a];
        ax = Axis();
        ax.active = a < 2;
        ax.labelText.font = QStringLiteral("Times-Roman");
        ax.ticklabelText.font = QStringLiteral("Times-Roman");
        ax.major = (a % 2 == 0) ? 0.2 : 0.2;
        ax.minorTicks = 1;
        ax.precision = 5;
        ax.tickSide = SideOp::Both;
        ax.tickDir = TickDirection::In;
    }
    g.legend = Legend();
    g.legend.autoPlace = false;
    g.legend.text.font = QStringLiteral("Times-Roman");
    g.frame = Frame();
}

void graceSetDefaults(DataSet& s, int index)
{
    const QColor c = colors::graceColor((index % 15) + 1);
    s.symbol = SymbolStyle();
    s.symbol.line.color = c;
    s.symbol.fill = FillStyle{c, 0};
    s.symbol.charFont = QStringLiteral("Times-Roman");
    s.line = LineStyle{c, 1.0, 1};
    s.lineType = LineType::Straight;
    s.fill = FillStyle{c, 1};
    s.fillType = FillType::None;
    s.errorbar = ErrorBarStyle();
    s.errorbar.color = c;
    s.avalue = AnnotatedValues();
    s.avalue.text.font = QStringLiteral("Times-Roman");
    s.colorFromMap = false;
}

void styleGraph(Graph& g, const QString& preset)
{
    if (isGrace(preset)) {
        const auto keepTitle = g.title.text, keepSub = g.subtitle.text;
        std::array<QString, 4> labels;
        std::array<bool, 4> act;
        for (int a = 0; a < 4; ++a) { labels[a] = g.axes[a].label; act[a] = g.axes[a].active; }
        const double wx0 = g.wxmin, wx1 = g.wxmax, wy0 = g.wymin, wy1 = g.wymax;
        const double vx0 = g.vxmin, vx1 = g.vxmax, vy0 = g.vymin, vy1 = g.vymax;
        const ScaleType xs = g.xscale, ys = g.yscale;
        const GraphType t = g.type;
        graceGraphDefaults(g);
        g.title.text = keepTitle;
        g.subtitle.text = keepSub;
        for (int a = 0; a < 4; ++a) { g.axes[a].label = labels[a]; g.axes[a].active = act[a]; }
        g.wxmin = wx0; g.wxmax = wx1; g.wymin = wy0; g.wymax = wy1;
        g.vxmin = vx0; g.vxmax = vx1; g.vymin = vy0; g.vymax = vy1;
        g.xscale = xs; g.yscale = ys;
        g.type = t;
        g.autoticks();
        return;
    }
    const Spec s = spec(preset);
    g.smartMinorTicks = s.smartMinor;
    g.title.style = TextStyle{s.font, s.titleSize, s.ink};
    g.subtitle.style = TextStyle{s.font, s.subtitleSize, colors::mix(s.ink, s.page, 0.75)};
    g.frame.type = s.frame;
    g.frame.line = LineStyle{s.axis, s.frameWidth, 1};
    g.frame.background = FillStyle{s.page, 0};
    for (int a = 0; a < 4; ++a) {
        Axis& ax = g.axes[a];
        ax.bar = LineStyle{s.axis, s.frameWidth, 1};
        ax.barOn = true;
        ax.tickDir = s.tickDir;
        ax.tickSide = s.tickSide;
        ax.majorProps.size = s.majorSize;
        ax.majorProps.line = LineStyle{s.axis, s.frameWidth, 1};
        ax.majorProps.grid = s.grid && a < 2;
        ax.majorProps.gridLine = LineStyle{s.gridColor, 0.8, 1};
        ax.minorProps.size = s.minorSize;
        ax.minorProps.line = LineStyle{s.axis, s.frameWidth * 0.8, s.minorSize > 0 ? 1 : 0};
        ax.minorProps.grid = s.minorGrid && a < 2;
        ax.minorProps.gridLine = LineStyle{colors::mix(s.gridColor, s.page, 0.5), 0.6, 1};
        ax.ticklabelText = TextStyle{s.font, s.tickLabelSize, s.ink};
        ax.ticklabelStagger = 0;
        ax.ticklabelOffsetAuto = true;
        ax.labelPlaceAuto = true;
        ax.labelText = TextStyle{s.font, s.labelSize, s.ink};
        ax.format = LabelFormat::General;
        ax.precision = 6;
    }
    g.legend.text = TextStyle{s.font, s.tickLabelSize * 0.95, s.ink};
    g.legend.box = LineStyle{s.axis, 0.8, s.legendBox ? 1 : 0};
    g.legend.boxFill = FillStyle{s.legendFill, 1};
    g.legend.cornerRadius = s.legendRadius;
    g.legend.length = 3;
    g.legend.autoPlace = true;
    g.autoticks();
}

void styleSet(DataSet& s, int index, const QString& preset)
{
    if (isGrace(preset)) {
        const Symbol keepSym = s.symbol.type;
        const LineType keepLt = s.lineType;
        graceSetDefaults(s, index);
        s.symbol.type = keepSym;
        s.lineType = keepLt;
        return;
    }
    const Spec sp = spec(preset);
    const QVector<QColor> pal = presetPalette(preset);
    const QColor c = pal.isEmpty() ? QColor(Qt::black) : pal[index % pal.size()];
    s.line = LineStyle{c, sp.lineWidth, s.line.style > 0 ? s.line.style : 1};
    const bool bars = s.type == SetType::BAR || s.type == SetType::BARDY || s.type == SetType::BARDYDY || s.type == SetType::XYBOXPLOT;
    if (bars) {
        // bar width is the symbol size: keep it, use a solid fill with a slightly darker edge
        s.symbol.size = std::max(s.symbol.size, 0.8);
        s.symbol.fill = FillStyle{c, 1};
        s.symbol.line = LineStyle{colors::mix(c, QColor(Qt::black), 0.75), 0.6, 1};
    } else {
        s.symbol.size = sp.symbolSize;
        s.symbol.fill = FillStyle{c, 1};
        s.symbol.line = sp.symbolEdgeContrast ? LineStyle{sp.page, 0.7, 1} : LineStyle{c, 1.0, 1};
    }
    s.symbol.charFont = sp.font;
    QColor fc = c;
    fc.setAlpha(sp.fillAlpha);
    s.fill = FillStyle{fc, 1};
    s.errorbar.color = c;
    s.errorbar.width = std::max(1.0, sp.lineWidth * 0.6);
    s.errorbar.riserWidth = std::max(1.0, sp.lineWidth * 0.6);
    s.errorbar.size = 0.8;
    s.avalue.text = TextStyle{sp.font, sp.tickLabelSize * 0.85, sp.ink};
}

void applyToProject(Project& p, const QString& preset)
{
    p.stylePreset = preset;
    p.prettyNumbers = !isGrace(preset);
    const Spec s = spec(preset);
    p.page.background = FillStyle{isGrace(preset) ? QColor(Qt::white) : s.page, 1};
    p.page.backgroundFill = true;
    for (Graph& g : p.graphs) {
        if (!g.active) continue;
        styleGraph(g, preset);
        int k = 0;
        for (DataSet& d : g.sets) {
            if (!d.active) continue;
            styleSet(d, k++, preset);
        }
    }
    for (DrawObject& o : p.objects) {
        if (o.kind == DrawObjectKind::Text) {
            o.textStyle.color = isGrace(preset) ? QColor(Qt::black) : s.ink;
            if (!isGrace(preset)) o.textStyle.font = s.font;
        } else {
            o.line.color = isGrace(preset) ? QColor(Qt::black) : s.axis;
        }
    }
}

void applyPalette(Project& p, const QString& paletteName, int graph)
{
    const Palette* pal = colors::palette(paletteName);
    if (!pal || pal->colors.isEmpty()) return;
    for (int gi = 0; gi < p.graphs.size(); ++gi) {
        if (graph >= 0 && gi != graph) continue;
        Graph& g = p.graphs[gi];
        int k = 0;
        for (DataSet& d : g.sets) {
            if (!d.active) continue;
            const QColor c = pal->colors[k++ % pal->colors.size()];
            d.line.color = c;
            d.symbol.fill.color = c;
            if (d.symbol.line.color != p.page.background.color) d.symbol.line.color = c;
            QColor fc = c;
            fc.setAlpha(d.fill.color.alpha());
            d.fill.color = fc;
            d.errorbar.color = c;
        }
    }
}

Project newProject(const QString& preset)
{
    Project p;
    Graph& g = p.ensureGraph(0);
    g.vxmin = 0.15;
    g.vxmax = 1.20;
    g.vymin = 0.14;
    g.vymax = 0.88;
    g.axes[Graph::AltX].active = false;
    g.axes[Graph::AltY].active = false;
    g.wxmin = 0; g.wxmax = 1; g.wymin = 0; g.wymax = 1;
    applyToProject(p, preset);
    return p;
}

}  // namespace cactus::presets
