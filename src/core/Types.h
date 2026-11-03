// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - core enumerations shared by the model, renderer and Grace layer.
#pragma once

#include <QString>
#include <QStringList>

namespace cactus {

enum class SetType {
    XY = 0,
    XYDX,
    XYDY,
    XYDXDX,
    XYDYDY,
    XYDXDY,
    XYDXDXDYDY,
    BAR,
    BARDY,
    BARDYDY,
    XYHILO,
    XYZ,
    XYR,
    XYSIZE,
    XYCOLOR,
    XYCOLPAT,
    XYVMAP,
    XYBOXPLOT,
    Count
};

/// Number of data columns for a set type.
int setTypeColumns(SetType t);
/// Grace name (lower case) of a set type, e.g. "xydy".
QString setTypeName(SetType t);
/// Parse a Grace set type name; returns false if unknown.
bool parseSetType(const QString& s, SetType* out);
/// Human readable column labels ("X", "Y", "dY", ...).
QStringList setTypeColumnNames(SetType t);
bool setTypeHasErrorBars(SetType t);

enum class GraphType { XY = 0, Chart, Polar, Smith, Fixed, Pie };
QString graphTypeName(GraphType t);
bool parseGraphType(const QString& s, GraphType* out);

enum class ScaleType { Linear = 0, Log, Reciprocal, Logit };
QString scaleTypeName(ScaleType t);
bool parseScaleType(const QString& s, ScaleType* out);

/// Connecting line type of a set (Grace "line type").
enum class LineType { None = 0, Straight, LeftStairs, RightStairs, CenterStairs, Segments, ThreeSegments };

enum class Symbol {
    None = 0,
    Circle,
    Square,
    Diamond,
    TriangleUp,
    TriangleLeft,
    TriangleDown,
    TriangleRight,
    Plus,
    Cross,
    Star,
    Char,
    // CactusPlot extensions
    Pentagon,
    Hexagon,
    Star5,
    ThinPlus,
    ThinCross,
    Count
};
QString symbolName(Symbol s);

enum class FillType { None = 0, Polygon, ToBaseline };
enum class BaselineType { Zero = 0, SetMin, SetMax, GraphMin, GraphMax, SetAverage };
enum class FillRule { Winding = 0, EvenOdd };

enum class TickDirection { In = 0, Out, Both };
enum class SideOp { Normal = 0, Opposite, Both };
enum class TickSign { Normal = 0, Absolute, Negate };
enum class LabelLayout { Parallel = 0, Perpendicular };

enum class ErrorBarPlacement { Both = 0, Top, Bottom };  // Grace: both/top/bottom (normal/plus/minus)

enum class LocType { World = 0, View };

enum class Justification { Left = 0, Right = 1, Center = 2 };
/// Grace justification also has a vertical component (+4 middle, +8 top...).
enum class VJustification { Baseline = 0, Bottom = 1, Middle = 2, Top = 3 };

enum class FrameType { Closed = 0, HalfOpen, BreakTop, BreakBottom, BreakLeft, BreakRight };

enum class RegionType { Polygon = 0, Above, Below, Left, Right, Horizontal, Vertical };

/// Tick label formats (Grace names).
enum class LabelFormat {
    Decimal = 0,
    Exponential,
    General,
    Power,
    Scientific,
    Engineering,
    Computer,
    DDMMYY,
    MMDDYY,
    YYMMDD,
    MMYY,
    MMDD,
    MonthDay,
    DayMonth,
    Months,
    MonthsY,
    MonthL,
    DayOfWeek,
    DayOfWeekL,
    DayOfYear,
    HMS,
    MMDDHMS,
    MMDDYYHMS,
    YYMMDDHMS,
    DegreesLon,
    DegreesMMLon,
    DegreesMMSSLon,
    MMSSLon,
    DegreesLat,
    DegreesMMLat,
    DegreesMMSSLat,
    MMSSLat,
    Count
};
QString labelFormatName(LabelFormat f);
bool parseLabelFormat(const QString& s, LabelFormat* out);

enum class ArrowType { Line = 0, Filled, Opaque };

}  // namespace cactus
