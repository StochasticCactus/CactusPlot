// SPDX-License-Identifier: GPL-2.0-or-later
#include "render/PlotRenderer.h"

#include <QBitmap>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPathStroker>
#include <QtMath>
#include <algorithm>
#include <cmath>

#include "core/Presets.h"
#include "core/color/Colors.h"
#include "plot/Fonts.h"
#include "plot/RichText.h"
#include "plot/Ticks.h"

namespace cactus {

// ====================================================================== geometry

GraphGeometry::GraphGeometry(const Page& page, const Graph& g)
{
    type = g.type;
    unit = page.unit();
    pageHeight = page.height;
    const QPointF a(g.vxmin * unit, pageHeight - g.vymax * unit);
    const QPointF b(g.vxmax * unit, pageHeight - g.vymin * unit);
    view = QRectF(a, b).normalized();
    ScaleType xs = g.xscale, ys = g.yscale;
    if (type == GraphType::Polar || type == GraphType::Pie || type == GraphType::Smith) { xs = ScaleType::Linear; ys = ScaleType::Linear; }
    double x0 = g.wxmin, x1 = g.wxmax, y0 = g.wymin, y1 = g.wymax;
    if (type == GraphType::Fixed && view.width() > 0 && view.height() > 0 && x1 > x0 && y1 > y0) {
        // equal world units per point on both axes: expand the narrower range about its centre
        const double kx = (x1 - x0) / view.width(), ky = (y1 - y0) / view.height();
        if (kx > ky) {
            const double c = 0.5 * (y0 + y1), h = 0.5 * kx * view.height();
            y0 = c - h; y1 = c + h;
        } else {
            const double c = 0.5 * (x0 + x1), h = 0.5 * ky * view.width();
            x0 = c - h; x1 = c + h;
        }
    }
    sx = Scale(xs, x0, x1, g.xinvert);
    sy = Scale(ys, y0, y1, g.yinvert);
    center = view.center();
    radius = 0.5 * std::min(view.width(), view.height());
    valid = view.width() > 0 && view.height() > 0 &&
            (type == GraphType::Pie || (sx.validRange() && (type == GraphType::Polar ? g.wymax > g.wymin : sy.validRange())));
}

QPointF GraphGeometry::map(double wx, double wy) const
{
    switch (type) {
    case GraphType::Polar: {
        // x = angle (radians), y = radius
        // Grace: radius 0 is the centre, ymax is the outer circle; [ymin, ymax] is the visible annulus
        const double rmax = sy.wmax;
        double r = rmax != 0.0 ? wy / rmax : 0.0;
        double phi = sx.invert ? -wx : wx;
        return QPointF(center.x() + r * radius * std::cos(phi), center.y() - r * radius * std::sin(phi));
    }
    case GraphType::Smith: {
        // reflection coefficient plane, world [-1,1] maps to the circle
        return QPointF(center.x() + wx * radius, center.y() - wy * radius);
    }
    default: break;
    }
    const double fx = sx.toFrac(wx), fy = sy.toFrac(wy);
    return QPointF(view.left() + fx * view.width(), view.bottom() - fy * view.height());
}

bool GraphGeometry::inverse(const QPointF& pt, double* wx, double* wy) const
{
    switch (type) {
    case GraphType::Polar: {
        const double dx = pt.x() - center.x(), dy = center.y() - pt.y();
        const double r = std::hypot(dx, dy) / radius;
        double phi = std::atan2(dy, dx);
        if (phi < 0) phi += 2 * M_PI;
        if (sx.invert) phi = 2 * M_PI - phi;
        *wx = phi;
        *wy = r * sy.wmax;
        return true;
    }
    case GraphType::Smith:
        *wx = (pt.x() - center.x()) / radius;
        *wy = (center.y() - pt.y()) / radius;
        return true;
    default: break;
    }
    if (view.width() <= 0 || view.height() <= 0) return false;
    *wx = sx.fromFrac((pt.x() - view.left()) / view.width());
    *wy = sy.fromFrac((view.bottom() - pt.y()) / view.height());
    return std::isfinite(*wx) && std::isfinite(*wy);
}

namespace {
// Grace's 32 fill patterns (16x16, bit x = column x), extracted from Grace output for exact fidelity.
const quint16 kGracePatterns[32][16] = {
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},  // 0
    {0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},  // 1
    {0xeeee, 0xffff, 0xbbbb, 0xffff, 0xeeee, 0xffff, 0xbbbb, 0xffff, 0xeeee, 0xffff, 0xbbbb, 0xffff, 0xeeee, 0xffff, 0xbbbb, 0xffff},  // 2
    {0xeeee, 0xbbbb, 0xeeee, 0xbbbb, 0xeeee, 0xbbbb, 0xeeee, 0xbbbb, 0xeeee, 0xbbbb, 0xeeee, 0xbbbb, 0xeeee, 0xbbbb, 0xeeee, 0xbbbb},  // 3
    {0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa},  // 4
    {0x1111, 0x4444, 0x1111, 0x4444, 0x1111, 0x4444, 0x1111, 0x4444, 0x1111, 0x4444, 0x1111, 0x4444, 0x1111, 0x4444, 0x1111, 0x4444},  // 5
    {0x1111, 0x0000, 0x4444, 0x0000, 0x1111, 0x0000, 0x4444, 0x0000, 0x1111, 0x0000, 0x4444, 0x0000, 0x1111, 0x0000, 0x4444, 0x0000},  // 6
    {0x1010, 0x0000, 0x0000, 0x0000, 0x0101, 0x0000, 0x0000, 0x0000, 0x1010, 0x0000, 0x0000, 0x0000, 0x0101, 0x0000, 0x0000, 0x0000},  // 7
    {0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},  // 8
    {0x1e1e, 0x0f0f, 0x8787, 0xc3c3, 0xe1e1, 0xf0f0, 0x7878, 0x3c3c, 0x1e1e, 0x0f0f, 0x8787, 0xc3c3, 0xe1e1, 0xf0f0, 0x7878, 0x3c3c},  // 9
    {0x7878, 0xf0f0, 0xe1e1, 0xc3c3, 0x8787, 0x0f0f, 0x1e1e, 0x3c3c, 0x7878, 0xf0f0, 0xe1e1, 0xc3c3, 0x8787, 0x0f0f, 0x1e1e, 0x3c3c},  // 10
    {0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333, 0x3333},  // 11
    {0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000},  // 12
    {0x8181, 0x4242, 0x2424, 0x1818, 0x1818, 0x2424, 0x4242, 0x8181, 0x8181, 0x4242, 0x2424, 0x1818, 0x1818, 0x2424, 0x4242, 0x8181},  // 13
    {0x8080, 0x4040, 0x2020, 0x1010, 0x0808, 0x0404, 0x0202, 0x0101, 0x8080, 0x4040, 0x2020, 0x1010, 0x0808, 0x0404, 0x0202, 0x0101},  // 14
    {0x0101, 0x0202, 0x0404, 0x0808, 0x1010, 0x2020, 0x4040, 0x8080, 0x0101, 0x0202, 0x0404, 0x0808, 0x1010, 0x2020, 0x4040, 0x8080},  // 15
    {0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222, 0x2222},  // 16
    {0x0000, 0xffff, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000, 0x0000, 0xffff, 0x0000, 0x0000},  // 17
    {0x2222, 0xffff, 0x2222, 0x2222, 0x2222, 0xffff, 0x2222, 0x2222, 0x2222, 0xffff, 0x2222, 0x2222, 0x2222, 0xffff, 0x2222, 0x2222},  // 18
    {0xffff, 0xffff, 0x3333, 0x3333, 0xffff, 0xffff, 0x3333, 0x3333, 0xffff, 0xffff, 0x3333, 0x3333, 0xffff, 0xffff, 0x3333, 0x3333},  // 19
    {0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0xf0f0, 0xf0f0, 0xf0f0, 0xf0f0, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0xf0f0, 0xf0f0, 0xf0f0, 0xf0f0},  // 20
    {0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00},  // 21
    {0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0xffff, 0xffff, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180},  // 22
    {0x03c0, 0x03c0, 0x03c0, 0x03c0, 0x03c0, 0x03c0, 0xffff, 0xffff, 0xffff, 0xffff, 0x03c0, 0x03c0, 0x03c0, 0x03c0, 0x03c0, 0x03c0},  // 23
    {0x0404, 0x0404, 0x0404, 0xffff, 0x4040, 0x4040, 0x4040, 0xffff, 0x0404, 0x0404, 0x0404, 0xffff, 0x4040, 0x4040, 0x4040, 0xffff},  // 24
    {0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0xffff, 0xffff, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0xffff, 0xffff},  // 25
    {0x1111, 0xb8b8, 0x7c7c, 0x3a3a, 0x1111, 0xa3a3, 0xc7c7, 0x8b8b, 0x1111, 0xb8b8, 0x7c7c, 0x3a3a, 0x1111, 0xa3a3, 0xc7c7, 0x8b8b},  // 26
    {0x1010, 0x1010, 0x2828, 0xc7c7, 0x0101, 0x0101, 0x8282, 0x7c7c, 0x1010, 0x1010, 0x2828, 0xc7c7, 0x0101, 0x0101, 0x8282, 0x7c7c},  // 27
    {0x1c1c, 0x1212, 0x1111, 0x2121, 0xc1c1, 0x2121, 0x1111, 0x1212, 0x1c1c, 0x1212, 0x1111, 0x2121, 0xc1c1, 0x2121, 0x1111, 0x1212},  // 28
    {0x3e3e, 0x4141, 0x8080, 0x8080, 0xe3e3, 0x1414, 0x0808, 0x0808, 0x3e3e, 0x4141, 0x8080, 0x8080, 0xe3e3, 0x1414, 0x0808, 0x0808},  // 29
    {0x4848, 0x8888, 0x8484, 0x8383, 0x8484, 0x8888, 0x4848, 0x3838, 0x4848, 0x8888, 0x8484, 0x8383, 0x8484, 0x8888, 0x4848, 0x3838},  // 30
    {0x0303, 0x0404, 0x0808, 0x0808, 0x0c0c, 0x1212, 0x2121, 0xc0c0, 0x0303, 0x0404, 0x0808, 0x0808, 0x0c0c, 0x1212, 0x2121, 0xc0c0},  // 31
};

QBrush patternBrush(const QColor& c, int pattern)
{
    static QVector<QPixmap> cache;
    if (cache.isEmpty()) {
        for (int k = 0; k < 32; ++k) {
            QImage img(16, 16, QImage::Format_MonoLSB);
            img.setColor(0, qRgb(255, 255, 255));
            img.setColor(1, qRgb(0, 0, 0));
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) img.setPixel(x, y, (kGracePatterns[k][y] >> x) & 1);
            cache.push_back(QBitmap::fromImage(img));
        }
    }
    return QBrush(c, cache[std::clamp(pattern, 0, 31)]);
}
}  // namespace

// ====================================================================== helpers

namespace {

Qt::BrushStyle patternStyle(int pattern)
{
    if (pattern <= 0) return Qt::NoBrush;
    if (pattern == 1) return Qt::SolidPattern;
    static const Qt::BrushStyle map[] = {Qt::Dense1Pattern, Qt::Dense2Pattern, Qt::Dense3Pattern, Qt::Dense4Pattern,
                                         Qt::Dense5Pattern, Qt::Dense6Pattern, Qt::Dense7Pattern, Qt::HorPattern,
                                         Qt::VerPattern,    Qt::CrossPattern,  Qt::BDiagPattern,  Qt::FDiagPattern,
                                         Qt::DiagCrossPattern};
    return map[(pattern - 2) % 13];
}

int hjustOf(int just) { return just & 3; }
int vjustOf(int just)
{
    switch (just & 12) {
    case 4: return 1;   // bottom
    case 8: return 3;   // top
    case 12: return 2;  // middle
    default: return 0;  // baseline
    }
}

QPointF unitVec(const QPointF& a, const QPointF& b)
{
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double l = std::hypot(dx, dy);
    if (l <= 0) return QPointF(1, 0);
    return QPointF(dx / l, dy / l);
}

bool finite2(double a, double b) { return std::isfinite(a) && std::isfinite(b); }
QPainterPath polarSector(const Graph& g, const GraphGeometry& geo);

}  // namespace

QVector<qreal> PlotRenderer::dashPattern(int style)
{
    switch (style) {
    case 2: return {1, 3};
    case 3: return {5, 3};
    case 4: return {8, 4};
    case 5: return {1, 3, 5, 3};
    case 6: return {1, 4, 8, 4};
    case 7: return {1, 3, 1, 3, 5, 3};
    case 8: return {1, 3, 5, 3, 1, 3, 5, 3, 8, 3};
    default: return {};
    }
}

QPen PlotRenderer::makePen(const LineStyle& ls, double unit)
{
    if (!ls.visible()) return QPen(Qt::NoPen);
    const double w = fonts::lineWidthToUnits(ls.width, unit);
    QPen pen(ls.color, w > 0 ? w : 0.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    if (w <= 0) pen.setCosmetic(true);
    const auto dp = dashPattern(ls.style);
    if (!dp.isEmpty()) {
        pen.setCapStyle(Qt::FlatCap);
        // dash lengths are in units of the line width; keep them visible for thin lines
        const double minw = std::max(w, 0.0015 * unit);
        QVector<qreal> scaled;
        for (qreal d : dp) scaled.push_back(d * minw / std::max(w, 1e-9));
        pen.setDashPattern(scaled);
    }
    return pen;
}

QBrush PlotRenderer::makeBrush(const FillStyle& fs, const QRectF& area)
{
    if (!fs.visible()) return QBrush(Qt::NoBrush);
    if (fs.pattern == 1 && fs.gradient != GradientType::None && area.isValid()) {
        if (fs.gradient == GradientType::Radial) {
            QRadialGradient g(area.center(), 0.5 * std::hypot(area.width(), area.height()));
            g.setColorAt(0, fs.color);
            g.setColorAt(1, fs.color2);
            return QBrush(g);
        }
        QLinearGradient g = fs.gradient == GradientType::Vertical ? QLinearGradient(area.topLeft(), area.bottomLeft())
                                                                  : QLinearGradient(area.topLeft(), area.topRight());
        g.setColorAt(0, fs.color);
        g.setColorAt(1, fs.color2);
        return QBrush(g);
    }
    if (fs.pattern == 1) return QBrush(fs.color);
    if (QGuiApplication::instance()) return patternBrush(fs.color, fs.pattern);
    return QBrush(fs.color, patternStyle(fs.pattern));
}

QPainterPath PlotRenderer::symbolPath(Symbol s, double r)
{
    QPainterPath p;
    auto poly = [&](int n, double rad, double rot) {
        QPolygonF pg;
        for (int i = 0; i < n; ++i) {
            const double a = rot + 2 * M_PI * i / n;
            pg << QPointF(rad * std::cos(a), -rad * std::sin(a));
        }
        p.addPolygon(pg);
        p.closeSubpath();
    };
    switch (s) {
    case Symbol::Circle: p.addEllipse(QPointF(0, 0), r, r); break;
    case Symbol::Square: p.addRect(QRectF(-0.886 * r, -0.886 * r, 1.772 * r, 1.772 * r)); break;
    case Symbol::Diamond: poly(4, 1.2 * r, M_PI / 2); break;
    case Symbol::TriangleUp: poly(3, 1.25 * r, M_PI / 2); break;
    case Symbol::TriangleLeft: poly(3, 1.25 * r, M_PI); break;
    case Symbol::TriangleDown: poly(3, 1.25 * r, -M_PI / 2); break;
    case Symbol::TriangleRight: poly(3, 1.25 * r, 0); break;
    case Symbol::Pentagon: poly(5, 1.1 * r, M_PI / 2); break;
    case Symbol::Hexagon: poly(6, 1.05 * r, 0); break;
    case Symbol::Star5: {
        QPolygonF pg;
        for (int i = 0; i < 10; ++i) {
            const double a = M_PI / 2 + M_PI * i / 5;
            const double rad = (i % 2 == 0) ? 1.35 * r : 0.55 * r;
            pg << QPointF(rad * std::cos(a), -rad * std::sin(a));
        }
        p.addPolygon(pg);
        p.closeSubpath();
        break;
    }
    case Symbol::Plus:
    case Symbol::ThinPlus:
        p.moveTo(-r, 0); p.lineTo(r, 0);
        p.moveTo(0, -r); p.lineTo(0, r);
        break;
    case Symbol::Cross:
    case Symbol::ThinCross: {
        const double d = r * 0.8;
        p.moveTo(-d, -d); p.lineTo(d, d);
        p.moveTo(-d, d); p.lineTo(d, -d);
        break;
    }
    case Symbol::Star: {
        const double d = r * 0.71;
        p.moveTo(-r, 0); p.lineTo(r, 0);
        p.moveTo(0, -r); p.lineTo(0, r);
        p.moveTo(-d, -d); p.lineTo(d, d);
        p.moveTo(-d, d); p.lineTo(d, -d);
        break;
    }
    default: break;
    }
    return p;
}

// ====================================================================== renderer

PlotRenderer::PlotRenderer(const Project& project, RenderOptions opts) : m_proj(project), m_opt(opts)
{
    m_unit = project.page.unit();
}

double PlotRenderer::ts(double cs) const { return fonts::charSizeToUnits(cs, m_unit); }
double PlotRenderer::lw(double w) const { return fonts::lineWidthToUnits(w, m_unit); }

QRectF PlotRenderer::legendRect(int gi) const { return (gi >= 0 && gi < m_legendRects.size()) ? m_legendRects[gi] : QRectF(); }

void PlotRenderer::render(QPainter* p, const QRectF& target)
{
    const Page& pg = m_proj.page;
    p->save();
    p->translate(target.topLeft());
    p->scale(target.width() / pg.width, target.height() / pg.height);
    renderPage(p);
    p->restore();
}

void PlotRenderer::renderPage(QPainter* p)
{
    m_hits.clear();
    m_legendRects = QVector<QRectF>(m_proj.graphs.size());
    p->save();
    p->setRenderHint(QPainter::Antialiasing, m_opt.antialias);
    p->setRenderHint(QPainter::TextAntialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const Page& pg = m_proj.page;
    const QRectF pageRect(0, 0, pg.width, pg.height);
    if (m_opt.drawPageBackground && pg.backgroundFill && pg.background.visible()) {
        p->fillRect(pageRect, makeBrush(pg.background, pageRect));
    }
    for (int gi = 0; gi < m_proj.graphs.size(); ++gi) {
        const Graph& g = m_proj.graphs[gi];
        if (!g.active || g.hidden) continue;
        drawGraph(p, gi);
    }
    for (int oi = 0; oi < m_proj.objects.size(); ++oi) {
        if (m_proj.objects[oi].active) drawObject(p, oi, m_proj.objects[oi]);
    }
    if (m_proj.timestamp.active) {
        const Timestamp& t = m_proj.timestamp;
        const QPointF a(t.pos.x() * m_unit, pg.height - t.pos.y() * m_unit);
        QRectF r = richtext::drawText(p, t.value, t.text, ts(t.text.size), &m_proj, a, t.angle, 0, 0);
        if (m_opt.collectHits) {
            QPainterPath sh;
            sh.addRect(r);
            m_hits.push_back({HitItem::Timestamp, -1, -1, sh});
        }
    }
    if (m_opt.showRegions) drawRegions(p);
    p->restore();
}

void PlotRenderer::drawGraph(QPainter* p, int gi)
{
    const Graph& g = m_proj.graphs[gi];
    const GraphGeometry geo(m_proj.page, g);
    if (m_opt.collectHits) {
        QPainterPath sh;
        sh.addRect(geo.view);
        m_hits.push_back({HitItem::GraphArea, gi, -1, sh});
    }
    drawFrame(p, g, geo, true);
    if (!geo.valid) {
        drawFrame(p, g, geo, false);
        return;
    }

    if (g.type == GraphType::Pie) {
        drawPie(p, gi, g, geo);
        drawFrame(p, g, geo, false);
        drawTitles(p, gi, g, geo);
        drawLegend(p, gi, g, geo);
        return;
    }

    if (g.type == GraphType::Smith) drawSmithGrid(p, g, geo);
    else if (g.type != GraphType::Polar) drawGrid(p, g, geo);

    // data, clipped to the graph area
    p->save();
    if (g.type == GraphType::Polar) {
        p->setClipPath(polarSector(g, geo), Qt::IntersectClip);
    } else if (g.type == GraphType::Smith) {
        QPainterPath clip;
        clip.addEllipse(geo.center, geo.radius, geo.radius);
        p->setClipPath(clip, Qt::IntersectClip);
    } else {
        p->setClipRect(geo.view.adjusted(-0.5, -0.5, 0.5, 0.5), Qt::IntersectClip);
    }
    // pass 0: area fills of all sets (so they never hide other curves); pass 1: everything else
    for (int pass = 0; pass < 2; ++pass) {
        QVector<double> stackBase;
        for (int si = 0; si < g.sets.size(); ++si) {
            const DataSet& s = g.sets[si];
            if (!s.active || s.hidden || s.size() == 0) continue;
            drawSet(p, gi, g, si, geo, (g.type == GraphType::Chart && g.stacked) ? &stackBase : nullptr, pass);
        }
    }
    p->restore();

    if (g.type == GraphType::Polar) drawPolarAxes(p, gi, g, geo);
    else {
        drawFrame(p, g, geo, false);
        drawAxes(p, gi, g, geo);
    }
    drawColorBars(p, g, geo);
    drawTitles(p, gi, g, geo);
    drawLegend(p, gi, g, geo);

    if (m_opt.highlightGraph == gi) {
        p->save();
        const double s = 0.006 * m_unit;
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 120, 215));
        for (const QPointF& c : {geo.view.topLeft(), geo.view.topRight(), geo.view.bottomLeft(), geo.view.bottomRight()})
            p->drawRect(QRectF(c.x() - s / 2, c.y() - s / 2, s, s));
        p->restore();
    }
}

namespace {
/// Annular sector covering angles [xmin, xmax] and radii [ymin, ymax] of a polar graph.
QPainterPath polarSector(const Graph& g, const GraphGeometry& geo)
{
    const double R = geo.radius;
    const double r0 = g.wymax != 0 ? std::max(0.0, g.wymin / g.wymax) * R : 0.0;
    double a0 = qRadiansToDegrees(g.wxmin), a1 = qRadiansToDegrees(g.wxmax);
    if (g.xinvert) { a0 = -a0; a1 = -a1; }
    double span = a1 - a0;
    if (std::fabs(span) >= 360.0 - 1e-9) span = span > 0 ? 360.0 : -360.0;
    QPainterPath path;
    const QRectF outer(geo.center.x() - R, geo.center.y() - R, 2 * R, 2 * R);
    if (std::fabs(span) >= 360.0 && r0 <= 0) {
        path.addEllipse(outer);
        return path;
    }
    path.arcMoveTo(outer, a0);
    path.arcTo(outer, a0, span);
    if (r0 > 0) {
        const QRectF inner(geo.center.x() - r0, geo.center.y() - r0, 2 * r0, 2 * r0);
        QPainterPath tmp;
        tmp.arcMoveTo(inner, a0 + span);
        path.lineTo(tmp.currentPosition());
        path.arcTo(inner, a0 + span, -span);
    } else {
        path.lineTo(geo.center);
    }
    path.closeSubpath();
    return path;
}
}  // namespace

void PlotRenderer::drawFrame(QPainter* p, const Graph& g, const GraphGeometry& geo, bool background)
{
    const QRectF v = geo.view;
    if (background) {
        if (!g.frame.background.visible()) return;
        p->save();
        p->setPen(Qt::NoPen);
        p->setBrush(makeBrush(g.frame.background, v));
        if (g.type == GraphType::Smith)
            p->drawEllipse(geo.center, geo.radius, geo.radius);
        else
            p->drawRect(v);
        p->restore();
        return;
    }
    if (!g.frame.line.visible()) return;
    p->save();
    p->setPen(makePen(g.frame.line, m_unit));
    p->setBrush(Qt::NoBrush);
    if (g.type == GraphType::Polar) {
        p->drawPath(polarSector(g, geo));
        p->restore();
        return;
    }
    if (g.type == GraphType::Smith) {
        p->drawEllipse(geo.center, geo.radius, geo.radius);
        p->restore();
        return;
    }
    const QPointF tl = v.topLeft(), tr = v.topRight(), bl = v.bottomLeft(), br = v.bottomRight();
    switch (g.frame.type) {
    case FrameType::Closed: p->drawRect(v); break;
    case FrameType::HalfOpen: p->drawPolyline(QPolygonF{tl, bl, br}); break;
    case FrameType::BreakTop: p->drawPolyline(QPolygonF{tl, bl, br, tr}); break;
    case FrameType::BreakBottom: p->drawPolyline(QPolygonF{bl, tl, tr, br}); break;
    case FrameType::BreakLeft: p->drawPolyline(QPolygonF{tl, tr, br, bl}); break;
    case FrameType::BreakRight: p->drawPolyline(QPolygonF{tr, tl, bl, br}); break;
    }
    p->restore();
}

void PlotRenderer::drawGrid(QPainter* p, const Graph& g, const GraphGeometry& geo)
{
    p->save();
    p->setClipRect(geo.view, Qt::IntersectClip);
    for (int a = 0; a < 4; ++a) {
        const Axis& ax = g.axes[a];
        if (!ax.active) continue;
        if (!ax.majorProps.grid && !ax.minorProps.grid) continue;
        const bool isX = (a % 2 == 0);
        const QVector<Tick> tks = ticks::compute(ax, isX ? geo.sx : geo.sy, &m_proj);
        for (int pass = 0; pass < 2; ++pass) {  // minor first, then major on top
            const bool major = pass == 1;
            const TickProps& tp = major ? ax.majorProps : ax.minorProps;
            if (!tp.grid) continue;
            p->setPen(makePen(tp.gridLine, m_unit));
            for (const Tick& t : tks) {
                if (t.major != major) continue;
                if (isX) {
                    const double x = geo.map(t.value, geo.sy.wmin).x();
                    p->drawLine(QPointF(x, geo.view.top()), QPointF(x, geo.view.bottom()));
                } else {
                    const double y = geo.map(geo.sx.wmin, t.value).y();
                    p->drawLine(QPointF(geo.view.left(), y), QPointF(geo.view.right(), y));
                }
            }
        }
    }
    p->restore();
}

void PlotRenderer::drawAxes(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo)
{
    const QRectF v = geo.view;
    for (int a = 0; a < 4; ++a) {
        const Axis& ax = g.axes[a];
        if (!ax.active) continue;
        const bool isX = (a % 2 == 0);
        const Scale& sc = isX ? geo.sx : geo.sy;
        const QVector<Tick> tks = ticks::compute(ax, sc, &m_proj);

        struct Side {
            double pos;      // y for X axes, x for Y axes
            QPointF out;     // outward unit vector
            bool normal;
        };
        QVector<Side> sides;
        if (ax.zero) {
            const double zp = isX ? geo.map(geo.sx.wmin, 0.0).y() : geo.map(0.0, geo.sy.wmin).x();
            const bool inside = isX ? geo.sy.valid(0.0) && zp >= v.top() - 0.5 && zp <= v.bottom() + 0.5
                                    : geo.sx.valid(0.0) && zp >= v.left() - 0.5 && zp <= v.right() + 0.5;
            if (!inside) continue;
            sides.push_back({zp, isX ? QPointF(0, 1) : QPointF(-1, 0), true});
        } else {
            const double offN = ax.offset.x() * m_unit, offO = ax.offset.y() * m_unit;
            const Side normal = isX ? Side{v.bottom() + offN, QPointF(0, 1), true} : Side{v.left() - offN, QPointF(-1, 0), true};
            const Side opp = isX ? Side{v.top() - offO, QPointF(0, -1), false} : Side{v.right() + offO, QPointF(1, 0), false};
            // alternate axes default to the opposite side
            const bool alt = (a >= 2);
            sides.push_back(alt ? opp : normal);
            sides.push_back(alt ? normal : opp);
        }
        auto axisPoint = [&](const Side& s, double value) {
            return isX ? QPointF(geo.map(value, geo.sy.wmin).x(), s.pos) : QPointF(s.pos, geo.map(geo.sx.wmin, value).y());
        };
        auto wantSide = [&](SideOp op, int idx) {
            if (sides.size() == 1) return idx == 0 && op != SideOp::Opposite;
            if (op == SideOp::Both) return true;
            return (op == SideOp::Normal) ? idx == 0 : idx == 1;
        };
        // axis bars
        if (ax.barOn && ax.bar.visible()) {
            p->setPen(makePen(ax.bar, m_unit));
            for (int si = 0; si < sides.size(); ++si) {
                if (!wantSide(ax.tickSide, si) && !(si == 0 && ax.zero)) continue;
                const Side& s = sides[si];
                if (isX) p->drawLine(QPointF(v.left(), s.pos), QPointF(v.right(), s.pos));
                else p->drawLine(QPointF(s.pos, v.top()), QPointF(s.pos, v.bottom()));
            }
        }
        // ticks
        const double majLen = ax.majorProps.size * 0.02 * m_unit;
        const double minLen = ax.minorProps.size * 0.02 * m_unit;
        if (ax.tickOn) {
            for (int si = 0; si < sides.size(); ++si) {
                if (!wantSide(ax.tickSide, si)) continue;
                const Side& s = sides[si];
                for (int pass = 0; pass < 2; ++pass) {
                    const bool major = pass == 1;
                    const TickProps& tp = major ? ax.majorProps : ax.minorProps;
                    if (!tp.line.visible()) continue;
                    QPen pen = makePen(tp.line, m_unit);
                    pen.setCapStyle(Qt::FlatCap);
                    p->setPen(pen);
                    const double len = major ? majLen : minLen;
                    for (const Tick& t : tks) {
                        if (t.major != major) continue;
                        const QPointF base = axisPoint(s, t.value);
                        QPointF a1 = base, a2 = base;
                        if (ax.tickDir == TickDirection::In || ax.tickDir == TickDirection::Both) a1 = base - s.out * len;
                        if (ax.tickDir == TickDirection::Out || ax.tickDir == TickDirection::Both) a2 = base + s.out * len;
                        p->drawLine(a1, a2);
                    }
                }
            }
        }
        // tick labels
        QVector<double> extent(sides.size(), 0.0);
        QRectF labelArea;
        if (ax.ticklabelOn) {
            const double size = ts(ax.ticklabelText.size);
            const bool outTicks = ax.tickOn && (ax.tickDir != TickDirection::In) && wantSide(ax.tickSide, 0);
            for (int si = 0; si < sides.size(); ++si) {
                if (!wantSide(ax.ticklabelSide, si)) continue;
                const Side& s = sides[si];
                double gap = 0.012 * m_unit;
                if (outTicks || (si == 1 && ax.tickOn && ax.tickDir != TickDirection::In && wantSide(ax.tickSide, 1))) gap += majLen;
                int k = 0;
                for (const Tick& t : tks) {
                    if (!t.major || t.label.isEmpty()) continue;
                    QPointF anchor = axisPoint(s, t.value);
                    double perp = gap;
                    if (!ax.ticklabelOffsetAuto) perp = ax.ticklabelOffset.y() * m_unit;
                    const TextLayout lay = richtext::layout(t.label, ax.ticklabelText.font, size, ax.ticklabelText.color, &m_proj, p->device());
                    if (ax.ticklabelStagger > 0) perp += (k % (ax.ticklabelStagger + 1)) * (lay.bounds.height() * 1.1);
                    ++k;
                    anchor += s.out * perp;
                    if (!ax.ticklabelOffsetAuto) anchor += (isX ? QPointF(1, 0) : QPointF(0, -1)) * ax.ticklabelOffset.x() * m_unit;
                    int hj = 2, vj = 2;
                    const double ang = ax.ticklabelAngle;
                    if (isX) {
                        if (ang == 0.0) { hj = 2; vj = s.out.y() > 0 ? 3 : 1; }
                        else if (std::fabs(ang - 90.0) < 1e-6 || std::fabs(ang + 270.0) < 1e-6) { hj = s.out.y() > 0 ? 1 : 0; vj = 2; }
                        else { hj = (s.out.y() > 0) == (std::sin(qDegreesToRadians(ang)) > 0) ? 1 : 0; vj = 2; }
                    } else {
                        if (ang == 0.0) { hj = s.out.x() < 0 ? 1 : 0; vj = 2; }
                        else if (std::fabs(ang - 90.0) < 1e-6) { hj = 2; vj = s.out.x() < 0 ? 1 : 3; }
                        else { hj = s.out.x() < 0 ? 1 : 0; vj = 2; }
                    }
                    richtext::draw(p, lay, anchor, ang, hj, vj);
                    const QRectF br = richtext::boundingRect(lay, anchor, ang, hj, vj);
                    labelArea = labelArea.isNull() ? br : labelArea.united(br);
                    const double e = isX ? (s.out.y() > 0 ? br.bottom() - s.pos : s.pos - br.top())
                                         : (s.out.x() < 0 ? s.pos - br.left() : br.right() - s.pos);
                    extent[si] = std::max(extent[si], e);
                }
            }
        }
        if (!labelArea.isNull()) {  // always recorded: titles avoid these areas
            QPainterPath sh;
            sh.addRect(labelArea);
            m_hits.push_back({HitItem::AxisTicks, gi, a, sh});
        }
        // axis label
        if (!ax.label.trimmed().isEmpty()) {
            const double size = ts(ax.labelText.size);
            const TextLayout lay = richtext::layout(ax.label, ax.labelText.font, size, ax.labelText.color, &m_proj, p->device());
            for (int si = 0; si < sides.size(); ++si) {
                const bool want = (ax.labelSide == SideOp::Both) || (ax.labelSide == SideOp::Normal ? si == 0 : si == 1);
                if (!want || (sides.size() == 1 && si > 0)) continue;
                const Side& s = sides[si];
                QPointF anchor = isX ? QPointF(v.center().x(), s.pos) : QPointF(s.pos, v.center().y());
                double perp = std::max(extent[si], (ax.tickOn && ax.tickDir != TickDirection::In) ? majLen : 0.0) + 0.012 * m_unit;
                if (!ax.labelPlaceAuto) perp = ax.labelOffset.y() * m_unit;
                anchor += s.out * perp;
                if (!ax.labelPlaceAuto) anchor += (isX ? QPointF(1, 0) : QPointF(0, -1)) * ax.labelOffset.x() * m_unit;
                double ang = 0.0;
                int hj = 2, vj = 2;
                if (isX) {
                    vj = s.out.y() > 0 ? 3 : 1;
                    if (ax.labelLayout == LabelLayout::Perpendicular) { ang = 90; hj = s.out.y() > 0 ? 1 : 0; vj = 2; }
                } else {
                    if (ax.labelLayout == LabelLayout::Parallel) { ang = 90; hj = 2; vj = s.out.x() < 0 ? 1 : 3; }
                    else { hj = s.out.x() < 0 ? 1 : 0; vj = 2; }
                }
                richtext::draw(p, lay, anchor, ang, hj, vj);
                {
                    QPainterPath sh;
                    sh.addRect(richtext::boundingRect(lay, anchor, ang, hj, vj));
                    m_hits.push_back({HitItem::AxisLabel, gi, a, sh});
                }
            }
        }
    }
}

void PlotRenderer::drawPolarAxes(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo)
{
    Q_UNUSED(gi);
    const Axis& ax = g.axes[Graph::X];   // angular
    const Axis& ay = g.axes[Graph::Y];   // radial
    const double R = geo.radius;
    const QPointF c = geo.center;
    // radial grid circles and angular grid lines
    const Scale rs(ScaleType::Linear, g.wymin, g.wymax);
    const Scale as(ScaleType::Linear, g.wxmin, g.wxmax);
    Axis angAxis = ax;
    if (angAxis.tickAuto && (!(angAxis.major > 0) || (g.wxmax - g.wxmin) / angAxis.major > 72)) angAxis.major = M_PI / 6;
    const QVector<Tick> rt = ticks::compute(ay, rs, &m_proj);
    const QVector<Tick> at = ticks::compute(angAxis, as, &m_proj);
    p->save();
    if (ay.majorProps.grid || ay.minorProps.grid) {
        for (const Tick& t : rt) {
            const TickProps& tp = t.major ? ay.majorProps : ay.minorProps;
            if (!tp.grid) continue;
            const double r = t.value / g.wymax * R;
            p->setPen(makePen(tp.gridLine, m_unit));
            p->setBrush(Qt::NoBrush);
            QPainterPath ring;
            const QRectF rr(c.x() - r, c.y() - r, 2 * r, 2 * r);
            const double a0 = qRadiansToDegrees(g.xinvert ? -g.wxmin : g.wxmin);
            const double span = qRadiansToDegrees(g.xinvert ? -(g.wxmax - g.wxmin) : (g.wxmax - g.wxmin));
            ring.arcMoveTo(rr, a0);
            ring.arcTo(rr, a0, span);
            p->drawPath(ring);
        }
    }
    if (ax.majorProps.grid || ax.minorProps.grid) {
        for (const Tick& t : at) {
            const TickProps& tp = t.major ? ax.majorProps : ax.minorProps;
            if (!tp.grid) continue;
            p->setPen(makePen(tp.gridLine, m_unit));
            p->drawLine(geo.map(t.value, std::max(0.0, g.wymin)), geo.map(t.value, g.wymax));
        }
    }
    drawFrame(p, g, geo, false);
    // angular ticks and labels around the circle
    if (ax.active) {
        const double size = ts(ax.ticklabelText.size);
        for (const Tick& t : at) {
            const double phi = g.xinvert ? -t.value : t.value;
            const QPointF dir(std::cos(phi), -std::sin(phi));
            const double len = (t.major ? ax.majorProps.size : ax.minorProps.size) * 0.02 * m_unit;
            if (ax.tickOn) {
                p->setPen(makePen(t.major ? ax.majorProps.line : ax.minorProps.line, m_unit));
                const QPointF base = c + dir * R;
                p->drawLine(base - dir * (ax.tickDir != TickDirection::Out ? len : 0), base + dir * (ax.tickDir != TickDirection::In ? len : 0));
            }
            if (t.major && ax.ticklabelOn && !t.label.isEmpty()) {
                if (std::fabs(std::fmod(t.value - g.wxmin, 2 * M_PI)) < 1e-9 && t.value != g.wxmin) continue;
                QString label = t.label;
                if (ax.tickAuto && ax.format == LabelFormat::General && m_proj.prettyNumbers)
                    label = QString::number(std::lround(qRadiansToDegrees(t.value))) + QChar(0x00B0);
                const QPointF anchor = c + dir * (R + 0.012 * m_unit + (ax.tickDir != TickDirection::In ? len : 0));
                const int hj = std::fabs(dir.x()) < 0.2 ? 2 : (dir.x() > 0 ? 0 : 1);
                const int vj = std::fabs(dir.y()) < 0.2 ? 2 : (dir.y() > 0 ? 3 : 1);
                richtext::drawText(p, label, ax.ticklabelText, size, &m_proj, anchor, 0, hj, vj);
            }
        }
    }
    // radial axis along angle 0
    if (ay.active) {
        const double size = ts(ay.ticklabelText.size);
        if (ay.barOn) {
            p->setPen(makePen(ay.bar, m_unit));
            p->drawLine(c + QPointF(std::max(0.0, g.wymin / g.wymax) * R, 0), c + QPointF(R, 0));
        }
        for (const Tick& t : rt) {
            const double r = t.value / g.wymax * R;
            const QPointF base = c + QPointF(r, 0);
            const double len = (t.major ? ay.majorProps.size : ay.minorProps.size) * 0.02 * m_unit;
            if (ay.tickOn) {
                p->setPen(makePen(t.major ? ay.majorProps.line : ay.minorProps.line, m_unit));
                p->drawLine(base, base + QPointF(0, len));
            }
            if (t.major && ay.ticklabelOn && !t.label.isEmpty())
                richtext::drawText(p, t.label, ay.ticklabelText, size, &m_proj, base + QPointF(0, len + 0.008 * m_unit), 0, 2, 3);
        }
        if (!ay.label.isEmpty())
            richtext::drawText(p, ay.label, ay.labelText, ts(ay.labelText.size), &m_proj,
                               c + QPointF(R * 0.5, 0.07 * m_unit), 0, 2, 3);
    }
    p->restore();
}

void PlotRenderer::drawSmithGrid(QPainter* p, const Graph& g, const GraphGeometry& geo)
{
    const QPointF c = geo.center;
    const double R = geo.radius;
    LineStyle gl = g.axes[0].majorProps.gridLine;
    if (!gl.visible()) gl = LineStyle{QColor(200, 200, 200), 0.5, 1};
    p->save();
    QPainterPath clip;
    clip.addEllipse(c, R, R);
    p->setClipPath(clip, Qt::IntersectClip);
    p->setPen(makePen(gl, m_unit));
    p->setBrush(Qt::NoBrush);
    // constant resistance circles: centre (r/(1+r), 0), radius 1/(1+r)
    for (double r : {0.2, 0.5, 1.0, 2.0, 5.0}) {
        const double cx = r / (1 + r), rad = 1 / (1 + r);
        p->drawEllipse(QPointF(c.x() + cx * R, c.y()), rad * R, rad * R);
    }
    // constant reactance arcs: centre (1, 1/x), radius 1/|x|
    for (double x : {0.2, 0.5, 1.0, 2.0, 5.0, -0.2, -0.5, -1.0, -2.0, -5.0}) {
        const double rad = 1 / std::fabs(x);
        p->drawEllipse(QPointF(c.x() + R, c.y() - R / x), rad * R, rad * R);
    }
    p->drawLine(QPointF(c.x() - R, c.y()), QPointF(c.x() + R, c.y()));
    p->restore();
    // labels on the real axis
    const TextStyle tsx = g.axes[0].ticklabelText;
    for (double r : {0.0, 0.2, 0.5, 1.0, 2.0, 5.0}) {
        const double gx = (r - 1) / (r + 1);
        richtext::drawText(p, QString::number(r), tsx, ts(tsx.size) * 0.7, &m_proj,
                           QPointF(c.x() + gx * R + 0.003 * m_unit, c.y() - 0.004 * m_unit), 90, 0, 1);
    }
    drawFrame(p, g, geo, false);
}

void PlotRenderer::drawSymbol(QPainter* p, const DataSet& s, const QPointF& pt, double sizeFactor, const QColor* fillOverride)
{
    const SymbolStyle& sy = s.symbol;
    if (sy.type == Symbol::None) return;
    const double r = 0.01 * m_unit * sy.size * sizeFactor;
    if (r <= 0) return;
    if (sy.type == Symbol::Char) {
        const QString ch = QString(QChar(ushort(sy.ch)));
        TextStyle st{sy.charFont, 1.0, sy.line.color};
        if (fillOverride) st.color = *fillOverride;
        richtext::drawText(p, ch, st, 2.0 * r, &m_proj, pt, 0, 2, 2);
        return;
    }
    QPainterPath path = symbolPath(sy.type, r);
    path.translate(pt);
    const bool strokeOnly = sy.type == Symbol::Plus || sy.type == Symbol::Cross || sy.type == Symbol::Star ||
                            sy.type == Symbol::ThinPlus || sy.type == Symbol::ThinCross;
    QPen pen = makePen(sy.line, m_unit);
    if (sy.type == Symbol::ThinPlus || sy.type == Symbol::ThinCross) pen.setWidthF(pen.widthF() * 0.5);
    if (strokeOnly && fillOverride) pen.setColor(*fillOverride);
    p->setPen(pen);
    if (strokeOnly) {
        p->setBrush(Qt::NoBrush);
    } else if (fillOverride) {
        p->setBrush(*fillOverride);
    } else {
        p->setBrush(makeBrush(sy.fill));
    }
    p->drawPath(path);
}

namespace {

/// Points of a set in page coordinates; invalid points become NaN (break the line).
QVector<QPointF> pagePoints(const DataSet& s, const GraphGeometry& geo, double xoff, const QVector<double>* ybase)
{
    const int n = s.size();
    QVector<QPointF> pts(n);
    const auto& X = s.cols[0];
    const auto& Y = s.cols[1];
    for (int i = 0; i < n; ++i) {
        double x = X[i];
        double y = i < Y.size() ? Y[i] : 0.0;
        if (ybase && i < ybase->size()) y += (*ybase)[i];
        if (!finite2(x, y) || (geo.type != GraphType::Polar && (!geo.sx.valid(x) || !geo.sy.valid(y)))) {
            pts[i] = QPointF(NAN, NAN);
            continue;
        }
        QPointF q = geo.map(x, y);
        q.rx() += xoff;
        pts[i] = q;
    }
    return pts;
}

bool validPt(const QPointF& p) { return std::isfinite(p.x()) && std::isfinite(p.y()); }

/// Build the polylines for a line type; each polyline is drawn separately.
QVector<QPolygonF> buildLines(const QVector<QPointF>& pts, LineType lt)
{
    QVector<QPolygonF> out;
    QPolygonF cur;
    auto finish = [&]() {
        if (cur.size() >= 2) out.push_back(cur);
        cur.clear();
    };
    const int n = pts.size();
    switch (lt) {
    case LineType::None: break;
    case LineType::Segments:
        for (int i = 0; i + 1 < n; i += 2)
            if (validPt(pts[i]) && validPt(pts[i + 1])) out.push_back(QPolygonF{pts[i], pts[i + 1]});
        break;
    case LineType::ThreeSegments:
        for (int i = 0; i + 2 < n; i += 3)
            if (validPt(pts[i]) && validPt(pts[i + 1]) && validPt(pts[i + 2])) out.push_back(QPolygonF{pts[i], pts[i + 1], pts[i + 2]});
        break;
    default: {
        QPointF prev;
        bool have = false;
        for (int i = 0; i < n; ++i) {
            const QPointF q = pts[i];
            if (!validPt(q)) { finish(); have = false; continue; }
            if (!have) { cur << q; prev = q; have = true; continue; }
            switch (lt) {
            case LineType::LeftStairs: cur << QPointF(prev.x(), q.y()) << q; break;
            case LineType::RightStairs: cur << QPointF(q.x(), prev.y()) << q; break;
            case LineType::CenterStairs: {
                const double mx = 0.5 * (prev.x() + q.x());
                cur << QPointF(mx, prev.y()) << QPointF(mx, q.y()) << q;
                break;
            }
            default:
                // decimate: skip points that do not move by at least 0.1 pt
                if (std::fabs(q.x() - cur.last().x()) + std::fabs(q.y() - cur.last().y()) < 0.1 && i + 1 < n) { prev = q; continue; }
                cur << q;
                break;
            }
            prev = q;
        }
        finish();
    }
    }
    return out;
}

}  // namespace

void PlotRenderer::drawSet(QPainter* p, int gi, const Graph& g, int si, const GraphGeometry& geo, const QVector<double>* stackBaseIn, int pass)
{
    const DataSet& s = g.sets[si];
    const int n = s.size();
    const bool chart = (g.type == GraphType::Chart);
    const bool barType = s.type == SetType::BAR || s.type == SetType::BARDY || s.type == SetType::BARDYDY;
    const double barW = std::max(0.5, s.symbol.size * 0.02 * m_unit);

    // side-by-side offset in non-stacked charts
    double xoff = 0.0;
    if (chart && !g.stacked) {
        int idx = 0, count = 0;
        for (int k = 0; k < g.sets.size(); ++k) {
            const DataSet& o = g.sets[k];
            if (!o.active || o.hidden || o.size() == 0) continue;
            if (k < si) ++idx;
            ++count;
        }
        const double step = barW * (1.0 + g.barHGap);
        xoff = (idx - 0.5 * (count - 1)) * step;
    }
    QVector<double>* stackBase = const_cast<QVector<double>*>(stackBaseIn);
    QVector<double> base;
    if (stackBase) {
        base = *stackBase;
        base.resize(n);
    }
    const QVector<QPointF> pts = pagePoints(s, geo, xoff, stackBase ? &base : nullptr);

    // colours for XYCOLOR/XYCOLPAT
    QVector<QColor> pointColors;
    if (s.type == SetType::XYCOLOR || s.type == SetType::XYCOLPAT) {
        pointColors.resize(n);
        const auto& Cc = s.cols[2];
        if (s.colorFromMap && s.type == SetType::XYCOLOR) {
            double lo = s.cmapMin, hi = s.cmapMax;
            if (s.cmapAuto && !Cc.isEmpty()) {
                lo = HUGE_VAL; hi = -HUGE_VAL;
                for (double c : Cc) if (std::isfinite(c)) { lo = std::min(lo, c); hi = std::max(hi, c); }
            }
            const Colormap* cm = colors::colormap(s.colormap);
            if (!cm) cm = colors::colormap(QStringLiteral("viridis"));
            for (int i = 0; i < n; ++i) {
                const double t = hi > lo ? (Cc.value(i) - lo) / (hi - lo) : 0.5;
                pointColors[i] = cm ? cm->at(t, 0.0, 1.0, s.cmapReversed) : QColor(Qt::black);
            }
        } else {
            for (int i = 0; i < n; ++i) pointColors[i] = m_proj.colorAt(int(std::lround(Cc.value(i))));
        }
    }

    // ---- fill (pass 0 only)
    if (pass == 0 && s.fillType != FillType::None && s.fill.visible() && !barType && n > 1) {
        QPolygonF poly;
        const auto lines = buildLines(pts, s.lineType == LineType::None ? LineType::Straight : s.lineType);
        for (const QPolygonF& l : lines) poly << l;
        if (poly.size() >= 2) {
            if (s.fillType == FillType::ToBaseline) {
                double by = 0.0;
                switch (s.baselineType) {
                case BaselineType::Zero: by = 0.0; break;
                case BaselineType::SetMin: by = *std::min_element(s.cols[1].begin(), s.cols[1].end()); break;
                case BaselineType::SetMax: by = *std::max_element(s.cols[1].begin(), s.cols[1].end()); break;
                case BaselineType::GraphMin: by = g.wymin; break;
                case BaselineType::GraphMax: by = g.wymax; break;
                case BaselineType::SetAverage: {
                    double sum = 0;
                    for (double v : s.cols[1]) sum += v;
                    by = sum / std::max(1, n);
                    break;
                }
                }
                double pageY;
                if (stackBase) {
                    // fill down to the previous stacked curve
                    QPolygonF lower;
                    for (int i = n - 1; i >= 0; --i) {
                        if (!validPt(pts[i])) continue;
                        lower << geo.map(s.cols[0][i], base.value(i)) + QPointF(xoff, 0);
                    }
                    poly << lower;
                } else {
                    if (!geo.sy.valid(by)) by = g.wymin;
                    pageY = geo.map(g.wxmin, by).y();
                    poly << QPointF(poly.last().x(), pageY) << QPointF(poly.first().x(), pageY);
                }
            }
            p->setPen(Qt::NoPen);
            p->setBrush(makeBrush(s.fill, poly.boundingRect()));
            p->drawPolygon(poly, s.fillRule == FillRule::EvenOdd ? Qt::OddEvenFill : Qt::WindingFill);
        }
    }

    if (pass == 0) {
        if (stackBase) {
            stackBase->resize(std::max(int(stackBase->size()), n));
            for (int i = 0; i < n; ++i) (*stackBase)[i] += s.cols[1][i];
        }
        return;
    }

    // ---- bars
    if (barType) {
        const QPen pen = makePen(s.symbol.line, m_unit);
        for (int i = 0; i < n; ++i) {
            if (!validPt(pts[i])) continue;
            double y0 = stackBase ? base.value(i) : 0.0;
            if (!geo.sy.valid(y0)) y0 = g.wymin;
            const QPointF b = geo.map(s.cols[0][i], y0) + QPointF(xoff, 0);
            QRectF r(QPointF(pts[i].x() - barW / 2, pts[i].y()), QPointF(pts[i].x() + barW / 2, b.y()));
            r = r.normalized();
            p->setPen(pen);
            p->setBrush(pointColors.isEmpty() ? makeBrush(s.symbol.fill, r) : QBrush(pointColors[i]));
            p->drawRect(r);
        }
    }

    // ---- special set types
    switch (s.type) {
    case SetType::XYHILO: {
        QPen pen = makePen(s.line.visible() ? s.line : s.symbol.line, m_unit);
        p->setPen(pen);
        const double tick = 0.01 * m_unit * s.symbol.size;
        for (int i = 0; i < n; ++i) {
            const double x = s.cols[0][i];
            const QPointF hi = geo.map(x, s.cols[1].value(i)), lo = geo.map(x, s.cols[2].value(i));
            const QPointF op = geo.map(x, s.cols[3].value(i)), cl = geo.map(x, s.cols[4].value(i));
            p->drawLine(lo, hi);
            p->drawLine(op, op - QPointF(tick, 0));
            p->drawLine(cl, cl + QPointF(tick, 0));
        }
        break;
    }
    case SetType::XYBOXPLOT: {
        const double hw = std::max(1.0, 0.01 * m_unit * s.symbol.size);
        QPen boxPen = makePen(s.symbol.line, m_unit);
        QPen whiskerPen = makePen(LineStyle{s.errorbar.color, s.errorbar.riserWidth, s.errorbar.riserStyle}, m_unit);
        QPen capPen = makePen(LineStyle{s.errorbar.color, s.errorbar.width, s.errorbar.style}, m_unit);
        const double cap = hw * s.errorbar.size;
        for (int i = 0; i < n; ++i) {
            const double x = s.cols[0][i];
            const QPointF med = geo.map(x, s.cols[1].value(i)), q1 = geo.map(x, s.cols[2].value(i));
            const QPointF q3 = geo.map(x, s.cols[3].value(i)), mn = geo.map(x, s.cols[4].value(i)), mx = geo.map(x, s.cols[5].value(i));
            if (s.errorbar.active) {
                p->setPen(whiskerPen);
                p->drawLine(q1, mn);
                p->drawLine(q3, mx);
                p->setPen(capPen);
                p->drawLine(mn - QPointF(cap * 0.5, 0), mn + QPointF(cap * 0.5, 0));
                p->drawLine(mx - QPointF(cap * 0.5, 0), mx + QPointF(cap * 0.5, 0));
            }
            const QRectF box(QPointF(q1.x() - hw, q3.y()), QPointF(q1.x() + hw, q1.y()));
            p->setPen(boxPen);
            p->setBrush(makeBrush(s.symbol.fill, box));
            p->drawRect(box.normalized());
            p->drawLine(QPointF(med.x() - hw, med.y()), QPointF(med.x() + hw, med.y()));
        }
        break;
    }
    case SetType::XYR: {
        p->setPen(makePen(s.line, m_unit));
        p->setBrush(s.fillType != FillType::None ? makeBrush(s.fill) : QBrush(Qt::NoBrush));
        for (int i = 0; i < n; ++i) {
            const double x = s.cols[0][i], y = s.cols[1][i], r = s.cols[2].value(i);
            const QPointF c = geo.map(x, y);
            const double rx = std::fabs(geo.map(x + r, y).x() - c.x());
            const double ry = std::fabs(geo.map(x, y + r).y() - c.y());
            if (validPt(c)) p->drawEllipse(c, rx, ry);
        }
        break;
    }
    case SetType::XYVMAP: {
        QPen pen = makePen(LineStyle{s.errorbar.color, s.errorbar.width, s.errorbar.style}, m_unit);
        p->setPen(pen);
        p->setBrush(s.errorbar.color);
        for (int i = 0; i < n; ++i) {
            if (!validPt(pts[i])) continue;
            const QPointF d(s.cols[2].value(i) * g.znorm * m_unit, -s.cols[3].value(i) * g.znorm * m_unit);
            const QPointF tip = pts[i] + d;
            p->drawLine(pts[i], tip);
            const double L = std::hypot(d.x(), d.y());
            if (L > 0) {
                const QPointF u = d / L, nrm(-u.y(), u.x());
                const double ah = std::min(L * 0.4, 0.012 * m_unit * s.errorbar.size);
                QPolygonF head{tip, tip - u * ah + nrm * ah * 0.35, tip - u * ah - nrm * ah * 0.35};
                p->drawPolygon(head);
            }
        }
        break;
    }
    default: break;
    }

    // ---- connecting line
    if (s.lineType != LineType::None && s.line.visible() && s.type != SetType::XYHILO && s.type != SetType::XYR) {
        p->setPen(makePen(s.line, m_unit));
        p->setBrush(Qt::NoBrush);
        const auto lines = buildLines(pts, s.lineType);
        for (const QPolygonF& l : lines) p->drawPolyline(l);
    }

    // ---- drop lines and baseline
    if (s.dropLines) {
        p->setPen(makePen(s.line, m_unit));
        for (int i = 0; i < n; ++i) {
            if (!validPt(pts[i])) continue;
            double y0 = stackBase ? base.value(i) : 0.0;
            if (!geo.sy.valid(y0) || y0 < g.wymin) y0 = g.wymin;
            p->drawLine(pts[i], geo.map(s.cols[0][i], y0) + QPointF(xoff, 0));
        }
    }
    if (s.baselineShown) {
        p->setPen(makePen(s.line, m_unit));
        double by = 0.0;
        if (s.baselineType == BaselineType::GraphMin) by = g.wymin;
        if (geo.sy.valid(by)) {
            const double y = geo.map(g.wxmin, by).y();
            p->drawLine(QPointF(geo.view.left(), y), QPointF(geo.view.right(), y));
        }
    }

    // ---- error bars
    if (setTypeHasErrorBars(s.type) && s.errorbar.active) drawErrorBars(p, s, geo, xoff, stackBase ? &base : nullptr);

    // ---- symbols
    if (s.symbol.type != Symbol::None && !barType && s.type != SetType::XYBOXPLOT) {
        const int skip = std::max(0, s.symbol.skip) + 1;
        for (int i = 0; i < n; i += skip) {
            if (!validPt(pts[i])) continue;
            double factor = 1.0;
            if (s.type == SetType::XYSIZE) factor = s.cols[2].value(i) / (g.znorm != 0 ? g.znorm : 1.0);
            drawSymbol(p, s, pts[i], factor, pointColors.isEmpty() ? nullptr : &pointColors[i]);
        }
    }

    // ---- annotated values
    if (s.avalue.active) drawAnnotatedValues(p, s, geo, pts, stackBase ? &base : nullptr);

    if (stackBase) {
        stackBase->resize(std::max(int(stackBase->size()), n));
        for (int i = 0; i < n; ++i) (*stackBase)[i] += s.cols[1][i];
    }

    if (m_opt.collectHits) {
        QPainterPath path;
        const int step = std::max(1, n / 2000);
        bool first = true;
        for (int i = 0; i < n; i += step) {
            if (!validPt(pts[i])) { first = true; continue; }
            if (first) { path.moveTo(pts[i]); first = false; }
            else path.lineTo(pts[i]);
        }
        if (n == 1 && validPt(pts[0])) path.addEllipse(pts[0], 2, 2);
        QPainterPathStroker st;
        st.setWidth(std::max(6.0, 0.012 * m_unit));
        QPainterPath shape = st.createStroke(path);
        if (barType) {
            for (int i = 0; i < n; i += step) {
                if (!validPt(pts[i])) continue;
                const QPointF b = geo.map(s.cols[0][i], 0.0) + QPointF(xoff, 0);
                shape.addRect(QRectF(QPointF(pts[i].x() - barW / 2, pts[i].y()), QPointF(pts[i].x() + barW / 2, b.y())).normalized());
            }
        }
        m_hits.push_back({HitItem::SetCurve, gi, si, shape});
    }
}

void PlotRenderer::drawErrorBars(QPainter* p, const DataSet& s, const GraphGeometry& geo, double xoff, const QVector<double>* stackBase)
{
    const ErrorBarStyle& e = s.errorbar;
    const QPen riser = makePen(LineStyle{e.color, e.riserWidth, e.riserStyle}, m_unit);
    const QPen capPen = makePen(LineStyle{e.color, e.width, e.style}, m_unit);
    const double cap = 0.008 * m_unit * e.size;
    const bool plus = e.placement != ErrorBarPlacement::Bottom;
    const bool minus = e.placement != ErrorBarPlacement::Top;
    const int n = s.size();
    auto col = [&](int c, int i) { return s.cols[c].value(i); };
    for (int i = 0; i < n; ++i) {
        const double x = s.cols[0][i];
        const double y = s.cols[1].value(i) + (stackBase ? stackBase->value(i) : 0.0);
        if (!finite2(x, y) || !geo.sx.valid(x) || !geo.sy.valid(y)) continue;
        const QPointF c = geo.map(x, y) + QPointF(xoff, 0);
        double dyp = 0, dym = 0, dxp = 0, dxm = 0;
        bool hasY = false, hasX = false;
        switch (s.type) {
        case SetType::XYDX: dxp = dxm = col(2, i); hasX = true; break;
        case SetType::XYDY:
        case SetType::BARDY: dyp = dym = col(2, i); hasY = true; break;
        case SetType::XYDXDX: dxp = col(2, i); dxm = col(3, i); hasX = true; break;
        case SetType::XYDYDY:
        case SetType::BARDYDY: dyp = col(2, i); dym = col(3, i); hasY = true; break;
        case SetType::XYDXDY: dxp = dxm = col(2, i); dyp = dym = col(3, i); hasX = hasY = true; break;
        case SetType::XYDXDXDYDY: dxp = col(2, i); dxm = col(3, i); dyp = col(4, i); dym = col(5, i); hasX = hasY = true; break;
        default: break;
        }
        auto bar = [&](double wx, double wy, bool vertical) {
            if (!geo.sx.valid(wx) || !geo.sy.valid(wy)) return;
            QPointF end = geo.map(wx, wy) + QPointF(xoff, 0);
            bool clipped = false;
            if (e.riserClip) {
                const double maxLen = e.riserClipLength * m_unit;
                const QPointF d = end - c;
                const double L = std::hypot(d.x(), d.y());
                if (L > maxLen && L > 0) { end = c + d * (maxLen / L); clipped = true; }
            }
            p->setPen(riser);
            p->drawLine(c, end);
            p->setPen(capPen);
            if (clipped) {
                // clipped error bars end in an arrow head (Grace)
                const QPointF u = unitVec(c, end), nrm(-u.y(), u.x());
                const double ah = std::max(cap, 0.006 * m_unit);
                p->drawLine(end, end - u * ah + nrm * ah * 0.5);
                p->drawLine(end, end - u * ah - nrm * ah * 0.5);
            } else if (vertical) {
                p->drawLine(end - QPointF(cap, 0), end + QPointF(cap, 0));
            } else {
                p->drawLine(end - QPointF(0, cap), end + QPointF(0, cap));
            }
        };
        if (hasY) {
            if (plus && dyp != 0) bar(x, y + std::fabs(dyp), true);
            if (minus && dym != 0) bar(x, y - std::fabs(dym), true);
        }
        if (hasX) {
            if (plus && dxp != 0) bar(x + std::fabs(dxp), y, false);
            if (minus && dxm != 0) bar(x - std::fabs(dxm), y, false);
        }
    }
}

void PlotRenderer::drawAnnotatedValues(QPainter* p, const DataSet& s, const GraphGeometry& geo, const QVector<QPointF>& pts,
                                       const QVector<double>* stackBase)
{
    const AnnotatedValues& av = s.avalue;
    if (av.type == 0) return;
    ticks::FormatOptions fo;
    fo.format = av.format;
    fo.precision = av.precision;
    fo.referenceDate = m_proj.referenceDate;
    fo.pretty = m_proj.prettyNumbers;
    fo.twoDigitYears = m_proj.dateWrap || !m_proj.prettyNumbers;
    const double size = ts(av.text.size);
    const int skip = std::max(0, s.symbol.skip) + 1;
    for (int i = 0; i < s.size(); i += skip) {
        const double x = s.cols[0][i];
        double y = s.cols[1].value(i);
        if (stackBase) y += stackBase->value(i);
        if (i >= pts.size() || !validPt(pts[i])) continue;
        QString txt;
        switch (av.type) {
        case 1: txt = ticks::formatValue(x, fo); break;
        case 2: txt = ticks::formatValue(y, fo); break;
        case 3: txt = QStringLiteral("(") + ticks::formatValue(x, fo) + QStringLiteral(", ") + ticks::formatValue(y, fo) + QStringLiteral(")"); break;
        case 4: txt = s.strings.value(i); break;
        case 5: txt = ticks::formatValue(s.cols[2].value(i), fo); break;
        default: break;
        }
        if (txt.isEmpty()) continue;
        txt = av.prepend + txt + av.append;
        const QPointF anchor = pts[i] + QPointF(av.offset.x() * m_unit, -av.offset.y() * m_unit);
        richtext::drawText(p, txt, av.text, size, &m_proj, anchor, av.angle, 2, 0);
    }
}

void PlotRenderer::drawPie(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo)
{
    Q_UNUSED(gi);
    const DataSet* s = nullptr;
    for (const DataSet& d : g.sets) {
        if (d.active && !d.hidden && d.size() > 0) { s = &d; break; }
    }
    if (!s) return;
    double total = 0.0;
    for (double v : s->cols[0]) if (v > 0) total += v;
    if (total <= 0) return;
    const QPointF c = geo.center;
    double maxExplode = 0.0;
    for (int i = 0; i < s->size(); ++i) maxExplode = std::max(maxExplode, s->cols[1].value(i));
    const double R = 0.8 * geo.radius / (1.0 + std::max(0.0, maxExplode));
    double start = 0.0;  // degrees, counter-clockwise from +x
    const QVector<QColor> cycle = presets::presetPalette(m_proj.stylePreset);
    for (int i = 0; i < s->size(); ++i) {
        const double v = s->cols[0][i];
        if (!(v > 0)) continue;
        const double span = 360.0 * v / total;
        const double explode = (s->type != SetType::XY || true) ? s->cols[1].value(i) : 0.0;
        const double mid = qDegreesToRadians(start + span / 2);
        const QPointF off(std::cos(mid) * explode * R, -std::sin(mid) * explode * R);
        const QRectF rect(c.x() - R + off.x(), c.y() - R + off.y(), 2 * R, 2 * R);
        QColor fill = cycle.value(i % std::max(1, int(cycle.size())));
        int pattern = 1;
        if (s->type == SetType::XY && s->fill.visible() && (s->fillType != FillType::None || !m_proj.prettyNumbers)) {
            fill = s->fill.color;
            pattern = s->fill.pattern;
            if (s->symbol.fill.pattern > 1) { fill = s->symbol.fill.color; pattern = s->symbol.fill.pattern; }
        }
        if (s->type == SetType::XYCOLOR || s->type == SetType::XYCOLPAT) fill = m_proj.colorAt(int(s->cols[2].value(i)));
        if (s->type == SetType::XYCOLPAT) pattern = int(s->cols[3].value(i));
        QPainterPath slice;
        slice.moveTo(rect.center());
        slice.arcTo(rect, start, span);
        slice.closeSubpath();
        p->setPen(makePen(s->line.visible() ? s->line : s->symbol.line, m_unit));
        p->setBrush(makeBrush(FillStyle{fill, pattern}));
        p->drawPath(slice);
        if (s->avalue.active) {
            QString txt;
            ticks::FormatOptions fo;
            fo.format = s->avalue.format;
            fo.precision = s->avalue.precision;
            fo.pretty = m_proj.prettyNumbers;
            switch (s->avalue.type) {
            case 1: txt = ticks::formatValue(v, fo); break;
            case 2: txt = ticks::formatValue(100.0 * v / total, fo) + QLatin1Char('%'); break;
            case 4: txt = s->strings.value(i); break;
            default: txt = ticks::formatValue(v, fo); break;
            }
            txt = s->avalue.prepend + txt + s->avalue.append;
            const double rr = R * (1.0 + explode) + s->avalue.offset.y() * m_unit;
            const QPointF anchor = c + QPointF(std::cos(mid) * rr, -std::sin(mid) * rr);
            const int hj = std::fabs(std::cos(mid)) < 0.3 ? 2 : (std::cos(mid) > 0 ? 0 : 1);
            richtext::drawText(p, txt, s->avalue.text, ts(s->avalue.text.size), &m_proj, anchor, 0,
                               s->avalue.offset.y() < 0 ? 2 : hj, 2);
        }
        start += span;
    }
}

void PlotRenderer::drawTitles(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo)
{
    const QRectF v = geo.view;
    // find the top-most element of the opposite x axis (tick labels / labels) to avoid overlap
    double top = v.top();
    if (g.type == GraphType::Polar) top = geo.center.y() - geo.radius - 0.03 * m_unit;
    for (const HitItem& h : m_hits) {
        if (h.graph == gi && (h.kind == HitItem::AxisTicks || h.kind == HitItem::AxisLabel)) {
            const QRectF r = h.shape.boundingRect();
            if (r.bottom() <= v.top() + 1.0) top = std::min(top, r.top());
        }
    }
    const double gap = 0.02 * m_unit;
    double y = top - gap;
    if (!g.subtitle.text.isEmpty()) {
        const QRectF r = richtext::drawText(p, g.subtitle.text, g.subtitle.style, ts(g.subtitle.style.size), &m_proj,
                                            QPointF(v.center().x(), y), 0, 2, 1);
        if (m_opt.collectHits) {
            QPainterPath sh;
            sh.addRect(r);
            m_hits.push_back({HitItem::Subtitle, gi, -1, sh});
        }
        y = r.top() - 0.008 * m_unit;
    }
    if (!g.title.text.isEmpty()) {
        const QRectF r = richtext::drawText(p, g.title.text, g.title.style, ts(g.title.style.size), &m_proj,
                                            QPointF(v.center().x(), y), 0, 2, 1);
        if (m_opt.collectHits) {
            QPainterPath sh;
            sh.addRect(r);
            m_hits.push_back({HitItem::Title, gi, -1, sh});
        }
    }
}

void PlotRenderer::drawLegend(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo)
{
    const Legend& L = g.legend;
    if (!L.active) return;
    QVector<int> entries;
    for (int i = 0; i < g.sets.size(); ++i) {
        const DataSet& s = g.sets[i];
        if (s.active && !s.hidden && !s.legend.isEmpty()) entries.push_back(i);
    }
    if (entries.isEmpty()) return;
    if (L.invert) std::reverse(entries.begin(), entries.end());

    const double size = ts(L.text.size);
    const double sampleW = std::max(0, L.length) * 0.012 * m_unit * std::max(0.5, L.text.size);
    const double hgap = (0.3 + L.hgap * 0.6) * 0.012 * m_unit;
    const double vgap = L.vgap * 0.25 * size;
    const double pad = 0.012 * m_unit;

    QVector<TextLayout> lays;
    double textW = 0.0, rowsH = 0.0;
    QVector<double> rowH;
    for (int idx : entries) {
        const TextLayout l = richtext::layout(g.sets[idx].legend, L.text.font, size, L.text.color, &m_proj, p->device());
        lays.push_back(l);
        textW = std::max(textW, l.width);
        const double symH = g.sets[idx].symbol.type != Symbol::None ? 0.02 * m_unit * g.sets[idx].symbol.size : 0.0;
        const double textH = l.bounds.height() > 1.5 * size ? l.bounds.height() : size;
        const double h = std::max(textH, symH);
        rowH.push_back(h);
        rowsH += h;
    }
    rowsH += vgap * (entries.size() - 1);
    const double sampleSpace = sampleW > 0 ? sampleW + hgap : 0.0;
    const QSizeF boxSize(pad * 2 + sampleSpace + textW, pad * 2 + rowsH);

    QPointF tl;
    if (L.autoPlace && g.type != GraphType::Pie) {
        // choose the position covering the fewest data points: the corners first, then the
        // middles of the edges (ties keep the earlier candidate)
        const QRectF v = geo.view;
        const double inset = 0.015 * m_unit;
        const double right = v.right() - inset - boxSize.width(), left = v.left() + inset;
        const double top = v.top() + inset, bottom = v.bottom() - inset - boxSize.height();
        const double midX = v.center().x() - boxSize.width() / 2, midY = v.center().y() - boxSize.height() / 2;
        const QVector<QPointF> cands = {QPointF(right, top),    QPointF(left, top),  QPointF(right, bottom),
                                        QPointF(left, bottom),  QPointF(right, midY), QPointF(left, midY),
                                        QPointF(midX, top),     QPointF(midX, bottom)};
        int best = 0;
        long bestCount = -1;
        for (int c = 0; c < cands.size(); ++c) {
            const QRectF r(cands[c], boxSize);
            long count = 0;
            for (const DataSet& s : g.sets) {
                if (!s.active || s.hidden) continue;
                const int step = std::max(1, s.size() / 400);
                for (int i = 0; i < s.size(); i += step) {
                    const double x = s.cols[0][i], y = s.cols[1].value(i);
                    if (!geo.sx.valid(x) || !geo.sy.valid(y)) continue;
                    if (r.contains(geo.map(x, y))) ++count;
                }
                // line segments crossing the box also count
                if (s.lineType != LineType::None) {
                    for (int i = step; i < s.size(); i += step) {
                        const QPointF a = geo.map(s.cols[0][i - step], s.cols[1].value(i - step));
                        const QPointF b = geo.map(s.cols[0][i], s.cols[1].value(i));
                        const QPointF m = (a + b) / 2;
                        if (r.contains(m)) ++count;
                    }
                }
            }
            if (bestCount < 0 || count < bestCount) {
                bestCount = count;
                best = c;
            }
        }
        tl = cands[best];
    } else if (L.loctype == LocType::World) {
        tl = geo.map(L.pos.x(), L.pos.y());
    } else {
        tl = geo.viewToPage(L.pos.x(), L.pos.y());
    }
    const QRectF box(tl, boxSize);
    if (gi >= 0 && gi < m_legendRects.size()) m_legendRects[gi] = box;

    p->save();
    const double radius = L.cornerRadius * m_unit;
    p->setPen(makePen(L.box, m_unit));
    p->setBrush(makeBrush(L.boxFill, box));
    if (radius > 0) p->drawRoundedRect(box, radius, radius);
    else p->drawRect(box);

    double y = box.top() + pad;
    for (int k = 0; k < entries.size(); ++k) {
        const DataSet& s = g.sets[entries[k]];
        const double h = rowH[k];
        const double cy = y + h / 2;
        const double sx0 = box.left() + pad;
        if (sampleW > 0) {
            const bool barType = s.type == SetType::BAR || s.type == SetType::BARDY || s.type == SetType::BARDYDY ||
                                 s.type == SetType::XYBOXPLOT;
            if (barType) {
                const double bh = std::min(h, 0.022 * m_unit);
                const QRectF r(sx0 + sampleW * 0.25, cy - bh / 2, sampleW * 0.5, bh);
                p->setPen(makePen(s.symbol.line, m_unit));
                p->setBrush(makeBrush(s.symbol.fill, r));
                p->drawRect(r);
            } else {
                if (s.fillType != FillType::None && s.fill.visible()) {
                    const double bh = std::min(h, 0.022 * m_unit);
                    const QRectF r(sx0, cy - bh / 2, sampleW, bh);
                    p->setPen(Qt::NoPen);
                    p->setBrush(makeBrush(s.fill, r));
                    p->drawRect(r);
                }
                if (s.lineType != LineType::None && s.line.visible()) {
                    p->setPen(makePen(s.line, m_unit));
                    p->drawLine(QPointF(sx0, cy), QPointF(sx0 + sampleW, cy));
                }
                if (s.symbol.type != Symbol::None) {
                    const bool line = s.lineType != LineType::None && s.line.visible();
                    if (line && !m_proj.prettyNumbers) {
                        // Grace draws the symbol at both ends of the sample line
                        drawSymbol(p, s, QPointF(sx0, cy), 1.0, nullptr);
                        drawSymbol(p, s, QPointF(sx0 + sampleW, cy), 1.0, nullptr);
                    } else {
                        drawSymbol(p, s, QPointF(sx0 + sampleW * 0.5, cy), 1.0, nullptr);
                    }
                }
            }
        }
        const TextLayout& l = lays[k];
        richtext::draw(p, l, QPointF(sx0 + sampleSpace, cy), 0, 0, 2);
        y += h + vgap;
    }
    p->restore();
    if (m_opt.collectHits) {
        QPainterPath sh;
        sh.addRect(box);
        m_hits.push_back({HitItem::Legend, gi, -1, sh});
    }
}

void PlotRenderer::drawColorBars(QPainter* p, const Graph& g, const GraphGeometry& geo)
{
    int k = 0;
    for (const DataSet& s : g.sets) {
        if (!s.active || s.hidden || !s.showColorBar || s.type != SetType::XYCOLOR || !s.colorFromMap) continue;
        const Colormap* cm = colors::colormap(s.colormap);
        if (!cm) continue;
        double lo = s.cmapMin, hi = s.cmapMax;
        if (s.cmapAuto && s.size() > 0) {
            lo = HUGE_VAL; hi = -HUGE_VAL;
            for (double c : s.cols[2]) if (std::isfinite(c)) { lo = std::min(lo, c); hi = std::max(hi, c); }
        }
        if (!(hi > lo)) continue;
        const double w = 0.025 * m_unit;
        const QRectF bar(geo.view.right() + 0.03 * m_unit + k * 0.12 * m_unit, geo.view.top(), w, geo.view.height());
        const int steps = 128;
        for (int i = 0; i < steps; ++i) {
            const double t0 = double(i) / steps;
            QRectF seg(bar.left(), bar.bottom() - (i + 1) * bar.height() / steps, w, bar.height() / steps + 0.3);
            p->fillRect(seg, cm->at(t0, 0, 1, s.cmapReversed));
        }
        p->setPen(QPen(QColor(60, 60, 60), lw(0.5)));
        p->setBrush(Qt::NoBrush);
        p->drawRect(bar);
        Axis ax;
        ax.autoTickCount = 6;
        const Scale sc(ScaleType::Linear, lo, hi);
        ax.major = ticks::autoSpacing(lo, hi, ScaleType::Linear, 6);
        ax.minorTicks = 0;
        const QVector<Tick> tks = ticks::compute(ax, sc, &m_proj);
        const TextStyle tst = g.axes[1].ticklabelText;
        for (const Tick& t : tks) {
            if (!t.major) continue;
            const double y = bar.bottom() - sc.toFrac(t.value) * bar.height();
            p->drawLine(QPointF(bar.right(), y), QPointF(bar.right() + 0.008 * m_unit, y));
            richtext::drawText(p, t.label, tst, ts(tst.size) * 0.85, &m_proj, QPointF(bar.right() + 0.012 * m_unit, y), 0, 0, 2);
        }
        ++k;
    }
}

void PlotRenderer::drawArrowHead(QPainter* p, const QPointF& tip, const QPointF& from, const DrawObject& o, const QPen& pen)
{
    const QPointF u = unitVec(from, tip);
    const QPointF nrm(-u.y(), u.x());
    double L = o.arrowLength * 0.01 * m_unit;
    QPointF dir = u;
    if (L < 0) {
        L = -L;
        dir = -u;
    }
    const double halfW = 0.5 * o.arrowDL * L;
    const QPointF base = tip - dir * L;
    const QPointF notch = tip - dir * L * (1.0 - o.arrowLL);
    QPolygonF head{tip, base + nrm * halfW, notch, base - nrm * halfW};
    p->save();
    QPen hp = pen;
    hp.setStyle(Qt::SolidLine);
    hp.setJoinStyle(Qt::MiterJoin);
    p->setPen(hp);
    if (o.arrowType == 0) {
        p->setBrush(Qt::NoBrush);
        p->drawPolyline(QPolygonF{base + nrm * halfW, tip, base - nrm * halfW});
    } else {
        p->setBrush(o.arrowType == 1 ? QBrush(pen.color()) : QBrush(m_proj.page.background.color));
        p->drawPolygon(head);
    }
    p->restore();
}

void PlotRenderer::drawObject(QPainter* p, int oi, const DrawObject& o)
{
    auto toPage = [&](const QPointF& q) {
        if (o.loctype == LocType::World) {
            if (const Graph* g = m_proj.graph(o.graph)) {
                const GraphGeometry geo(m_proj.page, *g);
                return geo.map(q.x(), q.y());
            }
        }
        return QPointF(q.x() * m_unit, m_proj.page.height - q.y() * m_unit);
    };
    QPainterPath shape;
    p->save();
    switch (o.kind) {
    case DrawObjectKind::Text: {
        const QPointF a = toPage(o.pos);
        const QRectF r = richtext::drawText(p, o.text, o.textStyle, ts(o.textStyle.size), &m_proj, a, o.angle,
                                            hjustOf(o.just), vjustOf(o.just));
        shape.addRect(r.adjusted(-2, -2, 2, 2));
        break;
    }
    case DrawObjectKind::Line: {
        const QPointF a = toPage(o.p1), b = toPage(o.p2);
        QPen pen = makePen(o.line, m_unit);
        p->setPen(pen);
        // shorten the shaft under filled arrow heads so wide lines do not poke through
        QPointF a2 = a, b2 = b;
        const double L = std::fabs(o.arrowLength) * 0.01 * m_unit;
        if ((o.arrow & 2) && o.arrowType != 0) b2 = b - unitVec(a, b) * std::min(L * 0.5, QLineF(a, b).length() * 0.5);
        if ((o.arrow & 1) && o.arrowType != 0) a2 = a + unitVec(a, b) * std::min(L * 0.5, QLineF(a, b).length() * 0.5);
        p->drawLine(a2, b2);
        if (o.arrow & 1) drawArrowHead(p, a, b, o, pen);
        if (o.arrow & 2) drawArrowHead(p, b, a, o, pen);
        QPainterPath pp;
        pp.moveTo(a);
        pp.lineTo(b);
        QPainterPathStroker st;
        st.setWidth(8);
        shape = st.createStroke(pp);
        break;
    }
    case DrawObjectKind::Box:
    case DrawObjectKind::Ellipse: {
        const QRectF r = QRectF(toPage(o.p1), toPage(o.p2)).normalized();
        p->translate(r.center());
        if (o.rotation != 0) p->rotate(-o.rotation);
        const QRectF rr(-r.width() / 2, -r.height() / 2, r.width(), r.height());
        p->setPen(makePen(o.line, m_unit));
        p->setBrush(makeBrush(o.fill, rr));
        if (o.kind == DrawObjectKind::Box) p->drawRect(rr);
        else p->drawEllipse(rr);
        QTransform t;
        t.translate(r.center().x(), r.center().y());
        if (o.rotation != 0) t.rotate(-o.rotation);
        QPainterPath pp;
        if (o.kind == DrawObjectKind::Box) pp.addRect(rr.adjusted(-3, -3, 3, 3));
        else pp.addEllipse(rr.adjusted(-3, -3, 3, 3));
        shape = t.map(pp);
        break;
    }
    }
    p->restore();
    if (m_opt.collectHits) m_hits.push_back({HitItem::Object, o.graph, oi, shape});
}

void PlotRenderer::drawRegions(QPainter* p)
{
    for (const Region& r : m_proj.regions) {
        if (!r.active) continue;
        const Graph* g = m_proj.graph(r.linkedGraph);
        if (!g) continue;
        const GraphGeometry geo(m_proj.page, *g);
        p->save();
        QPen pen = makePen(r.line, m_unit);
        pen.setStyle(Qt::DashLine);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->setClipRect(geo.view);
        if (r.type == RegionType::Polygon) {
            QPolygonF poly;
            for (const QPointF& q : r.polygon) poly << geo.map(q.x(), q.y());
            p->drawPolygon(poly);
        } else if (r.type == RegionType::Horizontal) {
            const double x1 = geo.map(r.p1.x(), g->wymin).x(), x2 = geo.map(r.p2.x(), g->wymin).x();
            p->drawLine(QPointF(x1, geo.view.top()), QPointF(x1, geo.view.bottom()));
            p->drawLine(QPointF(x2, geo.view.top()), QPointF(x2, geo.view.bottom()));
        } else if (r.type == RegionType::Vertical) {
            const double y1 = geo.map(g->wxmin, r.p1.y()).y(), y2 = geo.map(g->wxmin, r.p2.y()).y();
            p->drawLine(QPointF(geo.view.left(), y1), QPointF(geo.view.right(), y1));
            p->drawLine(QPointF(geo.view.left(), y2), QPointF(geo.view.right(), y2));
        } else {
            // half-planes: draw the (extended) defining line
            const QPointF a = geo.map(r.p1.x(), r.p1.y()), b = geo.map(r.p2.x(), r.p2.y());
            const QPointF d = b - a;
            p->drawLine(a - d * 100, b + d * 100);
        }
        p->restore();
    }
}

QImage renderToImage(const Project& project, int pixelWidth, bool transparent, double dpi)
{
    const double w = project.page.width, h = project.page.height;
    const int pw = std::max(1, pixelWidth);
    const int ph = std::max(1, int(std::lround(pw * h / w)));
    QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
    if (dpi <= 0) dpi = 72.0 * pw / w;
    img.setDotsPerMeterX(int(std::lround(dpi / 0.0254)));
    img.setDotsPerMeterY(int(std::lround(dpi / 0.0254)));
    img.fill(transparent ? Qt::transparent : Qt::white);
    QPainter p(&img);
    RenderOptions o;
    o.drawPageBackground = !transparent;
    PlotRenderer r(project, o);
    r.render(&p, QRectF(0, 0, pw, ph));
    p.end();
    return img;
}

}  // namespace cactus
