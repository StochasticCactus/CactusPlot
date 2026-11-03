// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/Types.h"

namespace cactus {

namespace {
struct SetTypeInfo {
    const char* name;
    int cols;
    const char* colNames;
};
const SetTypeInfo kSetTypes[] = {
    {"xy", 2, "X Y"},
    {"xydx", 3, "X Y dX"},
    {"xydy", 3, "X Y dY"},
    {"xydxdx", 4, "X Y dX+ dX-"},
    {"xydydy", 4, "X Y dY+ dY-"},
    {"xydxdy", 4, "X Y dX dY"},
    {"xydxdxdydy", 6, "X Y dX+ dX- dY+ dY-"},
    {"bar", 2, "X Y"},
    {"bardy", 3, "X Y dY"},
    {"bardydy", 4, "X Y dY+ dY-"},
    {"xyhilo", 5, "X High Low Open Close"},
    {"xyz", 3, "X Y Z"},
    {"xyr", 3, "X Y Radius"},
    {"xysize", 3, "X Y Size"},
    {"xycolor", 3, "X Y Color"},
    {"xycolpat", 4, "X Y Color Pattern"},
    {"xyvmap", 4, "X Y VX VY"},
    {"xyboxplot", 6, "X Median Q1 Q3 Min Max"},
};

const char* kGraphTypes[] = {"xy", "chart", "polar", "smith", "fixed", "pie"};
const char* kScaleTypes[] = {"Normal", "Logarithmic", "Reciprocal", "Logit"};
const char* kSymbols[] = {"None",   "Circle", "Square",   "Diamond",   "Triangle up", "Triangle left",
                          "Triangle down", "Triangle right", "Plus", "X", "Star", "Char",
                          "Pentagon", "Hexagon", "Filled star", "Thin plus", "Thin X"};
const char* kFormats[] = {"decimal",     "exponential",   "general",        "power",       "scientific",
                          "engineering", "computing",      "ddmmyy",         "mmddyy",      "yymmdd",
                          "mmyy",        "mmdd",          "monthday",       "daymonth",    "months",
                          "monthsy",     "monthl",        "dayofweek",      "dayofweekl",  "dayofyear",
                          "hms",         "mmddhms",       "mmddyyhms",      "yymmddhms",   "degreeslon",
                          "degreesmmlon", "degreesmmsslon", "mmsslon",      "degreeslat",  "degreesmmlat",
                          "degreesmmsslat", "mmsslat"};
}  // namespace

int setTypeColumns(SetType t)
{
    int i = int(t);
    if (i < 0 || i >= int(SetType::Count)) return 2;
    return kSetTypes[i].cols;
}

QString setTypeName(SetType t)
{
    int i = int(t);
    if (i < 0 || i >= int(SetType::Count)) return QStringLiteral("xy");
    return QString::fromLatin1(kSetTypes[i].name);
}

bool parseSetType(const QString& s, SetType* out)
{
    const QString l = s.trimmed().toLower();
    for (int i = 0; i < int(SetType::Count); ++i) {
        if (l == QLatin1String(kSetTypes[i].name)) {
            *out = SetType(i);
            return true;
        }
    }
    // legacy aliases
    if (l == QLatin1String("xydxdxdydy")) { *out = SetType::XYDXDXDYDY; return true; }
    if (l == QLatin1String("xyboxplot") || l == QLatin1String("xybox")) { *out = SetType::XYBOXPLOT; return true; }
    if (l == QLatin1String("xyhilo") || l == QLatin1String("hilo")) { *out = SetType::XYHILO; return true; }
    return false;
}

QStringList setTypeColumnNames(SetType t)
{
    int i = int(t);
    if (i < 0 || i >= int(SetType::Count)) i = 0;
    return QString::fromLatin1(kSetTypes[i].colNames).split(QLatin1Char(' '));
}

bool setTypeHasErrorBars(SetType t)
{
    switch (t) {
    case SetType::XYDX:
    case SetType::XYDY:
    case SetType::XYDXDX:
    case SetType::XYDYDY:
    case SetType::XYDXDY:
    case SetType::XYDXDXDYDY:
    case SetType::BARDY:
    case SetType::BARDYDY:
        return true;
    default:
        return false;
    }
}

QString graphTypeName(GraphType t) { return QString::fromLatin1(kGraphTypes[int(t)]); }

bool parseGraphType(const QString& s, GraphType* out)
{
    const QString l = s.trimmed().toLower();
    for (int i = 0; i < 6; ++i) {
        if (l == QLatin1String(kGraphTypes[i])) {
            *out = GraphType(i);
            return true;
        }
    }
    // xmgr legacy graph types
    if (l == QLatin1String("logx") || l == QLatin1String("logy") || l == QLatin1String("logxy") ||
        l == QLatin1String("bar") || l == QLatin1String("hbar") || l == QLatin1String("stackedbar") ||
        l == QLatin1String("stackedhbar")) {
        *out = (l.contains(QLatin1String("bar"))) ? GraphType::Chart : GraphType::XY;
        return true;
    }
    return false;
}

QString scaleTypeName(ScaleType t) { return QString::fromLatin1(kScaleTypes[int(t)]); }

bool parseScaleType(const QString& s, ScaleType* out)
{
    const QString l = s.trimmed().toLower();
    if (l == QLatin1String("normal") || l == QLatin1String("linear")) { *out = ScaleType::Linear; return true; }
    if (l.startsWith(QLatin1String("log")) && l != QLatin1String("logit")) { *out = ScaleType::Log; return true; }
    if (l == QLatin1String("reciprocal")) { *out = ScaleType::Reciprocal; return true; }
    if (l == QLatin1String("logit")) { *out = ScaleType::Logit; return true; }
    return false;
}

QString symbolName(Symbol s)
{
    int i = int(s);
    if (i < 0 || i >= int(Symbol::Count)) return QStringLiteral("None");
    return QString::fromLatin1(kSymbols[i]);
}

QString labelFormatName(LabelFormat f)
{
    int i = int(f);
    if (i < 0 || i >= int(LabelFormat::Count)) return QStringLiteral("general");
    return QString::fromLatin1(kFormats[i]);
}

bool parseLabelFormat(const QString& s, LabelFormat* out)
{
    const QString l = s.trimmed().toLower();
    for (int i = 0; i < int(LabelFormat::Count); ++i) {
        if (l == QLatin1String(kFormats[i])) {
            *out = LabelFormat(i);
            return true;
        }
    }
    if (l == QLatin1String("float")) { *out = LabelFormat::Decimal; return true; }
    if (l == QLatin1String("computer")) { *out = LabelFormat::Computer; return true; }
    return false;
}

}  // namespace cactus
