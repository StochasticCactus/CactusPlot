// SPDX-License-Identifier: GPL-2.0-or-later
#include "grace/ProjectIO.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>

#include "core/Presets.h"
#include "core/color/Colors.h"
#include "grace/Interpreter.h"
#include "io/DataReader.h"

namespace cactus::projectio {

namespace {

class Writer {
public:
    Writer(const Project& p, Dialect d) : P(p), D(d), colors(p.colorMap), fonts(p.fontMap) {}

    QString run()
    {
        body();
        QString head;
        head += native() ? QStringLiteral("# CactusPlot project file\n#\n") : QStringLiteral("# Grace project file\n#\n");
        head += QStringLiteral("@version 50125\n");
        head += QStringLiteral("@page size %1, %2\n").arg(num(P.page.width), num(P.page.height));
        head += QStringLiteral("@page scroll 5%\n@page inout 5%\n@link page off\n");
        for (int i = 0; i < fonts.size(); ++i)
            head += QStringLiteral("@map font %1 to %2, %2\n").arg(i).arg(str(fonts[i]));
        for (int i = 0; i < colors.size(); ++i) {
            const QColor& c = colors[i].color;
            QString name = colors[i].name;
            if (name.isEmpty()) name = colors::displayName(QColor(c.rgb()));
            if (native() && c.alpha() != 255)
                head += QStringLiteral("@map color %1 to (%2, %3, %4, %5), %6\n").arg(i).arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()).arg(str(name));
            else
                head += QStringLiteral("@map color %1 to (%2, %3, %4), %5\n").arg(i).arg(c.red()).arg(c.green()).arg(c.blue()).arg(str(name));
        }
        return head + out;
    }

private:
    const Project& P;
    Dialect D;
    QVector<ColorMapEntry> colors;
    QStringList fonts;
    QString out;

    bool native() const { return D == Dialect::Native; }

    static QString num(double v)
    {
        if (!std::isfinite(v)) return v != v ? QStringLiteral("nan") : (v > 0 ? QStringLiteral("inf") : QStringLiteral("-inf"));
        return QString::number(v, 'g', QLocale::FloatingPointShortest);
    }
    static QString fix(double v) { return QString::number(v, 'f', 6); }
    static QString str(const QString& s)
    {
        QString e = s;
        e.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        e.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        return QLatin1Char('"') + e + QLatin1Char('"');
    }
    static QString onoff(bool b) { return b ? QStringLiteral("on") : QStringLiteral("off"); }
    static QString tf(bool b) { return b ? QStringLiteral("true") : QStringLiteral("false"); }

    int color(const QColor& c)
    {
        for (int i = 0; i < colors.size(); ++i) {
            const QColor& m = colors[i].color;
            if (native() ? m.rgba() == c.rgba() : m.rgb() == c.rgb()) return i;
        }
        colors.push_back(ColorMapEntry{native() ? c : QColor(c.rgb()), colors::displayName(QColor(c.rgb()))});
        return int(colors.size()) - 1;
    }
    int font(const QString& name)
    {
        for (int i = 0; i < fonts.size(); ++i) if (fonts[i].compare(name, Qt::CaseInsensitive) == 0) return i;
        fonts.push_back(name);
        return int(fonts.size()) - 1;
    }
    int symbol(Symbol s) const
    {
        if (native()) return int(s);
        switch (s) {
        case Symbol::Pentagon:
        case Symbol::Hexagon: return int(Symbol::Circle);
        case Symbol::Star5: return int(Symbol::Star);
        case Symbol::ThinPlus: return int(Symbol::Plus);
        case Symbol::ThinCross: return int(Symbol::Cross);
        default: return int(s);
        }
    }
    int lineType(LineType t) const
    {
        const int v = Interpreter::lineTypeToGrace(t);
        if (!native() && v == 6) return 3;
        return v;
    }
    void line(const QString& s) { out += QLatin1Char('@') + s + QLatin1Char('\n'); }
    void g(const QString& s) { out += QStringLiteral("@    ") + s + QLatin1Char('\n'); }

    QString side(SideOp s, bool x) const
    {
        switch (s) {
        case SideOp::Normal: return x ? QStringLiteral("bottom") : QStringLiteral("left");
        case SideOp::Opposite: return x ? QStringLiteral("top") : QStringLiteral("right");
        case SideOp::Both: return QStringLiteral("both");
        }
        return QStringLiteral("normal");
    }
    static QString gradientName(GradientType t)
    {
        switch (t) {
        case GradientType::Vertical: return QStringLiteral("vertical");
        case GradientType::Horizontal: return QStringLiteral("horizontal");
        case GradientType::Radial: return QStringLiteral("radial");
        default: return QStringLiteral("none");
        }
    }

    void body()
    {
        if (native()) {
            line(QStringLiteral("cactusplot preset %1").arg(str(P.stylePreset)));
            line(QStringLiteral("cactusplot pretty %1").arg(onoff(P.prettyNumbers)));
            if (P.page.background.gradient != GradientType::None) {
                line(QStringLiteral("cactusplot page gradient %1").arg(gradientName(P.page.background.gradient)));
                line(QStringLiteral("cactusplot page gradient color %1").arg(color(P.page.background.color2)));
            }
        }
        line(QStringLiteral("reference date %1").arg(num(P.referenceDate)));
        line(QStringLiteral("date wrap %1").arg(onoff(P.dateWrap)));
        line(QStringLiteral("date wrap year %1").arg(P.dateWrapYear));
        line(QStringLiteral("default linewidth 1.0"));
        line(QStringLiteral("default linestyle 1"));
        line(QStringLiteral("default color 1"));
        line(QStringLiteral("default pattern 1"));
        line(QStringLiteral("default font 0"));
        line(QStringLiteral("default char size 1.000000"));
        line(QStringLiteral("default symbol size 1.000000"));
        line(QStringLiteral("default sformat \"%.8g\""));
        line(QStringLiteral("background color %1").arg(color(P.page.background.color)));
        line(QStringLiteral("page background fill %1").arg(onoff(P.page.backgroundFill)));
        const Timestamp& t = P.timestamp;
        line(QStringLiteral("timestamp %1").arg(onoff(t.active)));
        line(QStringLiteral("timestamp %1, %2").arg(num(t.pos.x()), num(t.pos.y())));
        line(QStringLiteral("timestamp color %1").arg(color(t.text.color)));
        line(QStringLiteral("timestamp rot %1").arg(num(t.angle)));
        line(QStringLiteral("timestamp font %1").arg(font(t.text.font)));
        line(QStringLiteral("timestamp char size %1").arg(fix(t.text.size)));
        line(QStringLiteral("timestamp def %1").arg(str(t.value.isEmpty() ? QDateTime::currentDateTime().toString() : t.value)));
        for (const DrawObject& o : P.objects) object(o);
        for (int r = 0; r < 5; ++r) region(r, P.regions[r]);
        for (int gi = 0; gi < P.graphs.size(); ++gi) {
            if (P.graphs[gi].active) graph(gi, P.graphs[gi]);
        }
        if (!P.description.isEmpty()) {
            for (const QString& l : P.description.split(QLatin1Char('\n'))) line(QStringLiteral("description %1").arg(str(l)));
        }
        for (int gi = 0; gi < P.graphs.size(); ++gi) {
            const Graph& gr = P.graphs[gi];
            if (!gr.active) continue;
            for (int si = 0; si < gr.sets.size(); ++si) {
                const DataSet& s = gr.sets[si];
                if (!s.active) continue;
                line(QStringLiteral("target G%1.S%2").arg(gi).arg(si));
                line(QStringLiteral("type %1").arg(setTypeName(s.type)));
                const int nc = s.columns();
                for (int i = 0; i < s.size(); ++i) {
                    QString row;
                    for (int c = 0; c < nc; ++c) {
                        if (c) row += QLatin1Char(' ');
                        row += num(s.cols[c].value(i));
                    }
                    if (!s.strings.isEmpty() && !s.strings.value(i).isEmpty()) row += QLatin1Char(' ') + str(s.strings.value(i));
                    out += row + QLatin1Char('\n');
                }
                out += QStringLiteral("&\n");
            }
        }
    }

    void object(const DrawObject& o)
    {
        const char* kinds[] = {"string", "line", "box", "ellipse"};
        const QString k = QString::fromLatin1(kinds[int(o.kind)]);
        line(QStringLiteral("with %1").arg(k));
        g(QStringLiteral("%1 %2").arg(k, onoff(o.active)));
        g(QStringLiteral("%1 loctype %2").arg(k, o.loctype == LocType::World ? QStringLiteral("world") : QStringLiteral("view")));
        g(QStringLiteral("%1 g%2").arg(k).arg(o.graph));
        if (o.kind == DrawObjectKind::Text) {
            g(QStringLiteral("string %1, %2").arg(num(o.pos.x()), num(o.pos.y())));
            g(QStringLiteral("string color %1").arg(color(o.textStyle.color)));
            g(QStringLiteral("string rot %1").arg(num(o.angle)));
            g(QStringLiteral("string font %1").arg(font(o.textStyle.font)));
            g(QStringLiteral("string just %1").arg(o.just));
            g(QStringLiteral("string char size %1").arg(fix(o.textStyle.size)));
            g(QStringLiteral("string def %1").arg(str(o.text)));
            return;
        }
        g(QStringLiteral("%1 %2, %3, %4, %5").arg(k, num(o.p1.x()), num(o.p1.y()), num(o.p2.x()), num(o.p2.y())));
        g(QStringLiteral("%1 linewidth %2").arg(k, num(o.line.width)));
        g(QStringLiteral("%1 linestyle %2").arg(k).arg(o.line.style));
        g(QStringLiteral("%1 color %2").arg(k).arg(color(o.line.color)));
        if (o.kind == DrawObjectKind::Line) {
            g(QStringLiteral("line arrow %1").arg(o.arrow));
            g(QStringLiteral("line arrow type %1").arg(o.arrowType));
            g(QStringLiteral("line arrow length %1").arg(fix(o.arrowLength)));
            g(QStringLiteral("line arrow layout %1, %2").arg(fix(o.arrowDL), fix(o.arrowLL)));
        } else {
            g(QStringLiteral("%1 rot %2").arg(k, num(o.rotation)));
            g(QStringLiteral("%1 fill color %2").arg(k).arg(color(o.fill.color)));
            g(QStringLiteral("%1 fill pattern %2").arg(k).arg(o.fill.pattern));
        }
        line(QStringLiteral("%1 def").arg(k));
    }

    void region(int r, const Region& R)
    {
        static const char* names[] = {"polyi", "above", "below", "left", "right", "horizi", "verti"};
        QString type = QString::fromLatin1(names[int(R.type)]);
        if (R.negate) {
            if (R.type == RegionType::Polygon) type = QStringLiteral("polyo");
            if (R.type == RegionType::Horizontal) type = QStringLiteral("horizo");
            if (R.type == RegionType::Vertical) type = QStringLiteral("verto");
        }
        if (R.active) line(QStringLiteral("r%1 on").arg(r));
        line(QStringLiteral("link r%1 to g%2").arg(r).arg(R.linkedGraph));
        line(QStringLiteral("r%1 type %2").arg(r).arg(type));
        line(QStringLiteral("r%1 linestyle %2").arg(r).arg(R.line.style));
        line(QStringLiteral("r%1 linewidth %2").arg(r).arg(num(R.line.width)));
        line(QStringLiteral("r%1 color %2").arg(r).arg(color(R.line.color)));
        line(QStringLiteral("r%1 line %2, %3, %4, %5").arg(r).arg(num(R.p1.x()), num(R.p1.y()), num(R.p2.x()), num(R.p2.y())));
        for (const QPointF& q : R.polygon) line(QStringLiteral("r%1 xy %2, %3").arg(r).arg(num(q.x()), num(q.y())));
    }

    void tick(const QString& pre, const QString& which, const TickProps& tp)
    {
        g(QStringLiteral("%1 tick %2 size %3").arg(pre, which, fix(tp.size)));
        g(QStringLiteral("%1 tick %2 color %3").arg(pre, which).arg(color(tp.line.color)));
        g(QStringLiteral("%1 tick %2 linewidth %3").arg(pre, which, num(tp.line.width)));
        g(QStringLiteral("%1 tick %2 linestyle %3").arg(pre, which).arg(tp.line.style));
        g(QStringLiteral("%1 tick %2 grid %3").arg(pre, which, onoff(tp.grid)));
        if (native()) {
            g(QStringLiteral("%1 tick %2 gridcolor %3").arg(pre, which).arg(color(tp.gridLine.color)));
            g(QStringLiteral("%1 tick %2 gridlinewidth %3").arg(pre, which, num(tp.gridLine.width)));
            g(QStringLiteral("%1 tick %2 gridlinestyle %3").arg(pre, which).arg(tp.gridLine.style));
        }
    }

    void axis(const QString& name, const Axis& a, bool x)
    {
        const QString pre = name;
        if (!a.active) {
            g(QStringLiteral("%1  off").arg(pre));
            return;
        }
        g(QStringLiteral("%1  on").arg(pre));
        g(QStringLiteral("%1  type zero %2").arg(pre, tf(a.zero)));
        g(QStringLiteral("%1  offset %2 , %3").arg(pre, fix(a.offset.x()), fix(a.offset.y())));
        g(QStringLiteral("%1  bar %2").arg(pre, onoff(a.barOn)));
        g(QStringLiteral("%1  bar color %2").arg(pre).arg(color(a.bar.color)));
        g(QStringLiteral("%1  bar linestyle %2").arg(pre).arg(a.bar.style));
        g(QStringLiteral("%1  bar linewidth %2").arg(pre, num(a.bar.width)));
        g(QStringLiteral("%1  label %2").arg(pre, str(a.label)));
        g(QStringLiteral("%1  label layout %2").arg(pre, a.labelLayout == LabelLayout::Parallel ? QStringLiteral("para") : QStringLiteral("perp")));
        g(QStringLiteral("%1  label place %2").arg(pre, a.labelPlaceAuto ? QStringLiteral("auto") : QStringLiteral("spec")));
        if (!a.labelPlaceAuto) g(QStringLiteral("%1  label place %2, %3").arg(pre, fix(a.labelOffset.x()), fix(a.labelOffset.y())));
        g(QStringLiteral("%1  label char size %2").arg(pre, fix(a.labelText.size)));
        g(QStringLiteral("%1  label font %2").arg(pre).arg(font(a.labelText.font)));
        g(QStringLiteral("%1  label color %2").arg(pre).arg(color(a.labelText.color)));
        g(QStringLiteral("%1  label place %2").arg(pre, a.labelSide == SideOp::Both ? QStringLiteral("both") : (a.labelSide == SideOp::Normal ? QStringLiteral("normal") : QStringLiteral("opposite"))));
        g(QStringLiteral("%1  tick %2").arg(pre, onoff(a.tickOn)));
        g(QStringLiteral("%1  tick major %2").arg(pre, num(a.major)));
        g(QStringLiteral("%1  tick minor ticks %2").arg(pre).arg(a.minorTicks));
        g(QStringLiteral("%1  tick default %2").arg(pre).arg(a.autoTickCount));
        g(QStringLiteral("%1  tick place rounded %2").arg(pre, tf(a.placeRounded)));
        g(QStringLiteral("%1  tick %2").arg(pre, a.tickDir == TickDirection::In ? QStringLiteral("in") : a.tickDir == TickDirection::Out ? QStringLiteral("out") : QStringLiteral("both")));
        tick(pre + QLatin1Char(' '), QStringLiteral("major"), a.majorProps);
        tick(pre + QLatin1Char(' '), QStringLiteral("minor"), a.minorProps);
        g(QStringLiteral("%1  ticklabel %2").arg(pre, onoff(a.ticklabelOn)));
        g(QStringLiteral("%1  ticklabel prec %2").arg(pre).arg(a.precision));
        g(QStringLiteral("%1  ticklabel format %2").arg(pre, labelFormatName(a.format)));
        if (!a.ticklabelFormula.isEmpty()) g(QStringLiteral("%1  ticklabel formula %2").arg(pre, str(a.ticklabelFormula)));
        g(QStringLiteral("%1  ticklabel append %2").arg(pre, str(a.append)));
        g(QStringLiteral("%1  ticklabel prepend %2").arg(pre, str(a.prepend)));
        g(QStringLiteral("%1  ticklabel angle %2").arg(pre).arg(int(std::lround(a.ticklabelAngle))));
        g(QStringLiteral("%1  ticklabel skip %2").arg(pre).arg(a.ticklabelSkip));
        g(QStringLiteral("%1  ticklabel stagger %2").arg(pre).arg(a.ticklabelStagger));
        g(QStringLiteral("%1  ticklabel place %2").arg(pre, a.ticklabelSide == SideOp::Both ? QStringLiteral("both") : (a.ticklabelSide == SideOp::Normal ? QStringLiteral("normal") : QStringLiteral("opposite"))));
        g(QStringLiteral("%1  ticklabel offset %2").arg(pre, a.ticklabelOffsetAuto ? QStringLiteral("auto") : QStringLiteral("spec")));
        g(QStringLiteral("%1  ticklabel offset %2 , %3").arg(pre, fix(a.ticklabelOffset.x()), fix(a.ticklabelOffset.y())));
        g(QStringLiteral("%1  ticklabel start type %2").arg(pre, a.ticklabelStartAuto ? QStringLiteral("auto") : QStringLiteral("spec")));
        g(QStringLiteral("%1  ticklabel start %2").arg(pre, fix(a.ticklabelStart)));
        g(QStringLiteral("%1  ticklabel stop type %2").arg(pre, a.ticklabelStopAuto ? QStringLiteral("auto") : QStringLiteral("spec")));
        g(QStringLiteral("%1  ticklabel stop %2").arg(pre, fix(a.ticklabelStop)));
        g(QStringLiteral("%1  ticklabel char size %2").arg(pre, fix(a.ticklabelText.size)));
        g(QStringLiteral("%1  ticklabel font %2").arg(pre).arg(font(a.ticklabelText.font)));
        g(QStringLiteral("%1  ticklabel color %2").arg(pre).arg(color(a.ticklabelText.color)));
        g(QStringLiteral("%1  ticklabel sign %2").arg(pre, a.ticklabelSign == TickSign::Absolute ? QStringLiteral("absolute") : a.ticklabelSign == TickSign::Negate ? QStringLiteral("negate") : QStringLiteral("normal")));
        g(QStringLiteral("%1  tick place %2").arg(pre, side(a.tickSide, x) == QLatin1String("both") ? QStringLiteral("both") : (a.tickSide == SideOp::Normal ? QStringLiteral("normal") : QStringLiteral("opposite"))));
        const bool spec = !a.tickAuto;
        g(QStringLiteral("%1  tick spec type %2").arg(pre, !spec ? QStringLiteral("none") : (a.ticklabelAuto ? QStringLiteral("ticks") : QStringLiteral("both"))));
        if (spec) {
            g(QStringLiteral("%1  tick spec %2").arg(pre).arg(a.specTicks.size()));
            for (int i = 0; i < a.specTicks.size(); ++i) {
                const TickMark& m = a.specTicks[i];
                g(QStringLiteral("%1  tick %2 %3, %4").arg(pre, m.major ? QStringLiteral("major") : QStringLiteral("minor")).arg(i).arg(num(m.value)));
                if (!a.ticklabelAuto && m.major) g(QStringLiteral("%1  ticklabel %2, %3").arg(pre).arg(i).arg(str(m.label)));
            }
        }
    }

    void graph(int gi, const Graph& gr)
    {
        line(QStringLiteral("g%1 on").arg(gi));
        line(QStringLiteral("g%1 hidden %2").arg(gi).arg(tf(gr.hidden)));
        static const char* types[] = {"XY", "Chart", "Polar", "Smith", "Fixed", "Pie"};
        line(QStringLiteral("g%1 type %2").arg(gi).arg(QString::fromLatin1(types[int(gr.type)])));
        line(QStringLiteral("g%1 stacked %2").arg(gi).arg(tf(gr.stacked)));
        line(QStringLiteral("g%1 bar hgap %2").arg(gi).arg(fix(gr.barHGap)));
        line(QStringLiteral("g%1 fixedpoint %2").arg(gi).arg(onoff(gr.fixedPoint.active)));
        line(QStringLiteral("g%1 fixedpoint type %2").arg(gi).arg(gr.fixedPoint.type));
        line(QStringLiteral("g%1 fixedpoint xy %2, %3").arg(gi).arg(fix(gr.fixedPoint.xy.x()), fix(gr.fixedPoint.xy.y())));
        line(QStringLiteral("g%1 fixedpoint format %2 %3").arg(gi).arg(labelFormatName(gr.fixedPoint.formatX), labelFormatName(gr.fixedPoint.formatY)));
        line(QStringLiteral("g%1 fixedpoint prec %2, %3").arg(gi).arg(gr.fixedPoint.precX).arg(gr.fixedPoint.precY));
        if (native()) line(QStringLiteral("g%1 smartminor %2").arg(gi).arg(onoff(gr.smartMinorTicks)));
        line(QStringLiteral("with g%1").arg(gi));
        g(QStringLiteral("world %1, %2, %3, %4").arg(num(gr.wxmin), num(gr.wymin), num(gr.wxmax), num(gr.wymax)));
        g(QStringLiteral("stack world 0, 0, 0, 0"));
        g(QStringLiteral("znorm %1").arg(num(gr.znorm)));
        g(QStringLiteral("view %1, %2, %3, %4").arg(fix(gr.vxmin), fix(gr.vymin), fix(gr.vxmax), fix(gr.vymax)));
        g(QStringLiteral("title %1").arg(str(gr.title.text)));
        g(QStringLiteral("title font %1").arg(font(gr.title.style.font)));
        g(QStringLiteral("title size %1").arg(fix(gr.title.style.size)));
        g(QStringLiteral("title color %1").arg(color(gr.title.style.color)));
        g(QStringLiteral("subtitle %1").arg(str(gr.subtitle.text)));
        g(QStringLiteral("subtitle font %1").arg(font(gr.subtitle.style.font)));
        g(QStringLiteral("subtitle size %1").arg(fix(gr.subtitle.style.size)));
        g(QStringLiteral("subtitle color %1").arg(color(gr.subtitle.style.color)));
        g(QStringLiteral("xaxes scale %1").arg(scaleTypeName(gr.xscale)));
        g(QStringLiteral("yaxes scale %1").arg(scaleTypeName(gr.yscale)));
        g(QStringLiteral("xaxes invert %1").arg(onoff(gr.xinvert)));
        g(QStringLiteral("yaxes invert %1").arg(onoff(gr.yinvert)));
        axis(QStringLiteral("xaxis"), gr.axes[0], true);
        axis(QStringLiteral("yaxis"), gr.axes[1], false);
        axis(QStringLiteral("altxaxis"), gr.axes[2], true);
        axis(QStringLiteral("altyaxis"), gr.axes[3], false);
        const Legend& L = gr.legend;
        g(QStringLiteral("legend %1").arg(onoff(L.active)));
        g(QStringLiteral("legend loctype %1").arg(L.loctype == LocType::World ? QStringLiteral("world") : QStringLiteral("view")));
        g(QStringLiteral("legend %1, %2").arg(num(L.pos.x()), num(L.pos.y())));
        if (native()) {
            g(QStringLiteral("legend auto %1").arg(onoff(L.autoPlace)));
            g(QStringLiteral("legend box radius %1").arg(num(L.cornerRadius)));
        }
        g(QStringLiteral("legend box color %1").arg(color(L.box.color)));
        g(QStringLiteral("legend box pattern %1").arg(L.box.style > 0 ? 1 : 0));
        g(QStringLiteral("legend box linewidth %1").arg(num(L.box.width)));
        g(QStringLiteral("legend box linestyle %1").arg(L.box.style));
        g(QStringLiteral("legend box fill color %1").arg(color(L.boxFill.color)));
        g(QStringLiteral("legend box fill pattern %1").arg(L.boxFill.pattern));
        g(QStringLiteral("legend font %1").arg(font(L.text.font)));
        g(QStringLiteral("legend char size %1").arg(fix(L.text.size)));
        g(QStringLiteral("legend color %1").arg(color(L.text.color)));
        g(QStringLiteral("legend length %1").arg(L.length));
        g(QStringLiteral("legend vgap %1").arg(L.vgap));
        g(QStringLiteral("legend hgap %1").arg(L.hgap));
        g(QStringLiteral("legend invert %1").arg(tf(L.invert)));
        const Frame& F = gr.frame;
        g(QStringLiteral("frame type %1").arg(int(F.type)));
        g(QStringLiteral("frame linestyle %1").arg(F.line.style));
        g(QStringLiteral("frame linewidth %1").arg(num(F.line.width)));
        g(QStringLiteral("frame color %1").arg(color(F.line.color)));
        g(QStringLiteral("frame pattern %1").arg(F.line.style > 0 ? 1 : 0));
        g(QStringLiteral("frame background color %1").arg(color(F.background.color)));
        g(QStringLiteral("frame background pattern %1").arg(F.background.pattern));
        for (int si = 0; si < gr.sets.size(); ++si) {
            if (gr.sets[si].active) set(si, gr.sets[si]);
        }
    }

    void set(int si, const DataSet& s)
    {
        const QString p = QStringLiteral("s%1").arg(si);
        g(QStringLiteral("%1 hidden %2").arg(p, tf(s.hidden)));
        g(QStringLiteral("%1 type %2").arg(p, setTypeName(s.type)));
        g(QStringLiteral("%1 symbol %2").arg(p).arg(symbol(s.symbol.type)));
        g(QStringLiteral("%1 symbol size %2").arg(p, fix(s.symbol.size)));
        g(QStringLiteral("%1 symbol color %2").arg(p).arg(color(s.symbol.line.color)));
        g(QStringLiteral("%1 symbol pattern %2").arg(p).arg(s.symbol.line.style > 0 ? 1 : 0));
        g(QStringLiteral("%1 symbol fill color %2").arg(p).arg(color(s.symbol.fill.color)));
        g(QStringLiteral("%1 symbol fill pattern %2").arg(p).arg(s.symbol.fill.pattern));
        g(QStringLiteral("%1 symbol linewidth %2").arg(p, num(s.symbol.line.width)));
        g(QStringLiteral("%1 symbol linestyle %2").arg(p).arg(s.symbol.line.style));
        g(QStringLiteral("%1 symbol char %2").arg(p).arg(s.symbol.ch));
        g(QStringLiteral("%1 symbol char font %2").arg(p).arg(font(s.symbol.charFont)));
        g(QStringLiteral("%1 symbol skip %2").arg(p).arg(s.symbol.skip));
        g(QStringLiteral("%1 line type %2").arg(p).arg(lineType(s.lineType)));
        g(QStringLiteral("%1 line linestyle %2").arg(p).arg(s.line.style));
        g(QStringLiteral("%1 line linewidth %2").arg(p, num(s.line.width)));
        g(QStringLiteral("%1 line color %2").arg(p).arg(color(s.line.color)));
        g(QStringLiteral("%1 line pattern %2").arg(p).arg(s.line.style > 0 ? 1 : 0));
        g(QStringLiteral("%1 baseline type %2").arg(p).arg(int(s.baselineType)));
        g(QStringLiteral("%1 baseline %2").arg(p, onoff(s.baselineShown)));
        g(QStringLiteral("%1 dropline %2").arg(p, onoff(s.dropLines)));
        g(QStringLiteral("%1 fill type %2").arg(p).arg(int(s.fillType)));
        g(QStringLiteral("%1 fill rule %2").arg(p).arg(s.fillRule == FillRule::EvenOdd ? 1 : 0));
        g(QStringLiteral("%1 fill color %2").arg(p).arg(color(s.fill.color)));
        g(QStringLiteral("%1 fill pattern %2").arg(p).arg(s.fill.pattern));
        if (native() && s.fill.gradient != GradientType::None) {
            g(QStringLiteral("%1 fill gradient %2").arg(p, gradientName(s.fill.gradient)));
            g(QStringLiteral("%1 fill gradient color %2").arg(p).arg(color(s.fill.color2)));
        }
        const AnnotatedValues& a = s.avalue;
        g(QStringLiteral("%1 avalue %2").arg(p, onoff(a.active)));
        g(QStringLiteral("%1 avalue type %2").arg(p).arg(a.type));
        g(QStringLiteral("%1 avalue char size %2").arg(p, fix(a.text.size)));
        g(QStringLiteral("%1 avalue font %2").arg(p).arg(font(a.text.font)));
        g(QStringLiteral("%1 avalue color %2").arg(p).arg(color(a.text.color)));
        g(QStringLiteral("%1 avalue rot %2").arg(p).arg(int(std::lround(a.angle))));
        g(QStringLiteral("%1 avalue format %2").arg(p, labelFormatName(a.format)));
        g(QStringLiteral("%1 avalue prec %2").arg(p).arg(a.precision));
        g(QStringLiteral("%1 avalue prepend %2").arg(p, str(a.prepend)));
        g(QStringLiteral("%1 avalue append %2").arg(p, str(a.append)));
        g(QStringLiteral("%1 avalue offset %2 , %3").arg(p, fix(a.offset.x()), fix(a.offset.y())));
        const ErrorBarStyle& e = s.errorbar;
        g(QStringLiteral("%1 errorbar %2").arg(p, onoff(e.active)));
        g(QStringLiteral("%1 errorbar place %2").arg(p, e.placement == ErrorBarPlacement::Both ? QStringLiteral("both") : e.placement == ErrorBarPlacement::Top ? QStringLiteral("normal") : QStringLiteral("opposite")));
        g(QStringLiteral("%1 errorbar color %2").arg(p).arg(color(e.color)));
        g(QStringLiteral("%1 errorbar pattern %2").arg(p).arg(e.pattern));
        g(QStringLiteral("%1 errorbar size %2").arg(p, fix(e.size)));
        g(QStringLiteral("%1 errorbar linewidth %2").arg(p, num(e.width)));
        g(QStringLiteral("%1 errorbar linestyle %2").arg(p).arg(e.style));
        g(QStringLiteral("%1 errorbar riser linewidth %2").arg(p, num(e.riserWidth)));
        g(QStringLiteral("%1 errorbar riser linestyle %2").arg(p).arg(e.riserStyle));
        g(QStringLiteral("%1 errorbar riser clip %2").arg(p, onoff(e.riserClip)));
        g(QStringLiteral("%1 errorbar riser clip length %2").arg(p, fix(e.riserClipLength)));
        if (native()) {
            g(QStringLiteral("%1 colormap %2").arg(p, str(s.colormap)));
            if (s.cmapAuto) g(QStringLiteral("%1 colormap range auto").arg(p));
            else g(QStringLiteral("%1 colormap range %2, %3").arg(p, num(s.cmapMin), num(s.cmapMax)));
            g(QStringLiteral("%1 colormap reversed %2").arg(p, onoff(s.cmapReversed)));
            g(QStringLiteral("%1 colormap index %2").arg(p, onoff(!s.colorFromMap)));
            g(QStringLiteral("%1 colormap bar %2").arg(p, onoff(s.showColorBar)));
        }
        g(QStringLiteral("%1 comment %2").arg(p, str(s.comment)));
        g(QStringLiteral("%1 legend  %2").arg(p, str(s.legend)));
    }
};

}  // namespace

QString write(const Project& p, Dialect d)
{
    Writer w(p, d);
    return w.run();
}

bool saveAs(const Project& p, const QString& path, Dialect d, QString* error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    const QString text = write(p, d);
    // Grace expects Latin-1; the native format is UTF-8
    f.write(d == Dialect::Grace ? text.toLatin1() : text.toUtf8());
    if (!f.commit()) {
        if (error) *error = QStringLiteral("cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

bool save(const Project& p, const QString& path, QString* error)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    const Dialect d = (suffix == QLatin1String("agr") || suffix == QLatin1String("xvgr") || suffix == QLatin1String("xmgr") ||
                       suffix == QLatin1String("par"))
                          ? Dialect::Grace
                          : Dialect::Native;
    return saveAs(p, path, d, error);
}

bool isProjectText(const QString& text)
{
    const QString head = text.left(4096);
    return head.contains(QLatin1String("Grace project file")) || head.contains(QLatin1String("CactusPlot project file")) ||
           head.contains(QLatin1String("@version")) || head.contains(QLatin1String("ACE/gr")) ||
           head.contains(QLatin1String("xvgr parameter file")) || head.contains(QLatin1String("@with g")) ||
           head.contains(QLatin1String("@g0 on"));
}

bool loadText(const QString& text, Project* out, const QString& sourceName, QString* error, QStringList* warnings)
{
    Project p;
    p.graphs.clear();
    const bool native = text.left(200).contains(QLatin1String("CactusPlot project file"));
    // Grace files are rendered with the Grace look for fidelity
    p.stylePreset = native ? presets::defaultName() : QStringLiteral("Grace Classic");
    p.prettyNumbers = native;
    p.page.background = FillStyle{QColor(Qt::white), 1};
    Interpreter in(&p);
    in.safeMode = true;
    in.autoscaleOnRead = 0;
    in.run(text, sourceName);
    if (!native) {
        // Grace sets: XYCOLOR uses colour indices, legends are boxed and placed explicitly
        for (Graph& g : p.graphs) {
            for (DataSet& s : g.sets) s.colorFromMap = false;
            g.smartMinorTicks = false;
        }
    }
    if (p.graphs.isEmpty()) p.ensureGraph(0);
    if (warnings) *warnings = in.warnings();
    if (!in.errors().isEmpty()) {
        if (warnings) *warnings << in.errors();
    }
    *out = p;
    Q_UNUSED(error);
    return true;
}

bool load(const QString& path, Project* out, QString* error, QStringList* warnings)
{
    QString text, err;
    if (!datareader::readFile(path, &text, &err)) {
        if (error) *error = err;
        return false;
    }
    return loadText(text, out, QFileInfo(path).fileName(), error, warnings);
}

}  // namespace cactus::projectio
