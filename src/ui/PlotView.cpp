// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/PlotView.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>
#include <cmath>

#include "core/Presets.h"
#include "plot/Ticks.h"
#include "ui/Document.h"
#include "ui/Theme.h"

namespace cactus::ui {

PlotView::PlotView(Document* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);
    connect(doc, &Document::changed, this, &PlotView::invalidate);
    connect(doc, &Document::selectionChanged, this, qOverload<>(&QWidget::update));
}

void PlotView::setTool(Tool t)
{
    if (m_tool == t) return;
    m_tool = t;
    m_regionPts.clear();
    switch (t) {
    case Tool::Pan: setCursor(Qt::OpenHandCursor); break;
    case Tool::ZoomBox:
    case Tool::ZoomX:
    case Tool::ZoomY:
    case Tool::Crosshair:
    case Tool::Line:
    case Tool::Arrow:
    case Tool::Box:
    case Tool::Ellipse:
    case Tool::Region: setCursor(Qt::CrossCursor); break;
    case Tool::Text: setCursor(Qt::IBeamCursor); break;
    case Tool::EditPoints: setCursor(Qt::PointingHandCursor); break;
    default: unsetCursor(); break;
    }
    emit toolChanged(t);
    update();
}

void PlotView::setZoom(double z)
{
    z = std::clamp(z, 0.1, 40.0);
    if (std::fabs(z - m_zoom) < 1e-9) return;
    m_zoom = z;
    if (z <= 1.0) m_panOffset = QPointF();
    invalidate();
    emit zoomChanged(z);
}

void PlotView::fitPage()
{
    m_panOffset = QPointF();
    m_zoom = 1.0;
    invalidate();
    emit zoomChanged(1.0);
}

void PlotView::setShowRegions(bool on)
{
    m_showRegions = on;
    invalidate();
}

QRectF PlotView::pageRectWidget() const
{
    const Page& pg = m_doc->project().page;
    const double margin = 28;
    const double fit = std::max(0.01, std::min((width() - 2 * margin) / pg.width, (height() - 2 * margin) / pg.height));
    const double s = fit * m_zoom;
    const QSizeF sz(pg.width * s, pg.height * s);
    return QRectF(QPointF((width() - sz.width()) / 2 + m_panOffset.x(), (height() - sz.height()) / 2 + m_panOffset.y()), sz);
}

QTransform PlotView::pageToWidget() const
{
    const QRectF r = pageRectWidget();
    const Page& pg = m_doc->project().page;
    QTransform t;
    t.translate(r.left(), r.top());
    t.scale(r.width() / pg.width, r.height() / pg.height);
    return t;
}

QPointF PlotView::toPage(const QPointF& w) const { return pageToWidget().inverted().map(w); }

QPointF PlotView::toView(const QPointF& pg) const
{
    const Page& page = m_doc->project().page;
    return QPointF(pg.x() / page.unit(), (page.height - pg.y()) / page.unit());
}

void PlotView::invalidate()
{
    m_dirty = true;
    update();
}

void PlotView::ensureRendered()
{
    const QRectF r = pageRectWidget();
    const qreal dpr = devicePixelRatioF();
    const QSize px(std::max(1, int(std::ceil(r.width() * dpr))), std::max(1, int(std::ceil(r.height() * dpr))));
    if (!m_dirty && m_cache.size() == px) return;
    m_cache = QImage(px, QImage::Format_ARGB32_Premultiplied);
    m_cache.setDevicePixelRatio(dpr);
    m_cache.fill(Qt::white);
    QPainter p(&m_cache);
    RenderOptions o;
    o.collectHits = true;
    o.showRegions = m_showRegions;
    const Project& pr = m_doc->project();
    o.highlightGraph = pr.activeGraphCount() > 1 ? m_doc->currentGraph() : -1;
    PlotRenderer renderer(pr, o);
    renderer.render(&p, QRectF(0, 0, r.width(), r.height()));
    p.end();
    m_hits = renderer.hitItems();
    m_legendRects.clear();
    for (int g = 0; g < pr.graphs.size(); ++g) m_legendRects.push_back(renderer.legendRect(g));
    m_dirty = false;
}

void PlotView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), theme::canvasBackground());
    ensureRendered();
    const QRectF r = pageRectWidget();
    // soft drop shadow
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 6; i > 0; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 10));
        p.drawRoundedRect(r.adjusted(-i, -i + 2, i, i + 2), i, i);
    }
    p.drawImage(r.topLeft(), m_cache);
    drawOverlay(p);
}

void PlotView::drawOverlay(QPainter& p)
{
    const QTransform t = pageToWidget();
    const Selection& sel = m_doc->selection();
    const QColor acc = theme::accent();
    p.setRenderHint(QPainter::Antialiasing);
    // selection outline
    for (const HitItem& h : m_hits) {
        bool match = false;
        switch (sel.kind) {
        case Selection::Set: match = h.kind == HitItem::SetCurve && h.graph == sel.graph && h.index == sel.index; break;
        case Selection::Legend: match = h.kind == HitItem::Legend && h.graph == sel.graph; break;
        case Selection::Title: match = (h.kind == HitItem::Title || h.kind == HitItem::Subtitle) && h.graph == sel.graph; break;
        case Selection::Axis: match = (h.kind == HitItem::AxisLabel || h.kind == HitItem::AxisTicks) && h.graph == sel.graph && h.index == sel.index; break;
        case Selection::Object: match = h.kind == HitItem::Object && h.index == sel.index; break;
        case Selection::Graph: match = h.kind == HitItem::GraphArea && h.graph == sel.graph; break;
        default: break;
        }
        if (!match) continue;
        if (sel.kind == Selection::Graph) {
            // subtle corner brackets for the selected graph
            const QRectF r = t.map(h.shape).boundingRect().adjusted(-3, -3, 3, 3);
            const double L = std::min(14.0, std::min(r.width(), r.height()) / 4);
            p.setPen(QPen(acc, 2.0, Qt::SolidLine, Qt::SquareCap));
            p.setBrush(Qt::NoBrush);
            for (int cx = 0; cx < 2; ++cx)
                for (int cy = 0; cy < 2; ++cy) {
                    const QPointF c(cx ? r.right() : r.left(), cy ? r.bottom() : r.top());
                    const double sx = cx ? -1 : 1, sy = cy ? -1 : 1;
                    p.drawLine(c, c + QPointF(sx * L, 0));
                    p.drawLine(c, c + QPointF(0, sy * L));
                }
            continue;
        }
        QPen pen(acc, 1.5, Qt::SolidLine);
        p.setPen(pen);
        QColor fill = acc;
        fill.setAlpha(sel.kind == Selection::Set ? 40 : 18);
        p.setBrush(sel.kind == Selection::Graph ? QBrush(Qt::NoBrush) : QBrush(fill));
        p.drawPath(t.map(h.shape));
    }
    // rubber band / shape preview
    if (m_drag == Drag::Rubber || m_drag == Drag::Shape) {
        QPointF a = t.map(m_pressPage), b = t.map(m_curPage);
        if (m_drag == Drag::Rubber && m_dragGraph >= 0) {
            const Graph* g = m_doc->project().graph(m_dragGraph);
            if (g) {
                const GraphGeometry geo(m_doc->project().page, *g);
                const QRectF v = t.mapRect(geo.view);
                if (m_tool == Tool::ZoomX) { a.setY(v.top()); b.setY(v.bottom()); }
                if (m_tool == Tool::ZoomY) { a.setX(v.left()); b.setX(v.right()); }
            }
        }
        QColor fill = acc;
        fill.setAlpha(30);
        p.setPen(QPen(acc, 1.2, Qt::DashLine));
        p.setBrush(fill);
        if (m_tool == Tool::Line || m_tool == Tool::Arrow) p.drawLine(a, b);
        else if (m_tool == Tool::Ellipse) p.drawEllipse(QRectF(a, b).normalized());
        else p.drawRect(QRectF(a, b).normalized());
    }
    // region being drawn
    if (!m_regionPts.isEmpty()) {
        QPolygonF poly;
        for (const QPointF& q : m_regionPts) poly << t.map(q);
        poly << t.map(m_curPage);
        p.setPen(QPen(acc, 1.5, Qt::DashLine));
        QColor fill = acc;
        fill.setAlpha(25);
        p.setBrush(fill);
        p.drawPolygon(poly);
    }
    // crosshair / point hover
    if (m_hoverValid && (m_tool == Tool::Crosshair || m_tool == Tool::EditPoints)) {
        const int g = graphAt(m_hover);
        const Graph* gr = m_doc->project().graph(g);
        if (gr) {
            const GraphGeometry geo(m_doc->project().page, *gr);
            const QRectF v = t.mapRect(geo.view);
            const QPointF c = t.map(m_hover);
            if (m_tool == Tool::Crosshair && v.contains(c)) {
                p.setPen(QPen(QColor(acc.red(), acc.green(), acc.blue(), 160), 1, Qt::DashLine));
                p.drawLine(QPointF(v.left(), c.y()), QPointF(v.right(), c.y()));
                p.drawLine(QPointF(c.x(), v.top()), QPointF(c.x(), v.bottom()));
            }
        }
        int ng, ns, ni;
        if (nearestPoint(m_hover, &ng, &ns, &ni, 10.0)) {
            const Graph* g2 = m_doc->project().graph(ng);
            const GraphGeometry geo(m_doc->project().page, *g2);
            const DataSet& s = g2->sets[ns];
            const QPointF c = t.map(geo.map(s.cols[0][ni], s.cols[1][ni]));
            p.setPen(QPen(acc, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 6, 6);
            const QString txt = QStringLiteral("S%1[%2]  (%3, %4)").arg(ns).arg(ni).arg(s.cols[0][ni], 0, 'g', 7).arg(s.cols[1][ni], 0, 'g', 7);
            QFont f = font();
            p.setFont(f);
            const QRectF br = QRectF(p.fontMetrics().boundingRect(txt)).adjusted(-6, -3, 6, 3).translated(c + QPointF(10, -10));
            p.setPen(Qt::NoPen);
            p.setBrush(palette().color(QPalette::ToolTipBase));
            p.drawRoundedRect(br, 4, 4);
            p.setPen(palette().color(QPalette::ToolTipText));
            p.drawText(br, Qt::AlignCenter, txt);
        }
    }
}

int PlotView::graphAt(const QPointF& pt) const
{
    const Project& pr = m_doc->project();
    for (int g = pr.graphs.size() - 1; g >= 0; --g) {
        const Graph& gr = pr.graphs[g];
        if (!gr.active || gr.hidden) continue;
        const GraphGeometry geo(pr.page, gr);
        if (geo.view.contains(pt)) return g;
    }
    return -1;
}

bool PlotView::worldAt(int graph, const QPointF& pt, double* wx, double* wy) const
{
    const Graph* g = m_doc->project().graph(graph);
    if (!g) return false;
    const GraphGeometry geo(m_doc->project().page, *g);
    return geo.inverse(pt, wx, wy);
}

const HitItem* PlotView::hitTest(const QPointF& pt) const
{
    // objects and decorations first (topmost = last drawn), then curves, then graph areas
    static const HitItem::Kind order[] = {HitItem::Object, HitItem::Legend, HitItem::Title, HitItem::Subtitle,
                                          HitItem::AxisLabel, HitItem::AxisTicks, HitItem::SetCurve, HitItem::Timestamp};
    for (HitItem::Kind k : order) {
        for (int i = m_hits.size() - 1; i >= 0; --i)
            if (m_hits[i].kind == k && m_hits[i].shape.contains(pt)) return &m_hits[i];
    }
    for (int i = m_hits.size() - 1; i >= 0; --i)
        if (m_hits[i].kind == HitItem::GraphArea && m_hits[i].shape.contains(pt)) return &m_hits[i];
    return nullptr;
}

void PlotView::selectAt(const QPointF& pt)
{
    const HitItem* h = hitTest(pt);
    if (!h) {
        m_doc->select(Selection{Selection::Page, -1, -1});
        return;
    }
    switch (h->kind) {
    case HitItem::Object: m_doc->select(Selection{Selection::Object, h->graph, h->index}); break;
    case HitItem::Legend: m_doc->select(Selection{Selection::Legend, h->graph, -1}); break;
    case HitItem::Title:
    case HitItem::Subtitle: m_doc->select(Selection{Selection::Title, h->graph, -1}); break;
    case HitItem::AxisLabel:
    case HitItem::AxisTicks: m_doc->select(Selection{Selection::Axis, h->graph, h->index}); break;
    case HitItem::SetCurve: m_doc->select(Selection{Selection::Set, h->graph, h->index}); break;
    case HitItem::GraphArea: m_doc->select(Selection{Selection::Graph, h->graph, -1}); break;
    case HitItem::Timestamp: m_doc->select(Selection{Selection::Page, -1, -1}); break;
    }
    if (h->graph >= 0) m_doc->setCurrentGraph(h->graph);
}

void PlotView::zoomWorld(int graph, const QPointF& pt, double fx, double fy, const QString& key)
{
    const Graph* g = m_doc->project().graph(graph);
    if (!g || g->type == GraphType::Pie) return;
    const GraphGeometry geo(m_doc->project().page, *g);
    if (g->type == GraphType::Polar) {
        m_doc->modify(tr("Zoom"), [graph, fy](Project& p) {
            if (Graph* gg = p.graph(graph)) { gg->wymax *= fy; gg->autoticks(); }
        }, key);
        return;
    }
    const double cx = (pt.x() - geo.view.left()) / geo.view.width();
    const double cy = (geo.view.bottom() - pt.y()) / geo.view.height();
    const double x0 = geo.sx.fromFrac(cx * (1 - fx)), x1 = geo.sx.fromFrac(cx + (1 - cx) * fx);
    const double y0 = geo.sy.fromFrac(cy * (1 - fy)), y1 = geo.sy.fromFrac(cy + (1 - cy) * fy);
    m_doc->modify(tr("Zoom"), [=](Project& p) {
        Graph* gg = p.graph(graph);
        if (!gg) return;
        if (fx != 1.0 && std::isfinite(x0) && std::isfinite(x1)) { gg->wxmin = std::min(x0, x1); gg->wxmax = std::max(x0, x1); }
        if (fy != 1.0 && std::isfinite(y0) && std::isfinite(y1)) { gg->wymin = std::min(y0, y1); gg->wymax = std::max(y0, y1); }
        gg->autoticks();
    }, key);
}

void PlotView::panWorld(int graph, const QPointF& from, const QPointF& to, const QString& key)
{
    const Graph* g = m_doc->project().graph(graph);
    if (!g || g->type == GraphType::Pie || g->type == GraphType::Polar) return;
    const GraphGeometry geo(m_doc->project().page, *g);
    const double dfx = (to.x() - from.x()) / geo.view.width();
    const double dfy = (to.y() - from.y()) / geo.view.height();
    const double x0 = geo.sx.fromFrac(-dfx), x1 = geo.sx.fromFrac(1 - dfx);
    const double y0 = geo.sy.fromFrac(dfy), y1 = geo.sy.fromFrac(1 + dfy);
    if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(y0) || !std::isfinite(y1)) return;
    m_doc->modify(tr("Pan"), [=](Project& p) {
        Graph* gg = p.graph(graph);
        if (!gg) return;
        gg->wxmin = std::min(x0, x1);
        gg->wxmax = std::max(x0, x1);
        gg->wymin = std::min(y0, y1);
        gg->wymax = std::max(y0, y1);
    }, key);
}

void PlotView::finishRubberBand(const QPointF& a, const QPointF& b)
{
    const Graph* g = m_doc->project().graph(m_dragGraph);
    if (!g) return;
    const GraphGeometry geo(m_doc->project().page, *g);
    const QRectF r = QRectF(a, b).normalized();
    if (r.width() < 3 && r.height() < 3) {
        // a click zooms in around the point
        zoomWorld(m_dragGraph, a, m_tool == Tool::ZoomY ? 1.0 : 0.5, m_tool == Tool::ZoomX ? 1.0 : 0.5, QString());
        return;
    }
    const double fx0 = (r.left() - geo.view.left()) / geo.view.width(), fx1 = (r.right() - geo.view.left()) / geo.view.width();
    const double fy0 = (geo.view.bottom() - r.bottom()) / geo.view.height(), fy1 = (geo.view.bottom() - r.top()) / geo.view.height();
    const double x0 = geo.sx.fromFrac(fx0), x1 = geo.sx.fromFrac(fx1), y0 = geo.sy.fromFrac(fy0), y1 = geo.sy.fromFrac(fy1);
    const Tool tool = m_tool;
    const int gi = m_dragGraph;
    m_doc->modify(tr("Zoom"), [=](Project& p) {
        Graph* gg = p.graph(gi);
        if (!gg) return;
        if (tool != Tool::ZoomY) { gg->wxmin = std::min(x0, x1); gg->wxmax = std::max(x0, x1); }
        if (tool != Tool::ZoomX) { gg->wymin = std::min(y0, y1); gg->wymax = std::max(y0, y1); }
        gg->autoticks();
    });
}

void PlotView::createShape(const QPointF& a, const QPointF& b)
{
    DrawObject o;
    o.loctype = LocType::View;
    o.graph = std::max(0, m_doc->currentGraph());
    const QString preset = m_doc->project().stylePreset;
    const bool grace = preset == QLatin1String("Grace Classic");
    o.line = LineStyle{grace ? QColor(Qt::black) : QColor(40, 40, 45), grace ? 1.0 : 1.6, 1};
    o.textStyle = TextStyle{grace ? QStringLiteral("Times-Roman") : QStringLiteral("Helvetica"), 0.9,
                            m_doc->project().page.background.color.lightness() < 100 ? QColor(230, 230, 235) : QColor(30, 30, 35)};
    o.p1 = toView(a);
    o.p2 = toView(b);
    o.fill = FillStyle{QColor(255, 255, 255), 0};
    switch (m_tool) {
    case Tool::Text: {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(this, tr("Add text"),
                                                            tr("Text (supports $math$ and Grace escapes such as \\S, \\s, \\x):"),
                                                            QString(), &ok);
        if (!ok || text.isEmpty()) return;
        o.kind = DrawObjectKind::Text;
        o.text = text;
        o.pos = toView(a);
        break;
    }
    case Tool::Line: o.kind = DrawObjectKind::Line; break;
    case Tool::Arrow:
        o.kind = DrawObjectKind::Line;
        o.arrow = 2;
        o.arrowType = 1;
        o.arrowLength = 1.6;
        o.arrowDL = 0.8;
        o.arrowLL = 0.15;
        break;
    case Tool::Box: o.kind = DrawObjectKind::Box; break;
    case Tool::Ellipse: o.kind = DrawObjectKind::Ellipse; break;
    default: return;
    }
    if (o.kind != DrawObjectKind::Text && QLineF(a, b).length() < 2) return;
    int newIndex = 0;
    m_doc->modify(tr("Add annotation"), [&](Project& p) {
        p.objects.push_back(o);
        newIndex = p.objects.size() - 1;
    });
    m_doc->select(Selection{Selection::Object, o.graph, newIndex});
}

bool PlotView::nearestPoint(const QPointF& pt, int* graph, int* set, int* index, double maxDistWidget) const
{
    const Project& pr = m_doc->project();
    const QTransform t = pageToWidget();
    const QPointF w = t.map(pt);
    double best = maxDistWidget * maxDistWidget;
    bool found = false;
    const Selection& sel = m_doc->selection();
    for (int g = 0; g < pr.graphs.size(); ++g) {
        const Graph& gr = pr.graphs[g];
        if (!gr.active || gr.hidden) continue;
        const GraphGeometry geo(pr.page, gr);
        if (!geo.view.adjusted(-10, -10, 10, 10).contains(pt)) continue;
        for (int s = 0; s < gr.sets.size(); ++s) {
            const DataSet& d = gr.sets[s];
            if (!d.active || d.hidden) continue;
            // prefer the selected set when editing points
            const double bonus = (sel.kind == Selection::Set && sel.graph == g && sel.index == s) ? 0.5 : 1.0;
            for (int i = 0; i < d.size(); ++i) {
                const double x = d.cols[0][i], y = d.cols[1].value(i);
                if (!geo.sx.valid(x) || !geo.sy.valid(y)) continue;
                const QPointF q = t.map(geo.map(x, y));
                const double dd = ((q.x() - w.x()) * (q.x() - w.x()) + (q.y() - w.y()) * (q.y() - w.y())) * bonus;
                if (dd < best) {
                    best = dd;
                    *graph = g;
                    *set = s;
                    *index = i;
                    found = true;
                }
            }
        }
    }
    return found;
}

void PlotView::updateLocator(const QPointF& pt)
{
    int g = graphAt(pt);
    if (g < 0) g = m_doc->currentGraph();
    double x, y;
    if (!worldAt(g, pt, &x, &y)) {
        emit locatorChanged(QString());
        return;
    }
    const Graph* gr = m_doc->project().graph(g);
    auto fmt = [&](double v, const Axis& ax) {
        if (int(ax.format) >= int(LabelFormat::DDMMYY)) {
            ticks::FormatOptions o;
            o.format = ax.format;
            o.precision = ax.precision + 2;
            o.referenceDate = m_doc->project().referenceDate;
            o.pretty = false;
            return ticks::formatValue(v, o);
        }
        return QString::number(v, 'g', 7);
    };
    if (gr->type == GraphType::Polar)
        emit locatorChanged(QStringLiteral("G%1   φ = %2°   r = %3").arg(g).arg(x * 180.0 / M_PI, 0, 'f', 2).arg(y, 0, 'g', 7));
    else
        emit locatorChanged(QStringLiteral("G%1   x = %2   y = %3").arg(g).arg(fmt(x, gr->axes[0]), fmt(y, gr->axes[1])));
}

// ---------------------------------------------------------------- events

void PlotView::resizeEvent(QResizeEvent*) { invalidate(); }

void PlotView::mousePressEvent(QMouseEvent* e)
{
    setFocus();
    const QPointF pt = toPage(e->position());
    m_pressPage = m_lastPage = m_curPage = pt;
    m_pressWidget = e->position().toPoint();
    m_moved = false;
    ++m_dragSession;
    m_dragGraph = graphAt(pt);

    if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && m_tool == Tool::Pan)) {
        if (m_dragGraph >= 0) {
            m_drag = Drag::PanWorld;
            setCursor(Qt::ClosedHandCursor);
        } else {
            m_drag = Drag::PanPage;
        }
        return;
    }
    if (e->button() != Qt::LeftButton) return;

    switch (m_tool) {
    case Tool::Select: {
        const HitItem* h = hitTest(pt);
        selectAt(pt);
        if (h && h->kind == HitItem::Legend) {
            m_drag = Drag::Legend;
            m_dragGraph = h->graph;
        } else if (h && h->kind == HitItem::Object) {
            m_drag = Drag::Object;
            m_dragIndex = h->index;
        } else if (h && (h->kind == HitItem::GraphArea || h->kind == HitItem::SetCurve)) {
            m_drag = Drag::PanWorld;
            m_dragGraph = h->graph;
        }
        break;
    }
    case Tool::ZoomBox:
    case Tool::ZoomX:
    case Tool::ZoomY:
        if (m_dragGraph >= 0) m_drag = Drag::Rubber;
        break;
    case Tool::Line:
    case Tool::Arrow:
    case Tool::Box:
    case Tool::Ellipse: m_drag = Drag::Shape; break;
    case Tool::Text: createShape(pt, pt); break;
    case Tool::EditPoints: {
        int g, s, i;
        if (e->modifiers() & Qt::ShiftModifier) {
            // add a point to the selected (or first) set of the graph under the cursor
            const int gi = m_dragGraph >= 0 ? m_dragGraph : m_doc->currentGraph();
            double x, y;
            if (!worldAt(gi, pt, &x, &y)) break;
            const Selection sel = m_doc->selection();
            int si = (sel.kind == Selection::Set && sel.graph == gi) ? sel.index : -1;
            m_doc->modify(tr("Add point"), [&](Project& p) {
                Graph* gr = p.graph(gi);
                if (!gr) return;
                if (si < 0 || !gr->set(si)) {
                    si = -1;
                    for (int k = 0; k < gr->sets.size(); ++k) if (gr->sets[k].active) { si = k; break; }
                    if (si < 0) {
                        si = gr->nextFreeSet();
                        DataSet& d = gr->ensureSet(si);
                        presets::styleSet(d, si, p.stylePreset);
                        d.symbol.type = Symbol::Circle;
                    }
                }
                DataSet& d = gr->sets[si];
                const int n = d.size();
                d.resize(n + 1);
                d.cols[0][n] = x;
                d.cols[1][n] = y;
            });
            if (si >= 0) m_doc->select(Selection{Selection::Set, gi, si});
            break;
        }
        if (nearestPoint(pt, &g, &s, &i, 10.0)) {
            if (e->modifiers() & Qt::ControlModifier) {
                m_doc->modify(tr("Delete point"), [=](Project& p) {
                    if (Graph* gr = p.graph(g))
                        if (DataSet* d = gr->set(s)) {
                            for (int c = 0; c < d->columns(); ++c) if (i < d->cols[c].size()) d->cols[c].remove(i);
                            if (i < d->strings.size()) d->strings.removeAt(i);
                        }
                });
            } else {
                m_drag = Drag::Point;
                m_dragGraph = g;
                m_dragSet = s;
                m_dragPoint = i;
                m_doc->select(Selection{Selection::Set, g, s});
            }
        }
        break;
    }
    case Tool::Region:
        if (m_regionPts.isEmpty()) m_regionGraph = m_dragGraph >= 0 ? m_dragGraph : m_doc->currentGraph();
        m_regionPts << pt;
        update();
        break;
    case Tool::Crosshair: {
        double x, y;
        if (m_dragGraph >= 0 && worldAt(m_dragGraph, pt, &x, &y)) {
            QToolTip::showText(e->globalPosition().toPoint(), QStringLiteral("x = %1\ny = %2").arg(x, 0, 'g', 10).arg(y, 0, 'g', 10), this);
        }
        break;
    }
    default: break;
    }
}

void PlotView::mouseMoveEvent(QMouseEvent* e)
{
    const QPointF pt = toPage(e->position());
    m_hover = pt;
    m_hoverValid = true;
    m_curPage = pt;
    updateLocator(pt);
    if ((e->position().toPoint() - m_pressWidget).manhattanLength() > 3) m_moved = true;
    const QString key = QStringLiteral("drag%1").arg(m_dragSession);
    switch (m_drag) {
    case Drag::PanWorld:
        if (m_moved) {
            panWorld(m_dragGraph, m_lastPage, pt, key);
            if (m_tool != Tool::Pan) setCursor(Qt::ClosedHandCursor);
        }
        break;
    case Drag::PanPage: {
        const QPointF d = pageToWidget().map(pt) - pageToWidget().map(m_lastPage);
        m_panOffset += d;
        update();
        m_lastPage = toPage(e->position());
        return;
    }
    case Drag::Legend: {
        if (!m_moved) break;
        const QPointF dv = toView(pt) - toView(m_lastPage);
        const int gi = m_dragGraph;
        const QRectF lr = gi < m_legendRects.size() ? m_legendRects[gi] : QRectF();
        const QPointF tl = toView(lr.topLeft());
        m_doc->modify(tr("Move legend"), [=](Project& p) {
            Graph* g = p.graph(gi);
            if (!g) return;
            if (g->legend.autoPlace || g->legend.loctype == LocType::World) {
                g->legend.autoPlace = false;
                g->legend.loctype = LocType::View;
                g->legend.pos = tl;
            }
            g->legend.pos += dv;
        }, key);
        break;
    }
    case Drag::Object: {
        if (!m_moved) break;
        const QPointF a = m_lastPage, b = pt;
        const int oi = m_dragIndex;
        const Project& pr = m_doc->project();
        if (oi < 0 || oi >= pr.objects.size()) break;
        const DrawObject& o = pr.objects[oi];
        QPointF delta;
        if (o.loctype == LocType::World) {
            double ax, ay, bx, by;
            if (!worldAt(o.graph, a, &ax, &ay) || !worldAt(o.graph, b, &bx, &by)) break;
            delta = QPointF(bx - ax, by - ay);
        } else {
            delta = toView(b) - toView(a);
        }
        m_doc->modify(tr("Move annotation"), [=](Project& p) {
            if (oi < 0 || oi >= p.objects.size()) return;
            DrawObject& ob = p.objects[oi];
            ob.pos += delta;
            ob.p1 += delta;
            ob.p2 += delta;
        }, key);
        break;
    }
    case Drag::Point: {
        double x, y;
        if (!worldAt(m_dragGraph, pt, &x, &y)) break;
        const int g = m_dragGraph, s = m_dragSet, i = m_dragPoint;
        const bool lockX = e->modifiers() & Qt::AltModifier;
        m_doc->modify(tr("Move point"), [=](Project& p) {
            if (Graph* gr = p.graph(g))
                if (DataSet* d = gr->set(s))
                    if (i < d->size()) {
                        if (!lockX) d->cols[0][i] = x;
                        d->cols[1][i] = y;
                    }
        }, key);
        break;
    }
    default: break;
    }
    m_lastPage = pt;
    update();
}

void PlotView::mouseReleaseEvent(QMouseEvent* e)
{
    const QPointF pt = toPage(e->position());
    switch (m_drag) {
    case Drag::Rubber: finishRubberBand(m_pressPage, pt); break;
    case Drag::Shape: createShape(m_pressPage, pt); break;
    default: break;
    }
    if (m_drag == Drag::PanWorld && m_tool == Tool::Select) unsetCursor();
    if (m_tool == Tool::Pan) setCursor(Qt::OpenHandCursor);
    m_drag = Drag::None;
    update();
}

void PlotView::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (m_tool == Tool::Region && m_regionPts.size() >= 2) {
        const QVector<QPointF> pts = m_regionPts;
        const int gi = m_regionGraph;
        m_regionPts.clear();
        const Graph* g = m_doc->project().graph(gi);
        if (!g) return;
        const GraphGeometry geo(m_doc->project().page, *g);
        QVector<QPointF> world;
        for (const QPointF& q : pts) {
            double x, y;
            if (geo.inverse(q, &x, &y)) world << QPointF(x, y);
        }
        int slot = -1;
        m_doc->modify(tr("Define region"), [&](Project& p) {
            for (int r = 0; r < 5; ++r) if (!p.regions[r].active) { slot = r; break; }
            if (slot < 0) slot = 0;
            Region& R = p.regions[slot];
            R = Region();
            R.active = true;
            R.type = RegionType::Polygon;
            R.polygon = world;
            R.linkedGraph = gi;
            R.line = LineStyle{theme::accent(), 1.0, 1};
        });
        m_showRegions = true;
        invalidate();
        emit locatorChanged(tr("Region R%1 defined (%2 vertices)").arg(slot).arg(world.size()));
        return;
    }
    if (m_tool == Tool::Select && e->button() == Qt::LeftButton) {
        selectAt(toPage(e->position()));
        emit propertiesRequested();
    }
}

void PlotView::wheelEvent(QWheelEvent* e)
{
    const QPointF pt = toPage(e->position());
    const double steps = e->angleDelta().y() / 120.0;
    if (steps == 0) return;
    if (e->modifiers() & Qt::ControlModifier) {
        // zoom the page view around the cursor
        const QPointF before = e->position();
        const QPointF pagePt = pt;
        const double nz = std::clamp(m_zoom * std::pow(1.15, steps), 0.1, 40.0);
        m_zoom = nz;
        const QPointF after = pageToWidget().map(pagePt);
        m_panOffset += before - after;
        invalidate();
        emit zoomChanged(m_zoom);
        return;
    }
    int g = graphAt(pt);
    if (g < 0) return;
    const double f = std::pow(0.85, steps);
    const bool onlyX = e->modifiers() & Qt::ShiftModifier;
    const bool onlyY = e->modifiers() & Qt::AltModifier;
    zoomWorld(g, pt, onlyY ? 1.0 : f, onlyX ? 1.0 : f, QStringLiteral("wheel"));
    e->accept();
}

void PlotView::keyPressEvent(QKeyEvent* e)
{
    const Selection sel = m_doc->selection();
    if (e->key() == Qt::Key_Escape) {
        m_regionPts.clear();
        setTool(Tool::Select);
        return;
    }
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        if (sel.kind == Selection::Object) {
            m_doc->modify(tr("Delete annotation"), [sel](Project& p) {
                if (sel.index >= 0 && sel.index < p.objects.size()) p.objects.remove(sel.index);
            });
            m_doc->select(Selection{Selection::Page, -1, -1});
        } else if (sel.kind == Selection::Set) {
            m_doc->modify(tr("Delete set"), [sel](Project& p) {
                if (Graph* g = p.graph(sel.graph)) if (sel.index < g->sets.size()) g->sets[sel.index] = DataSet();
            });
            m_doc->select(Selection{Selection::Graph, sel.graph, -1});
        }
        return;
    }
    if (sel.kind == Selection::Object &&
        (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right || e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)) {
        const double step = (e->modifiers() & Qt::ShiftModifier) ? 0.02 : 0.002;
        QPointF d;
        if (e->key() == Qt::Key_Left) d = QPointF(-step, 0);
        if (e->key() == Qt::Key_Right) d = QPointF(step, 0);
        if (e->key() == Qt::Key_Up) d = QPointF(0, step);
        if (e->key() == Qt::Key_Down) d = QPointF(0, -step);
        m_doc->modify(tr("Nudge annotation"), [sel, d](Project& p) {
            if (sel.index < 0 || sel.index >= p.objects.size()) return;
            DrawObject& o = p.objects[sel.index];
            if (o.loctype == LocType::World) return;
            o.pos += d;
            o.p1 += d;
            o.p2 += d;
        }, QStringLiteral("nudge"));
        return;
    }
    QWidget::keyPressEvent(e);
}

void PlotView::leaveEvent(QEvent*)
{
    m_hoverValid = false;
    emit locatorChanged(QString());
    update();
}

void PlotView::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void PlotView::dropEvent(QDropEvent* e)
{
    QStringList files;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile()) files << u.toLocalFile();
    if (!files.isEmpty()) emit filesDropped(files);
}

void PlotView::contextMenuEvent(QContextMenuEvent* e)
{
    selectAt(toPage(e->pos()));
    emit contextMenuRequested(e->globalPos());
}

}  // namespace cactus::ui
