// SPDX-License-Identifier: GPL-2.0-or-later
#include "render/Export.h"

#include <QBuffer>
#include <QFile>
#include <QImageWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSvgGenerator>
#include <cmath>

#include "Version.h"
#include "plot/RichText.h"
#include "render/PlotRenderer.h"
#include "render/PsEngine.h"

namespace cactus::exporter {

QStringList formats()
{
    QStringList l{QStringLiteral("pdf"), QStringLiteral("svg"), QStringLiteral("eps"), QStringLiteral("ps"),
                  QStringLiteral("png"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("ppm")};
    const auto supported = QImageWriter::supportedImageFormats();
    if (supported.contains("tiff")) l << QStringLiteral("tiff");
    if (supported.contains("webp")) l << QStringLiteral("webp");
    return l;
}

QString fileFilter()
{
    QStringList f;
    f << QStringLiteral("PDF document (*.pdf)") << QStringLiteral("SVG image (*.svg)")
      << QStringLiteral("Encapsulated PostScript (*.eps)") << QStringLiteral("PostScript (*.ps)")
      << QStringLiteral("PNG image (*.png)") << QStringLiteral("JPEG image (*.jpg *.jpeg)")
      << QStringLiteral("BMP image (*.bmp)") << QStringLiteral("Portable pixmap (*.ppm *.pnm)");
    if (formats().contains(QStringLiteral("tiff"))) f << QStringLiteral("TIFF image (*.tif *.tiff)");
    if (formats().contains(QStringLiteral("webp"))) f << QStringLiteral("WebP image (*.webp)");
    return f.join(QStringLiteral(";;"));
}

QString normalizeFormat(const QString& d)
{
    const QString l = d.trimmed().toLower().remove(QLatin1Char('.'));
    if (l == QLatin1String("postscript") || l == QLatin1String("ps")) return QStringLiteral("ps");
    if (l == QLatin1String("eps") || l == QLatin1String("epsf") || l == QLatin1String("epsi")) return QStringLiteral("eps");
    if (l == QLatin1String("jpg") || l == QLatin1String("jpeg")) return QStringLiteral("jpeg");
    if (l == QLatin1String("tif") || l == QLatin1String("tiff")) return QStringLiteral("tiff");
    if (l == QLatin1String("pnm") || l == QLatin1String("ppm") || l == QLatin1String("pgm") || l == QLatin1String("pbm"))
        return QStringLiteral("ppm");
    if (l == QLatin1String("mif")) return QStringLiteral("svg");  // FrameMaker MIF is obsolete: closest vector format
    return l;
}

QString extensionFor(const QString& format)
{
    const QString f = normalizeFormat(format);
    if (f == QLatin1String("jpeg")) return QStringLiteral("jpg");
    if (f == QLatin1String("tiff")) return QStringLiteral("tif");
    return f;
}

bool isVector(const QString& format)
{
    const QString f = normalizeFormat(format);
    return f == QLatin1String("pdf") || f == QLatin1String("svg") || f == QLatin1String("eps") || f == QLatin1String("ps");
}

namespace {

QString titleOf(const Project& p, const Options& o)
{
    if (!o.title.isEmpty()) return o.title;
    for (const Graph& g : p.graphs)
        if (g.active && !g.title.text.isEmpty()) return richtext::plainText(g.title.text);
    return QStringLiteral(CACTUS_APP_NAME " plot");
}

}  // namespace

QByteArray exportToBytes(const Project& p, const Options& o, QString* error)
{
    const QString fmt = normalizeFormat(o.format);
    const double W = p.page.width, H = p.page.height;
    QByteArray bytes;

    if (fmt == QLatin1String("pdf")) {
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        QPdfWriter w(&buf);
        w.setCreator(QStringLiteral(CACTUS_APP_NAME " " CACTUS_VERSION));
        w.setTitle(titleOf(p, o));
        w.setResolution(1200);
        w.setPageLayout(QPageLayout(QPageSize(QSizeF(W, H), QPageSize::Point, QString(), QPageSize::ExactMatch),
                                    QPageLayout::Portrait, QMarginsF(0, 0, 0, 0)));
        QPainter painter(&w);
        RenderOptions ro;
        ro.drawPageBackground = !o.transparent;
        PlotRenderer r(p, ro);
        r.render(&painter, QRectF(0, 0, w.width(), w.height()));
        painter.end();
        return bytes;
    }
    if (fmt == QLatin1String("svg")) {
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        QSvgGenerator gen;
        gen.setOutputDevice(&buf);
        gen.setResolution(72);
        gen.setSize(QSize(int(std::ceil(W)), int(std::ceil(H))));
        gen.setViewBox(QRectF(0, 0, W, H));
        gen.setTitle(titleOf(p, o));
        gen.setDescription(QStringLiteral("Created with " CACTUS_APP_NAME " " CACTUS_VERSION));
        QPainter painter(&gen);
        RenderOptions ro;
        ro.drawPageBackground = !o.transparent;
        PlotRenderer r(p, ro);
        r.render(&painter, QRectF(0, 0, W, H));
        painter.end();
        return bytes;
    }
    if (fmt == QLatin1String("eps") || fmt == QLatin1String("ps")) {
        PsDevice dev(W, H, fmt == QLatin1String("eps"), o.grayscale || o.format.contains(QLatin1String("gray")), o.tightBBox);
        dev.setTitle(titleOf(p, o));
        QPainter painter(&dev);
        RenderOptions ro;
        ro.drawPageBackground = !o.transparent;
        PlotRenderer r(p, ro);
        r.render(&painter, QRectF(0, 0, W, H));
        painter.end();
        return dev.data();
    }
    // raster
    const double dpi = o.dpi > 0 ? o.dpi : 300.0;
    const int pw = std::max(1, int(std::lround(W * dpi / 72.0)));
    const bool alpha = o.transparent && (fmt == QLatin1String("png") || fmt == QLatin1String("tiff") || fmt == QLatin1String("webp"));
    QImage img = renderToImage(p, pw, alpha, dpi);
    if (o.grayscale) img = img.convertToFormat(QImage::Format_Grayscale8);
    else if (!alpha) img = img.convertToFormat(QImage::Format_RGB32);
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    QImageWriter wr(&buf, fmt == QLatin1String("jpeg") ? QByteArray("jpg") : fmt.toLatin1());
    if (fmt == QLatin1String("jpeg") || fmt == QLatin1String("webp")) wr.setQuality(o.quality);
    if (fmt == QLatin1String("png")) wr.setCompression(9);
    wr.setText(QStringLiteral("Software"), QStringLiteral(CACTUS_APP_NAME " " CACTUS_VERSION));
    wr.setText(QStringLiteral("Title"), titleOf(p, o));
    if (!wr.write(img)) {
        if (error) *error = QStringLiteral("cannot encode %1: %2").arg(fmt, wr.errorString());
        return QByteArray();
    }
    return bytes;
}

bool exportToFile(const Project& p, const QString& path, const Options& o0, QString* error)
{
    Options o = o0;
    if (o.format.isEmpty()) {
        const int dot = path.lastIndexOf(QLatin1Char('.'));
        o.format = dot >= 0 ? path.mid(dot + 1) : QStringLiteral("png");
    }
    if (!formats().contains(normalizeFormat(o.format))) {
        if (error) *error = QStringLiteral("unsupported export format '%1'").arg(o.format);
        return false;
    }
    const QByteArray data = exportToBytes(p, o, error);
    if (data.isEmpty()) {
        if (error && error->isEmpty()) *error = QStringLiteral("export produced no data");
        return false;
    }
    if (path == QLatin1String("-")) {
        QFile out;
        if (!out.open(stdout, QIODevice::WriteOnly)) return false;
        out.write(data);
        return true;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    f.write(data);
    if (!f.commit()) {
        if (error) *error = QStringLiteral("cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

}  // namespace cactus::exporter
