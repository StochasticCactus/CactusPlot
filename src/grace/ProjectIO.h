// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - project files: Grace .agr (read/write) and the native .cplot
// format (Grace syntax plus CactusPlot extensions, lossless).
#pragma once

#include <QString>
#include <QStringList>

#include "core/Model.h"

namespace cactus::projectio {

enum class Dialect {
    Grace,   ///< strict Grace 5.1 syntax, readable by xmgrace
    Native   ///< Grace syntax + extensions (alpha colours, colormaps, presets...)
};

/// Serialise a project.
QString write(const Project& p, Dialect d);
/// Save, choosing the dialect from the extension (.agr/.xvgr -> Grace, otherwise Native).
bool save(const Project& p, const QString& path, QString* error = nullptr);
bool saveAs(const Project& p, const QString& path, Dialect d, QString* error = nullptr);

/// Load a project file (.cplot, .agr, xmgr .par/.xvgr). Returns false on fatal errors.
bool load(const QString& path, Project* out, QString* error = nullptr, QStringList* warnings = nullptr);
bool loadText(const QString& text, Project* out, const QString& sourceName, QString* error = nullptr,
              QStringList* warnings = nullptr);
/// Does this text look like a project file (as opposed to plain data)?
bool isProjectText(const QString& text);

}  // namespace cactus::projectio
