// SPDX-License-Identifier: GPL-2.0-or-later
#include "render/PsEngine.h"

#include <QDateTime>
#include <QFontMetricsF>
#include <QImage>
#include <QPixmap>
#include <QTextItem>
#include <cmath>

#include "Version.h"

namespace cactus {

namespace {
QByteArray f(double v)
{
    if (std::fabs(v) < 1e-9) v = 0;
    return QByteArray::number(v, 'f', 3);
}
}  // namespace

// ------------------------------------------------------------------ device

PsDevice::PsDevice(double w, double h, bool eps, bool gray, bool tight)
    : m_w(w), m_h(h), m_eps(eps), m_gray(gray), m_tight(tight)
{
    m_engine = std::make_unique<PsPaintEngine>(this);
}

PsDevice::~PsDevice() = default;

QPaintEngine* PsDevice::paintEngine() const { return m_engine.get(); }

QByteArray PsDevice::data() const { return m_engine->result(); }

int PsDevice::metric(PaintDeviceMetric m) const
{
    switch (m) {
    case PdmWidth: return int(std::ceil(m_w));
    case PdmHeight: return int(std::ceil(m_h));
    case PdmWidthMM: return int(m_w * 25.4 / 72.0);
    case PdmHeightMM: return int(m_h * 25.4 / 72.0);
    case PdmNumColors: return 0x7fffffff;
    case PdmDepth: return 32;
    case PdmDpiX:
    case PdmDpiY:
    case PdmPhysicalDpiX:
    case PdmPhysicalDpiY: return 72;
    case PdmDevicePixelRatio: return 1;
    case PdmDevicePixelRatioScaled: return int(QPaintDevice::devicePixelRatioFScale());
    default: return 0;
    }
}

// ------------------------------------------------------------------ engine

PsPaintEngine::PsPaintEngine(PsDevice* dev) : QPaintEngine(QPaintEngine::AllFeatures), m_dev(dev) {}

bool PsPaintEngine::begin(QPaintDevice*)
{
    m_body.clear();
    m_out.clear();
    m_bbox = QRectF();
    m_clipEnabled = false;
    m_clipActive = false;
    m_clip = QPainterPath();
    m_xf = QTransform();
    setActive(true);
    return true;
}

void PsPaintEngine::track(const QRectF& r)
{
    if (r.isNull() && r.width() == 0 && r.height() == 0) return;
    m_bbox = m_bbox.isNull() ? r : m_bbox.united(r);
}

bool PsPaintEngine::end()
{
    if (m_clipActive) m_body += "grestore\n";
    const double W = m_dev->m_w, H = m_dev->m_h;
    QByteArray head;
    QRectF bb(0, 0, W, H);
    if (m_dev->m_tight && !m_bbox.isNull()) bb = m_bbox.adjusted(-1, -1, 1, 1).intersected(QRectF(0, 0, W, H));
    // PostScript bounding boxes are bottom-left based
    const int llx = int(std::floor(bb.left())), lly = int(std::floor(H - bb.bottom()));
    const int urx = int(std::ceil(bb.right())), ury = int(std::ceil(H - bb.top()));
    head += m_dev->m_eps ? "%!PS-Adobe-3.0 EPSF-3.0\n" : "%!PS-Adobe-3.0\n";
    head += "%%BoundingBox: " + QByteArray::number(llx) + ' ' + QByteArray::number(lly) + ' ' + QByteArray::number(urx) + ' ' +
            QByteArray::number(ury) + '\n';
    head += "%%HiResBoundingBox: " + f(bb.left()) + ' ' + f(H - bb.bottom()) + ' ' + f(bb.right()) + ' ' + f(H - bb.top()) + '\n';
    head += "%%Creator: " CACTUS_APP_NAME " " CACTUS_VERSION "\n";
    if (!m_dev->m_title.isEmpty()) head += "%%Title: " + m_dev->m_title.toLatin1() + '\n';
    head += "%%CreationDate: " + QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1() + '\n';
    head += "%%LanguageLevel: 2\n%%Pages: 1\n";
    if (!m_dev->m_eps) head += "%%DocumentMedia: Plot " + f(W) + ' ' + f(H) + " 0 () ()\n";
    head += "%%EndComments\n%%BeginProlog\n";
    head += "/m {moveto} bind def /l {lineto} bind def /c {curveto} bind def /cp {closepath} bind def\n";
    head += "/rgb {setrgbcolor} bind def /g {setgray} bind def /lw {setlinewidth} bind def\n";
    head += "%%EndProlog\n%%Page: 1 1\n";
    if (!m_dev->m_eps) head += "<< /PageSize [" + f(W) + ' ' + f(H) + "] >> setpagedevice\n";
    head += "gsave\n0 " + f(H) + " translate 1 -1 scale\n";
    m_out = head + m_body + "grestore\nshowpage\n%%Trailer\n%%EOF\n";
    setActive(false);
    return true;
}

void PsPaintEngine::updateState(const QPaintEngineState& st)
{
    const DirtyFlags d = st.state();
    if (d & DirtyPen) m_pen = st.pen();
    if (d & DirtyBrush) m_brush = st.brush();
    if (d & DirtyTransform) m_xf = st.transform();
    if (d & (DirtyClipPath | DirtyClipRegion | DirtyClipEnabled)) {
        if (d & DirtyClipEnabled) m_clipEnabled = st.isClipEnabled();
        QPainterPath newClip;
        bool have = false;
        if (d & DirtyClipPath) {
            newClip = st.transform().map(st.clipPath());
            have = true;
        } else if (d & DirtyClipRegion) {
            QPainterPath p;
            for (const QRect& r : st.clipRegion()) p.addRect(r);
            newClip = st.transform().map(p);
            have = true;
        }
        if (have) {
            switch (st.clipOperation()) {
            case Qt::NoClip: m_clipEnabled = false; m_clip = QPainterPath(); break;
            case Qt::ReplaceClip: m_clipEnabled = true; m_clip = newClip; break;
            case Qt::IntersectClip:
                m_clip = (m_clipEnabled && !m_clip.isEmpty()) ? m_clip.intersected(newClip) : newClip;
                m_clipEnabled = true;
                break;
            }
        }
        if (!(d & (DirtyClipPath | DirtyClipRegion)) && (d & DirtyClipEnabled)) {
            // only the enabled flag changed; painter state restore re-sends the clip path
            if (st.painter() && st.painter()->hasClipping()) {
                m_clip = st.painter()->clipPath() * st.painter()->transform();
                m_clipEnabled = true;
            } else {
                m_clipEnabled = false;
            }
        }
        m_clipDirty = true;
    }
}

void PsPaintEngine::applyClip()
{
    if (!m_clipDirty) return;
    m_clipDirty = false;
    if (m_clipActive) {
        m_body += "grestore\n";
        m_clipActive = false;
    }
    if (m_clipEnabled && !m_clip.isEmpty()) {
        m_body += "gsave\nnewpath\n";
        emitPath(m_clip);
        m_body += "clip newpath\n";
        m_clipActive = true;
    }
}

void PsPaintEngine::setColor(const QColor& c0)
{
    // blend alpha against white (PostScript has no transparency)
    const double a = c0.alphaF();
    const double r = c0.redF() * a + (1 - a), g = c0.greenF() * a + (1 - a), b = c0.blueF() * a + (1 - a);
    if (m_dev->m_gray) {
        m_body += f(0.299 * r + 0.587 * g + 0.114 * b) + " g\n";
    } else {
        m_body += f(r) + ' ' + f(g) + ' ' + f(b) + " rgb\n";
    }
}

void PsPaintEngine::emitPath(const QPainterPath& p)
{
    for (int i = 0; i < p.elementCount(); ++i) {
        const QPainterPath::Element e = p.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement: m_body += f(e.x) + ' ' + f(e.y) + " m\n"; break;
        case QPainterPath::LineToElement: m_body += f(e.x) + ' ' + f(e.y) + " l\n"; break;
        case QPainterPath::CurveToElement: {
            const QPainterPath::Element c1 = p.elementAt(i + 1), c2 = p.elementAt(i + 2);
            m_body += f(e.x) + ' ' + f(e.y) + ' ' + f(c1.x) + ' ' + f(c1.y) + ' ' + f(c2.x) + ' ' + f(c2.y) + " c\n";
            i += 2;
            break;
        }
        default: break;
        }
    }
}

void PsPaintEngine::drawPath(const QPainterPath& path)
{
    applyClip();
    const QPainterPath dp = m_xf.map(path);
    if (dp.isEmpty()) return;
    const bool fill = m_brush.style() != Qt::NoBrush && m_brush.color().alpha() > 0;
    const bool stroke = m_pen.style() != Qt::NoPen && m_pen.color().alpha() > 0;
    if (!fill && !stroke) return;
    QRectF br = dp.boundingRect();
    if (fill) {
        m_body += "newpath\n";
        emitPath(dp);
        QColor fc = m_brush.color();
        if (m_brush.gradient()) {
            const auto stops = m_brush.gradient()->stops();
            if (!stops.isEmpty()) {
                // average of the gradient stops as a flat approximation
                double r = 0, g = 0, b = 0, a = 0;
                for (const auto& s : stops) { r += s.second.redF(); g += s.second.greenF(); b += s.second.blueF(); a += s.second.alphaF(); }
                const int n = stops.size();
                fc = QColor::fromRgbF(float(r / n), float(g / n), float(b / n), float(a / n));
            }
        } else if (m_brush.style() != Qt::SolidPattern) {
            // hatch patterns: use a lighter tint of the colour
            fc.setAlphaF(fc.alphaF() * 0.45f);
        }
        setColor(fc);
        m_body += (path.fillRule() == Qt::OddEvenFill) ? "eofill\n" : "fill\n";
    }
    if (stroke) {
        const double scale = std::sqrt(std::fabs(m_xf.determinant()));
        double w = m_pen.widthF() * (m_pen.isCosmetic() ? 1.0 : scale);
        if (w <= 0) w = 0.3;
        m_body += "newpath\n";
        emitPath(dp);
        setColor(m_pen.color());
        m_body += f(w) + " lw ";
        m_body += QByteArray::number(m_pen.capStyle() == Qt::FlatCap ? 0 : m_pen.capStyle() == Qt::RoundCap ? 1 : 2) + " setlinecap ";
        m_body += QByteArray::number(m_pen.joinStyle() == Qt::MiterJoin || m_pen.joinStyle() == Qt::SvgMiterJoin ? 0 : m_pen.joinStyle() == Qt::RoundJoin ? 1 : 2) + " setlinejoin\n";
        if (m_pen.style() != Qt::SolidLine) {
            const QVector<qreal> dp2 = m_pen.dashPattern();
            m_body += "[";
            for (qreal d : dp2) m_body += f(std::max(0.01, d * w)) + ' ';
            m_body += "] 0 setdash\n";
        } else {
            m_body += "[] 0 setdash\n";
        }
        m_body += "stroke\n";
        br.adjust(-w / 2, -w / 2, w / 2, w / 2);
    }
    if (m_clipEnabled && !m_clip.isEmpty()) br = br.intersected(m_clip.boundingRect());
    track(br);
}

void PsPaintEngine::drawPolygon(const QPointF* pts, int n, PolygonDrawMode mode)
{
    QPainterPath p;
    if (n <= 0) return;
    p.moveTo(pts[0]);
    for (int i = 1; i < n; ++i) p.lineTo(pts[i]);
    if (mode != PolylineMode) p.closeSubpath();
    p.setFillRule(mode == OddEvenMode ? Qt::OddEvenFill : Qt::WindingFill);
    if (mode == PolylineMode) {
        const QBrush saved = m_brush;
        m_brush = Qt::NoBrush;
        drawPath(p);
        m_brush = saved;
    } else {
        drawPath(p);
    }
}

void PsPaintEngine::drawTextItem(const QPointF& pt, const QTextItem& ti)
{
    QPainterPath path;
    path.addText(QPointF(0, 0), ti.font(), ti.text());
    // QPainterPath::addText uses the default screen DPI; rescale to this device (72 dpi)
    const QFontMetricsF devFm(ti.font(), m_dev);
    const QFontMetricsF defFm(ti.font());
    const double r = defFm.ascent() > 0 ? devFm.ascent() / defFm.ascent() : 1.0;
    QTransform t;
    t.translate(pt.x(), pt.y());
    t.scale(r, r);
    path = t.map(path);
    const QPen savedPen = m_pen;
    const QBrush savedBrush = m_brush;
    m_brush = QBrush(m_pen.color());
    m_pen = Qt::NoPen;
    drawPath(path);
    m_pen = savedPen;
    m_brush = savedBrush;
}

void PsPaintEngine::drawImage(const QRectF& r, const QImage& image, const QRectF& sr, Qt::ImageConversionFlags)
{
    applyClip();
    const QImage img = image.copy(sr.toRect()).convertToFormat(QImage::Format_RGB888);
    if (img.isNull()) return;
    const QRectF dr = m_xf.mapRect(r);
    m_body += "gsave\n" + f(dr.left()) + ' ' + f(dr.bottom()) + " translate " + f(dr.width()) + ' ' + f(-dr.height()) + " scale\n";
    m_body += QByteArray::number(img.width()) + ' ' + QByteArray::number(img.height()) + " 8 [" + QByteArray::number(img.width()) +
              " 0 0 " + QByteArray::number(-img.height()) + " 0 " + QByteArray::number(img.height()) + "]\n";
    m_body += "currentfile /ASCIIHexDecode filter false 3 colorimage\n";
    static const char hex[] = "0123456789abcdef";
    int col = 0;
    for (int y = 0; y < img.height(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < img.width() * 3; ++x) {
            m_body += hex[line[x] >> 4];
            m_body += hex[line[x] & 15];
            if (++col == 39) { m_body += '\n'; col = 0; }
        }
    }
    m_body += ">\ngrestore\n";
    track(dr);
}

void PsPaintEngine::drawPixmap(const QRectF& r, const QPixmap& pm, const QRectF& sr)
{
    drawImage(r, pm.toImage(), sr, Qt::AutoColor);
}

}  // namespace cactus
