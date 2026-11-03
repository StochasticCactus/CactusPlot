// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - renders a Project with QPainter (screen, raster, PDF, SVG, PS).
#pragma once

#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "core/Model.h"
#include "plot/Scale.h"

class QPainter;
class QPen;
class QBrush;

namespace cactus {

/// Coordinate mapping for one graph (world <-> page points, y down).
class GraphGeometry {
public:
    GraphGeometry() = default;
    GraphGeometry(const Page& page, const Graph& g);

    GraphType type = GraphType::XY;
    QRectF view;  ///< viewport rectangle in page points
    Scale sx, sy;
    QPointF center;
    double radius = 0.0;
    double unit = 612.0;
    double pageHeight = 612.0;
    bool valid = false;

    QPointF map(double wx, double wy) const;
    bool inverse(const QPointF& page, double* wx, double* wy) const;
    QPointF viewToPage(double vx, double vy) const { return QPointF(vx * unit, pageHeight - vy * unit); }
    QPointF pageToView(const QPointF& p) const { return QPointF(p.x() / unit, (pageHeight - p.y()) / unit); }
    bool isPolarLike() const { return type == GraphType::Polar || type == GraphType::Pie; }
};

struct RenderOptions {
    bool antialias = true;
    bool drawPageBackground = true;
    bool showRegions = false;  ///< screen-only display of regions
    int highlightGraph = -1;   ///< draw a focus marker around this graph (screen only)
    bool collectHits = false;  ///< record hit-testing shapes (interactive views)
};

/// Hit-testing information produced while rendering (page points).
struct HitItem {
    enum Kind { GraphArea, SetCurve, AxisTicks, AxisLabel, Legend, Title, Subtitle, Object, Timestamp };
    Kind kind;
    int graph = -1;
    int index = -1;  ///< set id, axis id, object id
    QPainterPath shape;
};

class PlotRenderer {
public:
    explicit PlotRenderer(const Project& project, RenderOptions opts = {});

    /// Render the whole page so that it fills `target` (painter logical coordinates).
    void render(QPainter* p, const QRectF& target);
    /// Render with the painter already set up in page points (y down).
    void renderPage(QPainter* p);
    const QVector<HitItem>& hitItems() const { return m_hits; }

    /// Static helpers shared with the UI.
    static QPen makePen(const LineStyle& ls, double unit);
    static QBrush makeBrush(const FillStyle& fs, const QRectF& area = QRectF());
    static QPainterPath symbolPath(Symbol s, double radius);
    static QVector<qreal> dashPattern(int style);

    /// Legend rectangle (page points) as last rendered for graph gi.
    QRectF legendRect(int gi) const;

private:
    void drawGraph(QPainter* p, int gi);
    void drawGrid(QPainter* p, const Graph& g, const GraphGeometry& geo);
    void drawFrame(QPainter* p, const Graph& g, const GraphGeometry& geo, bool background);
    void drawAxes(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo);
    void drawPolarAxes(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo);
    void drawSmithGrid(QPainter* p, const Graph& g, const GraphGeometry& geo);
    void drawSet(QPainter* p, int gi, const Graph& g, int si, const GraphGeometry& geo, const QVector<double>* stackBase, int pass);
    void drawPie(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo);
    void drawLegend(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo);
    void drawTitles(QPainter* p, int gi, const Graph& g, const GraphGeometry& geo);
    void drawColorBars(QPainter* p, const Graph& g, const GraphGeometry& geo);
    void drawObject(QPainter* p, int oi, const DrawObject& o);
    void drawArrowHead(QPainter* p, const QPointF& tip, const QPointF& from, const DrawObject& o, const QPen& pen);
    void drawSymbol(QPainter* p, const DataSet& s, const QPointF& pt, double sizeFactor, const QColor* override);
    void drawErrorBars(QPainter* p, const DataSet& s, const GraphGeometry& geo, double xoff, const QVector<double>* base);
    void drawAnnotatedValues(QPainter* p, const DataSet& s, const GraphGeometry& geo, const QVector<QPointF>& pts,
                             const QVector<double>* stackBase);
    void drawRegions(QPainter* p);

    double ts(double charSize) const;   ///< text size in points
    double lw(double width) const;      ///< line width in points

    const Project& m_proj;
    RenderOptions m_opt;
    double m_unit = 612.0;
    QVector<HitItem> m_hits;
    QVector<QRectF> m_legendRects;
};

/// Render a project to a QImage of the given pixel width (height follows the page aspect).
QImage renderToImage(const Project& project, int pixelWidth, bool transparent = false, double dpi = 0.0);

}  // namespace cactus
