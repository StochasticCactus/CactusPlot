// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/Model.h"

#include <cmath>

#include "core/color/Colors.h"
#include "plot/Ticks.h"

namespace cactus {

// ---------------------------------------------------------------- DataSet

void DataSet::resize(int n)
{
    const int nc = columns();
    for (int c = 0; c < 6; ++c) {
        if (c < nc) cols[c].resize(n);
        else cols[c].clear();
    }
    if (!strings.isEmpty()) {
        while (strings.size() < n) strings.append(QString());
        while (strings.size() > n) strings.removeLast();
    }
}

void DataSet::setType(SetType t)
{
    const int n = size();
    type = t;
    resize(n);
}

void DataSet::clearData()
{
    for (auto& c : cols) c.clear();
    strings.clear();
}

bool DataSet::bounds(double* xmin, double* xmax, double* ymin, double* ymax, bool withErrors) const
{
    const int n = size();
    double x0 = HUGE_VAL, x1 = -HUGE_VAL, y0 = HUGE_VAL, y1 = -HUGE_VAL;
    auto acc = [&](double x, double y) {
        if (std::isfinite(x)) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
        if (std::isfinite(y)) { y0 = std::min(y0, y); y1 = std::max(y1, y); }
    };
    auto col = [&](int c, int i) { return (c < 6 && i < cols[c].size()) ? cols[c][i] : 0.0; };
    for (int i = 0; i < n; ++i) {
        const double x = cols[0][i];
        const double y = i < cols[1].size() ? cols[1][i] : 0.0;
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        acc(x, y);
        if (!withErrors) continue;
        switch (type) {
        case SetType::XYDX: acc(x - col(2, i), y); acc(x + col(2, i), y); break;
        case SetType::XYDY:
        case SetType::BARDY: acc(x, y - col(2, i)); acc(x, y + col(2, i)); break;
        case SetType::XYDXDX: acc(x + col(2, i), y); acc(x - col(3, i), y); break;
        case SetType::XYDYDY:
        case SetType::BARDYDY: acc(x, y + col(2, i)); acc(x, y - col(3, i)); break;
        case SetType::XYDXDY:
            acc(x - col(2, i), y - col(3, i));
            acc(x + col(2, i), y + col(3, i));
            break;
        case SetType::XYDXDXDYDY:
            acc(x + col(2, i), y + col(4, i));
            acc(x - col(3, i), y - col(5, i));
            break;
        case SetType::XYHILO:
            acc(x, col(1, i)); acc(x, col(2, i)); acc(x, col(3, i)); acc(x, col(4, i));
            break;
        case SetType::XYBOXPLOT:
            for (int c = 2; c <= 5; ++c) acc(x, col(c, i));
            break;
        case SetType::XYVMAP: acc(x + col(2, i), y + col(3, i)); break;
        default: break;
        }
    }
    if (x0 > x1 || y0 > y1) return false;
    *xmin = x0; *xmax = x1; *ymin = y0; *ymax = y1;
    return true;
}

// ---------------------------------------------------------------- Graph

DataSet& Graph::ensureSet(int i)
{
    if (i < 0) i = 0;
    while (sets.size() <= i) sets.push_back(DataSet());
    if (!sets[i].active) {
        sets[i].active = true;
    }
    return sets[i];
}

int Graph::nextFreeSet() const
{
    for (int i = 0; i < sets.size(); ++i) if (!sets[i].active) return i;
    return int(sets.size());
}

int Graph::nextEmptySet() const
{
    for (int i = 0; i < sets.size(); ++i) if (!sets[i].active || sets[i].size() == 0) return i;
    return int(sets.size());
}

int Graph::activeSetCount() const
{
    int n = 0;
    for (const DataSet& s : sets) if (s.active) ++n;
    return n;
}

void Graph::autoscale(int which, int onlySet)
{
    double x0 = HUGE_VAL, x1 = -HUGE_VAL, y0 = HUGE_VAL, y1 = -HUGE_VAL;
    bool any = false;
    double cumulative = 0.0;  // for stacked charts
    QVector<double> stackPos, stackNeg;
    for (int i = 0; i < sets.size(); ++i) {
        const DataSet& s = sets[i];
        if (!s.active || s.hidden || s.size() == 0) continue;
        if (onlySet >= 0 && i != onlySet) continue;
        double a, b, c, d;
        if (!s.bounds(&a, &b, &c, &d)) continue;
        if (xscale == ScaleType::Log || yscale == ScaleType::Log) {
            // ignore non-positive values on log axes
            double la = HUGE_VAL, lb = -HUGE_VAL, lc = HUGE_VAL, ld = -HUGE_VAL;
            for (int k = 0; k < s.size(); ++k) {
                const double x = s.cols[0][k], y = s.cols[1][k];
                if (xscale == ScaleType::Log && !(x > 0)) continue;
                if (yscale == ScaleType::Log && !(y > 0)) continue;
                la = std::min(la, x); lb = std::max(lb, x); lc = std::min(lc, y); ld = std::max(ld, y);
            }
            if (la > lb) continue;
            if (xscale == ScaleType::Log) { a = la; b = lb; }
            if (yscale == ScaleType::Log) { c = lc; d = ld; }
        }
        if (type == GraphType::Chart && stacked) {
            // stacked charts: accumulate y per point index
            for (int k = 0; k < s.size(); ++k) {
                if (stackPos.size() <= k) { stackPos.resize(k + 1); stackNeg.resize(k + 1); }
                const double y = s.cols[1][k];
                if (y >= 0) stackPos[k] += y; else stackNeg[k] += y;
                c = std::min(c, stackNeg[k]);
                d = std::max(d, stackPos[k]);
            }
        }
        x0 = std::min(x0, a); x1 = std::max(x1, b);
        y0 = std::min(y0, c); y1 = std::max(y1, d);
        any = true;
    }
    Q_UNUSED(cumulative);
    if (!any) return;
    if (type == GraphType::Chart || type == GraphType::Pie) {
        // bars need room on both sides and a zero baseline
        if (type == GraphType::Chart) {
            x0 -= 0.5; x1 += 0.5;
            y0 = std::min(0.0, y0);
            y1 = std::max(0.0, y1);
        }
    }
    if (type == GraphType::Polar) {
        if (which & 1) { wxmin = 0.0; wxmax = 2 * M_PI; }
        if (which & 2) {
            wymin = 0.0;
            double r = std::max(std::fabs(y0), std::fabs(y1));
            double lo = 0.0;
            ticks::niceRange(&lo, &r, ScaleType::Linear, axes[Y].autoTickCount);
            wymax = r;
        }
        autoticks();
        return;
    }
    if (which & 1) {
        ticks::niceRange(&x0, &x1, xscale, axes[X].autoTickCount);
        wxmin = x0; wxmax = x1;
    }
    if (which & 2) {
        ticks::niceRange(&y0, &y1, yscale, axes[Y].autoTickCount);
        wymin = y0; wymax = y1;
    }
    if (type == GraphType::Fixed) {
        // keep equal scaling: expand the range that is too small
        const double vw = vxmax - vxmin, vh = vymax - vymin;
        const double sx = (wxmax - wxmin) / vw, sy = (wymax - wymin) / vh;
        if (sx > sy) {
            const double c = 0.5 * (wymin + wymax), h = 0.5 * sx * vh;
            wymin = c - h; wymax = c + h;
        } else {
            const double c = 0.5 * (wxmin + wxmax), h = 0.5 * sy * vw;
            wxmin = c - h; wxmax = c + h;
        }
    }
    autoticks();
}

void Graph::autoticks()
{
    for (int a = 0; a < 4; ++a) {
        Axis& ax = axes[a];
        const bool isX = (a == X || a == AltX);
        const double lo = isX ? wxmin : wymin, hi = isX ? wxmax : wymax;
        const ScaleType st = isX ? xscale : yscale;
        ax.major = ticks::autoSpacing(lo, hi, st, ax.autoTickCount);
        ax.minorTicks = ticks::autoMinorCount(ax.major, st, smartMinorTicks);
    }
}

// ---------------------------------------------------------------- Region

bool Region::contains(double x, double y) const
{
    bool in = false;
    auto lineY = [&](double xx) {
        if (p2.x() == p1.x()) return p1.y();
        return p1.y() + (p2.y() - p1.y()) * (xx - p1.x()) / (p2.x() - p1.x());
    };
    auto lineX = [&](double yy) {
        if (p2.y() == p1.y()) return p1.x();
        return p1.x() + (p2.x() - p1.x()) * (yy - p1.y()) / (p2.y() - p1.y());
    };
    switch (type) {
    case RegionType::Polygon: {
        const int n = int(polygon.size());
        for (int i = 0, j = n - 1; i < n; j = i++) {
            const QPointF& a = polygon[i];
            const QPointF& b = polygon[j];
            if (((a.y() > y) != (b.y() > y)) && (x < (b.x() - a.x()) * (y - a.y()) / (b.y() - a.y()) + a.x())) in = !in;
        }
        break;
    }
    case RegionType::Above: in = y > lineY(x); break;
    case RegionType::Below: in = y < lineY(x); break;
    case RegionType::Left: in = x < lineX(y); break;
    case RegionType::Right: in = x > lineX(y); break;
    case RegionType::Horizontal: in = x >= std::min(p1.x(), p2.x()) && x <= std::max(p1.x(), p2.x()); break;
    case RegionType::Vertical: in = y >= std::min(p1.y(), p2.y()) && y <= std::max(p1.y(), p2.y()); break;
    }
    return negate ? !in : in;
}

// ---------------------------------------------------------------- Project

Project::Project()
{
    for (const NamedColor& c : colors::graceDefaults()) colorMap.push_back(ColorMapEntry{c.color, c.name});
    fontMap = QStringList{QStringLiteral("Times-Roman"),      QStringLiteral("Times-Italic"),
                          QStringLiteral("Times-Bold"),       QStringLiteral("Times-BoldItalic"),
                          QStringLiteral("Helvetica"),        QStringLiteral("Helvetica-Oblique"),
                          QStringLiteral("Helvetica-Bold"),   QStringLiteral("Helvetica-BoldOblique"),
                          QStringLiteral("Courier"),          QStringLiteral("Courier-Oblique"),
                          QStringLiteral("Courier-Bold"),     QStringLiteral("Courier-BoldOblique"),
                          QStringLiteral("Symbol"),           QStringLiteral("ZapfDingbats")};
}

Graph& Project::ensureGraph(int i)
{
    if (i < 0) i = 0;
    while (graphs.size() <= i) graphs.push_back(Graph());
    graphs[i].active = true;
    return graphs[i];
}

int Project::activeGraphCount() const
{
    int n = 0;
    for (const Graph& g : graphs) if (g.active) ++n;
    return n;
}

QColor Project::colorAt(int index) const
{
    if (index >= 0 && index < colorMap.size()) return colorMap[index].color;
    return colors::graceColor(index);
}

int Project::colorIndexConst(const QColor& c) const
{
    const QRgb want = c.rgb();
    for (int i = 0; i < colorMap.size(); ++i) {
        if (colorMap[i].color.rgb() == want) return i;
    }
    return -1;
}

int Project::colorIndex(const QColor& c)
{
    const int i = colorIndexConst(c);
    if (i >= 0) return i;
    colorMap.push_back(ColorMapEntry{QColor(c.rgb()), colors::displayName(QColor(c.rgb()))});
    return int(colorMap.size()) - 1;
}

QString Project::fontAt(int index) const
{
    if (index >= 0 && index < fontMap.size()) return fontMap[index];
    return QStringLiteral("Helvetica");
}

int Project::fontIndex(const QString& name)
{
    for (int i = 0; i < fontMap.size(); ++i) {
        if (fontMap[i].compare(name, Qt::CaseInsensitive) == 0) return i;
    }
    fontMap.push_back(name);
    return int(fontMap.size()) - 1;
}

void arrangeGraphs(Project& p, int rows, int cols, double offset, double hgap, double vgap, bool hvinv, bool hinv,
                   bool vinv, bool snake)
{
    if (rows < 1 || cols < 1) return;
    const double pw = p.page.viewWidth(), ph = p.page.viewHeight();
    // Grace: hgap/vgap are relative to the graph size
    const double gw = (pw - 2 * offset) / (cols + (cols - 1) * hgap);
    const double gh = (ph - 2 * offset) / (rows + (rows - 1) * vgap);
    const int n = rows * cols;
    // existing active graphs in order
    QVector<int> ids;
    for (int i = 0; i < p.graphs.size(); ++i) if (p.graphs[i].active) ids.push_back(i);
    int next = p.graphs.size();
    while (ids.size() < n) {
        Graph g = p.graphs.isEmpty() ? Graph() : p.graphs[ids.isEmpty() ? 0 : ids.first()];
        g.sets.clear();
        g.active = true;
        g.title.text.clear();
        g.subtitle.text.clear();
        p.graphs.push_back(g);
        ids.push_back(next++);
    }
    for (int k = 0; k < n; ++k) {
        int r, c;
        if (!hvinv) { r = k / cols; c = k % cols; if (snake && (r % 2 == 1)) c = cols - 1 - c; }
        else { c = k / rows; r = k % rows; if (snake && (c % 2 == 1)) r = rows - 1 - r; }
        if (hinv) c = cols - 1 - c;
        if (!vinv) r = rows - 1 - r;  // Grace fills from the top row
        Graph& g = p.graphs[ids[k]];
        g.vxmin = offset + c * gw * (1 + hgap);
        g.vxmax = g.vxmin + gw;
        g.vymin = offset + r * gh * (1 + vgap);
        g.vymax = g.vymin + gh;
    }
}

}  // namespace cactus
