// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - PostScript / Encapsulated PostScript paint device.
// Qt 6 dropped PostScript output, so this small QPaintEngine writes Level 2
// PostScript directly. Text is converted to glyph outlines (exact, font
// independent output); colours with alpha are blended against white.
#pragma once

#include <QByteArray>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainterPath>
#include <QRectF>
#include <memory>

namespace cactus {

class PsPaintEngine;

class PsDevice : public QPaintDevice {
public:
    PsDevice(double widthPt, double heightPt, bool eps, bool grayscale = false, bool tightBBox = false);
    ~PsDevice() override;

    QPaintEngine* paintEngine() const override;
    QByteArray data() const;
    void setTitle(const QString& t) { m_title = t; }
    QString title() const { return m_title; }

protected:
    int metric(PaintDeviceMetric m) const override;

private:
    friend class PsPaintEngine;
    double m_w, m_h;
    bool m_eps, m_gray, m_tight;
    QString m_title;
    std::unique_ptr<PsPaintEngine> m_engine;
};

class PsPaintEngine : public QPaintEngine {
public:
    explicit PsPaintEngine(PsDevice* dev);

    bool begin(QPaintDevice* pdev) override;
    bool end() override;
    void updateState(const QPaintEngineState& state) override;
    void drawPath(const QPainterPath& path) override;
    void drawPolygon(const QPointF* points, int pointCount, PolygonDrawMode mode) override;
    void drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr) override;
    void drawImage(const QRectF& r, const QImage& image, const QRectF& sr, Qt::ImageConversionFlags flags) override;
    void drawTextItem(const QPointF& p, const QTextItem& textItem) override;
    Type type() const override { return QPaintEngine::User; }

    QByteArray result() const { return m_out; }

private:
    void emitPath(const QPainterPath& devPath);
    void setColor(const QColor& c);
    void applyClip();
    void track(const QRectF& r);

    PsDevice* m_dev;
    QByteArray m_body;
    QByteArray m_out;
    QPen m_pen;
    QBrush m_brush;
    QTransform m_xf;
    bool m_clipEnabled = false;
    QPainterPath m_clip;  // device coordinates
    bool m_clipDirty = false;
    bool m_clipActive = false;
    QRectF m_bbox;
};

}  // namespace cactus
