// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/color/Colors.h"

#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace cactus {

namespace {
struct NamedColorEntry {
    const char* name;
    quint32 rgb;
    const char* source;
};
struct PaletteEntry {
    const char* name;
    const char* group;
    int offset;
    int count;
};
struct ColormapEntry {
    const char* name;
    const char* category;
    const char* source;
    int offset;
};

#include "core/color/palettes_data.inc"

QColor fromRgb(quint32 v) { return QColor(int((v >> 16) & 0xFF), int((v >> 8) & 0xFF), int(v & 0xFF)); }

struct Library {
    QVector<NamedColor> grace;
    QVector<NamedColor> named;
    QVector<Palette> palettes;
    QVector<Colormap> colormaps;
    QHash<QString, int> nameIndex;  // lower-case name -> index into named (first/preferred wins)
    QHash<QRgb, int> rgbIndex;      // rgb -> preferred named entry
    QVector<QColor> cycle;

    Library()
    {
        const int n = int(sizeof(kNamedColors) / sizeof(kNamedColors[0]));
        named.reserve(n);
        for (int i = 0; i < n; ++i) {
            NamedColor c{QString::fromLatin1(kNamedColors[i].name), fromRgb(kNamedColors[i].rgb),
                         QString::fromLatin1(kNamedColors[i].source)};
            if (c.source == QLatin1String("Grace")) grace.push_back(c);
            const QString key = c.name.toLower();
            const QString prefixed = c.source.toLower() + QLatin1Char(':') + key;
            if (!nameIndex.contains(key)) nameIndex.insert(key, named.size());
            nameIndex.insert(prefixed, named.size());
            if (!rgbIndex.contains(c.color.rgb())) rgbIndex.insert(c.color.rgb(), named.size());
            named.push_back(c);
        }
        // Common aliases
        if (nameIndex.contains(QStringLiteral("grey"))) nameIndex.insert(QStringLiteral("gray"), nameIndex.value(QStringLiteral("css:gray"), nameIndex.value(QStringLiteral("grey"))));

        const int np = int(sizeof(kPalettes) / sizeof(kPalettes[0]));
        for (int i = 0; i < np; ++i) {
            Palette p;
            p.name = QString::fromLatin1(kPalettes[i].name);
            p.group = QString::fromLatin1(kPalettes[i].group);
            for (int k = 0; k < kPalettes[i].count; ++k) p.colors.push_back(fromRgb(kPaletteColors[kPalettes[i].offset + k]));
            palettes.push_back(p);
        }
        const int nc = int(sizeof(kColormaps) / sizeof(kColormaps[0]));
        for (int i = 0; i < nc; ++i) {
            Colormap m;
            m.name = QString::fromLatin1(kColormaps[i].name);
            m.category = QString::fromLatin1(kColormaps[i].category);
            m.source = QString::fromLatin1(kColormaps[i].source);
            m.samples.reserve(kColormapSamples);
            for (int k = 0; k < kColormapSamples; ++k) {
                const quint32 v = kColormapData[kColormaps[i].offset + k];
                m.samples.push_back(qRgb(int((v >> 16) & 0xFF), int((v >> 8) & 0xFF), int(v & 0xFF)));
            }
            colormaps.push_back(m);
        }
        for (const Palette& p : palettes) {
            if (p.name == QLatin1String("CactusPlot Modern")) cycle = p.colors;
        }
        if (cycle.isEmpty()) cycle = {QColor(0, 114, 178), QColor(213, 94, 0), QColor(0, 158, 115)};
    }
};

const Library& lib()
{
    static const Library l;
    return l;
}
}  // namespace

QColor Colormap::at(double t) const
{
    if (samples.isEmpty()) return QColor(Qt::black);
    if (!std::isfinite(t)) t = 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const double pos = t * (samples.size() - 1);
    const int i0 = int(std::floor(pos));
    const int i1 = std::min(i0 + 1, int(samples.size()) - 1);
    const double f = pos - i0;
    const QRgb a = samples[i0], b = samples[i1];
    auto lerp = [f](int x, int y) { return int(std::lround(x + (y - x) * f)); };
    return QColor(lerp(qRed(a), qRed(b)), lerp(qGreen(a), qGreen(b)), lerp(qBlue(a), qBlue(b)));
}

QColor Colormap::at(double t, double lo, double hi, bool reversed) const
{
    if (!std::isfinite(t)) t = 0.0;
    t = std::clamp(t, 0.0, 1.0);
    if (reversed) t = 1.0 - t;
    return at(lo + (hi - lo) * t);
}

namespace colors {

const QVector<NamedColor>& graceDefaults() { return lib().grace; }

QColor graceColor(int index)
{
    const auto& g = lib().grace;
    if (index >= 0 && index < g.size()) return g[index].color;
    return QColor(Qt::black);
}

const QVector<NamedColor>& namedColors() { return lib().named; }
const QVector<Palette>& palettes() { return lib().palettes; }

const Palette* palette(const QString& name)
{
    for (const Palette& p : lib().palettes) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) return &p;
    }
    return nullptr;
}

const QVector<Colormap>& colormaps() { return lib().colormaps; }

const Colormap* colormap(const QString& name)
{
    for (const Colormap& m : lib().colormaps) {
        if (m.name.compare(name, Qt::CaseInsensitive) == 0) return &m;
    }
    // allow names without the "cmo."/"cet." prefix
    for (const Colormap& m : lib().colormaps) {
        const int dot = m.name.indexOf(QLatin1Char('.'));
        if (dot > 0 && m.name.mid(dot + 1).compare(name, Qt::CaseInsensitive) == 0) return &m;
    }
    return nullptr;
}

const QVector<QColor>& defaultCycle() { return lib().cycle; }

bool parse(const QString& text, QColor* out)
{
    QString s = text.trimmed();
    if (s.isEmpty()) return false;
    if (s.startsWith(QLatin1Char('#'))) {
        QString h = s.mid(1);
        bool ok = false;
        if (h.size() == 3) {
            h = QString(h[0]) + h[0] + h[1] + h[1] + h[2] + h[2];
        }
        if (h.size() == 6) {
            const uint v = h.toUInt(&ok, 16);
            if (!ok) return false;
            *out = QColor(int((v >> 16) & 0xFF), int((v >> 8) & 0xFF), int(v & 0xFF));
            return true;
        }
        if (h.size() == 8) {
            const uint v = h.toUInt(&ok, 16);
            if (!ok) return false;
            *out = QColor(int((v >> 24) & 0xFF), int((v >> 16) & 0xFF), int((v >> 8) & 0xFF), int(v & 0xFF));
            return true;
        }
        return false;
    }
    static const QRegularExpression rgbRe(
        QStringLiteral("^rgba?\\(\\s*([0-9.]+)\\s*,\\s*([0-9.]+)\\s*,\\s*([0-9.]+)\\s*(?:,\\s*([0-9.]+)\\s*)?\\)$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = rgbRe.match(s);
    if (m.hasMatch()) {
        QColor c(int(m.captured(1).toDouble()), int(m.captured(2).toDouble()), int(m.captured(3).toDouble()));
        if (!m.captured(4).isEmpty()) {
            double a = m.captured(4).toDouble();
            c.setAlphaF(float(a > 1.0 ? a / 255.0 : a));
        }
        *out = c;
        return true;
    }
    const auto& l = lib();
    const QString key = s.toLower();
    auto it = l.nameIndex.constFind(key);
    if (it == l.nameIndex.constEnd()) {
        QString alt = key;
        alt.remove(QLatin1Char(' '));
        it = l.nameIndex.constFind(alt);
    }
    if (it != l.nameIndex.constEnd()) {
        *out = l.named[*it].color;
        return true;
    }
    return false;
}

QString toHex(const QColor& c)
{
    if (c.alpha() == 255) return c.name(QColor::HexRgb);
    return QStringLiteral("#%1%2%3%4")
        .arg(c.red(), 2, 16, QLatin1Char('0'))
        .arg(c.green(), 2, 16, QLatin1Char('0'))
        .arg(c.blue(), 2, 16, QLatin1Char('0'))
        .arg(c.alpha(), 2, 16, QLatin1Char('0'));
}

QString displayName(const QColor& c)
{
    if (c.alpha() == 255) {
        auto it = lib().rgbIndex.constFind(c.rgb());
        if (it != lib().rgbIndex.constEnd()) return lib().named[*it].name;
    }
    return toHex(c);
}

double luminance(const QColor& c)
{
    auto lin = [](double v) {
        v /= 255.0;
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.red()) + 0.7152 * lin(c.green()) + 0.0722 * lin(c.blue());
}

QColor contrasting(const QColor& c) { return luminance(c) > 0.35 ? QColor(Qt::black) : QColor(Qt::white); }

QColor mix(const QColor& a, const QColor& b, double t)
{
    auto l = [t](int x, int y) { return int(std::lround(y + (x - y) * t)); };
    return QColor(l(a.red(), b.red()), l(a.green(), b.green()), l(a.blue(), b.blue()), l(a.alpha(), b.alpha()));
}

}  // namespace colors
}  // namespace cactus
