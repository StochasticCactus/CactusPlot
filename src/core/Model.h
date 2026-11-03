// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - the document model.
//
// The model is a tree of value types. Containers use Qt's implicitly shared
// classes, so copying a whole Project is cheap (copy-on-write). The undo
// stack relies on this: every undoable change stores a snapshot.
//
// Units follow Grace so that .agr files round-trip:
//  * page size in points (1/72 inch);
//  * "viewport" coordinates are normalised so that the shorter page side is 1;
//  * text sizes are relative (1.0 = 0.028 x shorter page side);
//  * line widths are relative (1.0 = 0.0015 x shorter page side, ~0.92 pt on letter).
#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <array>
#include <limits>

#include "core/Types.h"

namespace cactus {

// ---------------------------------------------------------------- styles

struct LineStyle {
    QColor color{Qt::black};
    double width = 1.0;
    int style = 1;  ///< 0 none, 1 solid, 2.. dash patterns (Grace numbering)
    bool visible() const { return style > 0 && width >= 0.0 && color.alpha() > 0; }
    bool operator==(const LineStyle&) const = default;
};

enum class GradientType { None = 0, Vertical, Horizontal, Radial };

struct FillStyle {
    QColor color{Qt::black};
    int pattern = 1;  ///< 0 none, 1 solid, 2..31 Grace patterns
    GradientType gradient = GradientType::None;
    QColor color2{Qt::white};  ///< gradient end colour
    bool visible() const { return pattern > 0 && color.alpha() > 0; }
    bool operator==(const FillStyle&) const = default;
};

struct TextStyle {
    QString font{QStringLiteral("Helvetica")};
    double size = 1.0;
    QColor color{Qt::black};
    bool operator==(const TextStyle&) const = default;
};

struct SymbolStyle {
    Symbol type = Symbol::None;
    double size = 1.0;
    LineStyle line;
    FillStyle fill{QColor(Qt::black), 0};
    int ch = 65;
    QString charFont{QStringLiteral("Helvetica")};
    int skip = 0;
    bool operator==(const SymbolStyle&) const = default;
};

// ---------------------------------------------------------------- sets

struct AnnotatedValues {
    bool active = false;
    int type = 2;  ///< 0 none, 1 X, 2 Y, 3 XY, 4 string, 5 Z
    TextStyle text{QStringLiteral("Helvetica"), 1.0, QColor(Qt::black)};
    double angle = 0.0;
    LabelFormat format = LabelFormat::General;
    int precision = 3;
    QString prepend, append;
    QPointF offset{0.0, 0.0};
    bool operator==(const AnnotatedValues&) const = default;
};

struct ErrorBarStyle {
    bool active = true;
    ErrorBarPlacement placement = ErrorBarPlacement::Both;
    QColor color{Qt::black};
    int pattern = 1;
    double size = 1.0;  ///< cap length
    double width = 1.0;
    int style = 1;
    double riserWidth = 1.0;
    int riserStyle = 1;
    bool riserClip = false;
    double riserClipLength = 0.1;
    bool operator==(const ErrorBarStyle&) const = default;
};

struct DataSet {
    bool active = false;  ///< slot is allocated
    bool hidden = false;
    SetType type = SetType::XY;
    std::array<QVector<double>, 6> cols;  ///< X, Y, Y1..Y4
    QStringList strings;                   ///< optional per-point labels

    SymbolStyle symbol;
    LineType lineType = LineType::Straight;
    LineStyle line;
    FillType fillType = FillType::None;
    FillRule fillRule = FillRule::Winding;
    FillStyle fill{QColor(Qt::black), 1};
    BaselineType baselineType = BaselineType::Zero;
    bool baselineShown = false;
    bool dropLines = false;
    AnnotatedValues avalue;
    ErrorBarStyle errorbar;
    QString comment;
    QString legend;
    // XYCOLOR / XYZ colour mapping (CactusPlot extension)
    QString colormap{QStringLiteral("viridis")};
    double cmapMin = 0.0, cmapMax = 1.0;
    bool cmapAuto = true;
    bool cmapReversed = false;
    bool colorFromMap = true;  ///< XYCOLOR: colour column through colormap (false: Grace colour index)
    bool showColorBar = false; ///< draw a colour bar next to the graph

    int size() const { return int(cols[0].size()); }
    int columns() const { return setTypeColumns(type); }
    QVector<double>& x() { return cols[0]; }
    QVector<double>& y() { return cols[1]; }
    const QVector<double>& x() const { return cols[0]; }
    const QVector<double>& y() const { return cols[1]; }
    /// Resize all used columns to n points (new values are 0).
    void resize(int n);
    /// Change the set type, padding/truncating columns.
    void setType(SetType t);
    void clearData();
    /// Bounding box of x/y (including error bars when withErrors).
    bool bounds(double* xmin, double* xmax, double* ymin, double* ymax, bool withErrors = true) const;
};

// ---------------------------------------------------------------- axes

struct TickMark {
    double value = 0.0;
    bool major = true;
    QString label;  ///< custom label for special ticks
};

struct TickProps {
    double size = 1.0;
    LineStyle line;
    bool grid = false;
    LineStyle gridLine{QColor(220, 220, 220), 1.0, 1};
    bool operator==(const TickProps&) const = default;
};

struct Axis {
    bool active = true;
    bool zero = false;          ///< draw at the zero of the other coordinate
    QPointF offset{0.0, 0.0};   ///< (normal side, opposite side) offsets in viewport units
    bool barOn = true;
    LineStyle bar;
    // label
    QString label;
    LabelLayout labelLayout = LabelLayout::Parallel;
    bool labelPlaceAuto = true;
    QPointF labelOffset{0.0, 0.08};
    TextStyle labelText{QStringLiteral("Helvetica"), 1.0, QColor(Qt::black)};
    SideOp labelSide = SideOp::Normal;
    // ticks
    bool tickOn = true;
    double major = 0.2;     ///< major tick spacing (factor for log scale)
    int minorTicks = 1;     ///< minor ticks between majors
    int autoTickCount = 6;  ///< target number of major ticks for auto ticking
    bool placeRounded = true;
    TickDirection tickDir = TickDirection::In;
    TickProps majorProps;
    TickProps minorProps{0.5};
    SideOp tickSide = SideOp::Both;
    bool tickAuto = true;      ///< false: use special ticks
    QVector<TickMark> specTicks;
    // tick labels
    bool ticklabelOn = true;
    int precision = 5;
    LabelFormat format = LabelFormat::General;
    QString prepend, append;
    double ticklabelAngle = 0.0;
    int ticklabelSkip = 0;
    int ticklabelStagger = 0;
    SideOp ticklabelSide = SideOp::Normal;
    bool ticklabelOffsetAuto = true;
    QPointF ticklabelOffset{0.0, 0.01};
    TickSign ticklabelSign = TickSign::Normal;
    bool ticklabelStartAuto = true;
    double ticklabelStart = 0.0;
    bool ticklabelStopAuto = true;
    double ticklabelStop = 0.0;
    TextStyle ticklabelText{QStringLiteral("Helvetica"), 1.0, QColor(Qt::black)};
    QString ticklabelFormula;  ///< Grace "ticklabel formula" (transform of tick value)
    bool ticklabelAuto = true; ///< false: use special tick labels
};

// ---------------------------------------------------------------- graph pieces

struct Legend {
    bool active = true;
    LocType loctype = LocType::View;
    QPointF pos{0.85, 0.8};     ///< upper-left corner
    bool autoPlace = true;       ///< CactusPlot: choose a free corner automatically
    TextStyle text{QStringLiteral("Helvetica"), 1.0, QColor(Qt::black)};
    LineStyle box{QColor(Qt::black), 1.0, 1};
    FillStyle boxFill{QColor(Qt::white), 1};
    double cornerRadius = 0.0;   ///< CactusPlot: rounded legend box (viewport units)
    int length = 4;  ///< sample line length
    int vgap = 1;
    int hgap = 1;
    bool invert = false;
};

struct Frame {
    FrameType type = FrameType::Closed;
    LineStyle line;
    FillStyle background{QColor(Qt::white), 0};
};

struct TitleText {
    QString text;
    TextStyle style;
};

/// A locator/fixed point (Grace "fixedpoint").
struct FixedPoint {
    bool active = false;
    int type = 0;
    QPointF xy{0.0, 0.0};
    LabelFormat formatX = LabelFormat::General, formatY = LabelFormat::General;
    int precX = 6, precY = 6;
};

struct Graph {
    bool active = false;
    bool hidden = false;
    GraphType type = GraphType::XY;
    bool stacked = false;
    double barHGap = 0.0;
    double znorm = 1.0;
    bool smartMinorTicks = true;  ///< CactusPlot: choose minor tick count from the major spacing
    double wxmin = 0.0, wxmax = 1.0, wymin = 0.0, wymax = 1.0;
    double vxmin = 0.15, vxmax = 1.15, vymin = 0.15, vymax = 0.85;
    ScaleType xscale = ScaleType::Linear, yscale = ScaleType::Linear;
    bool xinvert = false, yinvert = false;
    TitleText title{QString(), TextStyle{QStringLiteral("Helvetica"), 1.5, QColor(Qt::black)}};
    TitleText subtitle{QString(), TextStyle{QStringLiteral("Helvetica"), 1.0, QColor(Qt::black)}};
    std::array<Axis, 4> axes;  ///< x, y, alt-x, alt-y
    Legend legend;
    Frame frame;
    FixedPoint fixedPoint;
    QVector<DataSet> sets;

    enum AxisIndex { X = 0, Y = 1, AltX = 2, AltY = 3 };

    Graph()
    {
        axes[AltX].active = false;
        axes[AltY].active = false;
    }

    DataSet* set(int i) { return (i >= 0 && i < sets.size() && sets[i].active) ? &sets[i] : nullptr; }
    const DataSet* set(int i) const { return (i >= 0 && i < sets.size() && sets[i].active) ? &sets[i] : nullptr; }
    /// Returns the set at index i, allocating (and activating) it if needed.
    DataSet& ensureSet(int i);
    /// First free (inactive) set slot.
    int nextFreeSet() const;
    /// First set without data (inactive, or allocated by property statements only) - Grace semantics.
    int nextEmptySet() const;
    int activeSetCount() const;
    /// Autoscale world coordinates to the active sets (axes: 1 = x, 2 = y, 3 = both).
    void autoscale(int which = 3, int onlySet = -1);
    /// Recompute automatic tick spacing for all axes.
    void autoticks();
};

// ---------------------------------------------------------------- annotations

enum class DrawObjectKind { Text = 0, Line, Box, Ellipse };

struct DrawObject {
    DrawObjectKind kind = DrawObjectKind::Text;
    bool active = true;
    LocType loctype = LocType::View;
    int graph = 0;  ///< graph used for world coordinates
    // Text
    QPointF pos{0.5, 0.5};
    QString text;
    TextStyle textStyle;
    double angle = 0.0;
    int just = 0;  ///< Grace justification code
    // Line / Box / Ellipse
    QPointF p1{0.0, 0.0}, p2{0.1, 0.1};
    LineStyle line;
    FillStyle fill{QColor(Qt::white), 0};
    int arrow = 0;          ///< 0 none, 1 start, 2 end, 3 both
    int arrowType = 0;      ///< 0 line, 1 filled, 2 opaque
    double arrowLength = 1.0;
    double arrowDL = 1.0;   ///< layout: width/length ratio
    double arrowLL = 0.0;   ///< layout: inset factor
    double rotation = 0.0;  ///< boxes/ellipses
};

struct Region {
    bool active = false;
    RegionType type = RegionType::Above;
    bool negate = false;  ///< outside polygon / outside range
    LineStyle line;
    QPointF p1{0, 0}, p2{0, 0};
    QVector<QPointF> polygon;
    int linkedGraph = 0;
    /// Is the point (x, y) (world coords) inside the region?
    bool contains(double x, double y) const;
};

struct Page {
    double width = 792.0;   ///< points
    double height = 612.0;  ///< points
    FillStyle background{QColor(Qt::white), 1};
    bool backgroundFill = true;
    /// Viewport units -> points scale (shorter side).
    double unit() const { return std::min(width, height); }
    double viewWidth() const { return width / unit(); }
    double viewHeight() const { return height / unit(); }
};

struct Timestamp {
    bool active = false;
    QPointF pos{0.03, 0.03};
    TextStyle text;
    double angle = 0.0;
    QString value;
};

/// Indexed colour map mirrored for Grace compatibility.
struct ColorMapEntry {
    QColor color;
    QString name;
};

struct Project {
    Page page;
    QVector<Graph> graphs;
    QVector<DrawObject> objects;
    std::array<Region, 5> regions;
    Timestamp timestamp;
    QString description;
    QVector<ColorMapEntry> colorMap;  ///< Grace colour indices
    QStringList fontMap;              ///< Grace font indices
    double referenceDate = 0.0;       ///< Julian date of value 0 for date formats
    bool dateWrap = false;
    int dateWrapYear = 1950;
    QString stylePreset{QStringLiteral("Modern")};
    bool prettyNumbers = true;  ///< Unicode minus and x10^n tick labels
    int currentGraph = 0;

    Project();
    Graph* graph(int i) { return (i >= 0 && i < graphs.size() && graphs[i].active) ? &graphs[i] : nullptr; }
    const Graph* graph(int i) const { return (i >= 0 && i < graphs.size() && graphs[i].active) ? &graphs[i] : nullptr; }
    Graph& ensureGraph(int i);
    int activeGraphCount() const;
    /// Resolve a Grace colour index through the colour map.
    QColor colorAt(int index) const;
    /// Find (or append) the colour map index for a colour.
    int colorIndex(const QColor& c);
    int colorIndexConst(const QColor& c) const;
    QString fontAt(int index) const;
    int fontIndex(const QString& name);
};

/// Arrange graphs into a rows x cols matrix (Grace ARRANGE).
void arrangeGraphs(Project& p, int rows, int cols, double offset, double hgap, double vgap, bool hvinv = false,
                   bool hinv = false, bool vinv = false, bool snake = false);

}  // namespace cactus
