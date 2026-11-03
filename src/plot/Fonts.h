// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - font registry. Maps Grace/PostScript font names to the bundled
// TeX Gyre fonts (metric clones of Times, Helvetica and Courier) so output is
// identical on every platform; any other name is treated as a system family.
#pragma once

#include <QFont>
#include <QString>
#include <QStringList>

class QPaintDevice;

namespace cactus::fonts {

/// Load the bundled fonts (needs a QGuiApplication). Safe to call repeatedly.
void registerBundledFonts();
bool bundledFontsAvailable();

/// Grace's 14 standard font names (index = Grace font id).
const QStringList& standardFontNames();
/// Names offered in font pickers: standard ones plus installed families.
QStringList availableFontNames();

/// Build a QFont for a (Grace or system) font name with the given size in
/// painter logical units, for drawing on `device` (may be null).
QFont font(const QString& name, double sizeUnits, const QPaintDevice* device);

/// Is this the Symbol font (characters use the Adobe Symbol encoding)?
bool isSymbolFont(const QString& name);
bool isDingbatsFont(const QString& name);
/// Map a character from the Symbol / ZapfDingbats encoding to Unicode.
QChar symbolToUnicode(QChar c);
QChar dingbatToUnicode(QChar c);

/// Size in points of Grace char size 1.0 on a page whose shorter side is `pageUnit` points.
inline double charSizeToUnits(double charSize, double pageUnit) { return charSize * 0.028 * pageUnit; }
/// Grace line width (relative) to points.
inline double lineWidthToUnits(double lw, double pageUnit) { return lw * 0.0015 * pageUnit; }

}  // namespace cactus::fonts
