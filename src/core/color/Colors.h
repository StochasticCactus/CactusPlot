// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - colour library: named colours, qualitative palettes and colormaps.
#pragma once

#include <QColor>
#include <QString>
#include <QVector>

namespace cactus {

struct NamedColor {
    QString name;
    QColor color;
    QString source;  ///< "Grace", "CSS", "X11", "xkcd", "Tableau"
};

struct Palette {
    QString name;
    QString group;
    QVector<QColor> colors;
};

class Colormap {
public:
    QString name;
    QString category;  ///< Perceptual, Sequential, Diverging, Cyclic, Miscellaneous
    QString source;
    QVector<QRgb> samples;

    /// Colour at t in [0,1] (clamped), linearly interpolated.
    QColor at(double t) const;
    /// Colour at t, remapped through an optional sub-range and reversal.
    QColor at(double t, double lo, double hi, bool reversed) const;
};

namespace colors {

/// The 16 colours of Grace's default colour map, index 0..15.
const QVector<NamedColor>& graceDefaults();
QColor graceColor(int index);

/// All named colours (Grace, CSS, Tableau, X11, xkcd), about 1800 entries.
const QVector<NamedColor>& namedColors();
/// Qualitative palettes (Okabe-Ito, Tableau, ColorBrewer, Tol, Glasbey, ...).
const QVector<Palette>& palettes();
const Palette* palette(const QString& name);
/// Continuous colormaps (viridis, cmocean, Crameri, CET, ColorBrewer, ...).
const QVector<Colormap>& colormaps();
const Colormap* colormap(const QString& name);

/// The default colour cycle used for new data sets.
const QVector<QColor>& defaultCycle();

/// Parse a colour: names (case-insensitive, also "xkcd:name"), "#rgb", "#rrggbb",
/// "#rrggbbaa", "rgb(r,g,b)", "rgba(r,g,b,a)".
bool parse(const QString& text, QColor* out);
/// Serialise a colour as "#rrggbb" or "#rrggbbaa" if not opaque.
QString toHex(const QColor& c);
/// Best name for a colour: an exact named-colour match, else hex.
QString displayName(const QColor& c);
/// Perceived luminance (0..1), used to pick contrasting edges/text.
double luminance(const QColor& c);
/// Black or white, whichever contrasts better with c.
QColor contrasting(const QColor& c);
/// Blend a over b with factor t (0 = b, 1 = a).
QColor mix(const QColor& a, const QColor& b, double t);

}  // namespace colors
}  // namespace cactus
