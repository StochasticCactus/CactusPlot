// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - interactive plot canvas.
#pragma once

#include <QImage>
#include <QPointer>
#include <QVector>
#include <QWidget>

#include "render/PlotRenderer.h"

namespace cactus::ui {

class Document;

class PlotView : public QWidget {
    Q_OBJECT
public:
    enum class Tool { Select, Pan, ZoomBox, ZoomX, ZoomY, Text, Line, Arrow, Box, Ellipse, Crosshair, EditPoints, Region };
    Q_ENUM(Tool)

    explicit PlotView(Document* doc, QWidget* parent = nullptr);

    Tool tool() const { return m_tool; }
    void setTool(Tool t);
    double zoom() const { return m_zoom; }
    void setZoom(double z);
    void zoomIn() { setZoom(m_zoom * 1.25); }
    void zoomOut() { setZoom(m_zoom / 1.25); }
    void fitPage();
    void setShowRegions(bool on);
    bool showRegions() const { return m_showRegions; }
    /// Graph whose viewport contains the page point (topmost), or -1.
    int graphAt(const QPointF& pagePt) const;

signals:
    void locatorChanged(const QString& text);
    void zoomChanged(double zoom);
    void toolChanged(Tool t);
    void propertiesRequested();
    void contextMenuRequested(const QPoint& globalPos);
    void filesDropped(const QStringList& paths);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void leaveEvent(QEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    QTransform pageToWidget() const;
    QPointF toPage(const QPointF& widgetPt) const;
    QRectF pageRectWidget() const;
    void invalidate();
    void ensureRendered();
    const HitItem* hitTest(const QPointF& pagePt) const;
    void selectAt(const QPointF& pagePt);
    QPointF toView(const QPointF& pagePt) const;
    bool worldAt(int graph, const QPointF& pagePt, double* wx, double* wy) const;
    void zoomWorld(int graph, const QPointF& pagePt, double fx, double fy, const QString& mergeKey);
    void panWorld(int graph, const QPointF& fromPage, const QPointF& toPage, const QString& mergeKey);
    void finishRubberBand(const QPointF& a, const QPointF& b);
    void createShape(const QPointF& a, const QPointF& b);
    bool nearestPoint(const QPointF& pagePt, int* graph, int* set, int* index, double maxDist) const;
    void updateLocator(const QPointF& pagePt);
    void drawOverlay(QPainter& p);

    Document* m_doc;
    Tool m_tool = Tool::Select;
    double m_zoom = 1.0;
    QPointF m_panOffset;  // widget pixels
    QImage m_cache;
    bool m_dirty = true;
    QVector<HitItem> m_hits;
    QVector<QRectF> m_legendRects;
    bool m_showRegions = false;

    // interaction state
    enum class Drag { None, PanWorld, PanPage, Rubber, Legend, Object, Point, Shape };
    Drag m_drag = Drag::None;
    QPointF m_pressPage, m_lastPage, m_curPage;
    QPoint m_pressWidget;
    int m_dragGraph = -1, m_dragIndex = -1, m_dragSet = -1, m_dragPoint = -1;
    int m_dragSession = 0;
    bool m_moved = false;
    QVector<QPointF> m_regionPts;  // page coords
    int m_regionGraph = -1;
    QPointF m_hover;
    bool m_hoverValid = false;
};

}  // namespace cactus::ui
