// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - exporting projects to files (vector and raster formats).
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "core/Model.h"

namespace cactus::exporter {

struct Options {
    QString format;          ///< "pdf", "svg", "eps", "ps", "png", "jpeg", "tiff", "bmp", "ppm", "webp"
    double dpi = 300.0;      ///< raster resolution
    bool transparent = false;
    bool grayscale = false;
    int quality = 95;        ///< JPEG/WebP quality
    bool tightBBox = false;  ///< EPS: crop the bounding box to the drawing
    QString title;
};

/// Supported export formats (lower-case extensions).
QStringList formats();
/// File dialog filter string.
QString fileFilter();
/// Map a Grace device name or extension to a format ("PostScript" -> "ps", "JPG" -> "jpeg").
QString normalizeFormat(const QString& deviceOrExt);
QString extensionFor(const QString& format);
bool isVector(const QString& format);

bool exportToFile(const Project& p, const QString& path, const Options& o, QString* error = nullptr);
QByteArray exportToBytes(const Project& p, const Options& o, QString* error = nullptr);

}  // namespace cactus::exporter
