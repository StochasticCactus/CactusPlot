// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - application themes (light/dark/system) and icons.
#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

class QApplication;

namespace cactus::ui {

enum class ThemeMode { System = 0, Light, Dark };

namespace theme {

/// Apply the theme (Fusion with a tuned palette; native style on macOS when following the system).
void apply(QApplication& app, ThemeMode mode, bool nativeStyle);
bool isDark();
/// Accent colour used for selections and focus markers.
QColor accent();
/// Neutral background around the page in the plot view.
QColor canvasBackground();

}  // namespace theme

namespace icons {

/// Load a monochrome SVG icon from :/icons/<name>.svg, tinted for the current theme.
QIcon get(const QString& name);
/// Clear the cache (after a theme change).
void reset();
QIcon appIcon();
/// Small swatch icon for a colour (with checkerboard for transparency).
QIcon swatch(const QColor& c, int size = 16);
/// Icon showing a line/symbol sample for a data set.
QIcon colormapIcon(const QString& name, int w = 96, int h = 14);

}  // namespace icons
}  // namespace cactus::ui
