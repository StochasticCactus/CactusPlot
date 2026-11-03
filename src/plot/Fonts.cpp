// SPDX-License-Identifier: GPL-2.0-or-later
#include "plot/Fonts.h"

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QPaintDevice>

static void cactusInitFontResources() { Q_INIT_RESOURCE(fonts); }

namespace cactus::fonts {

namespace {
bool g_registered = false;
bool g_available = false;

struct Mapped {
    QString family;
    bool bold;
    bool italic;
};

Mapped mapName(const QString& name)
{
    const QString n = name.trimmed();
    const QString l = n.toLower();
    auto style = [&](const QString& suffix) {
        const QString s = suffix.toLower();
        return std::make_pair(s.contains(QLatin1String("bold")),
                              s.contains(QLatin1String("italic")) || s.contains(QLatin1String("oblique")));
    };
    struct Base {
        const char* prefix;
        const char* gyre;
        const char* fallback;
    };
    static const Base bases[] = {
        {"times", "TeX Gyre Termes", "Times New Roman"},
        {"helvetica", "TeX Gyre Heros", "Arial"},
        {"courier", "TeX Gyre Cursor", "Courier New"},
        {"symbol", "TeX Gyre Termes", "Times New Roman"},
        {"zapfdingbats", "DejaVu Sans", "Segoe UI Symbol"},
        {"nimbus roman", "TeX Gyre Termes", "Times New Roman"},
        {"nimbus sans", "TeX Gyre Heros", "Arial"},
        {"nimbus mono", "TeX Gyre Cursor", "Courier New"},
    };
    for (const Base& b : bases) {
        const QLatin1String pre(b.prefix);
        if (l.startsWith(pre)) {
            auto [bold, italic] = style(n.mid(pre.size()));
            const QString fam = (g_available || QString::fromLatin1(b.gyre) == QLatin1String("DejaVu Sans"))
                                    ? QString::fromLatin1(b.gyre)
                                    : QString::fromLatin1(b.fallback);
            return {fam, bold, italic};
        }
    }
    // "Family-Style" or "Family Bold Italic"
    QString fam = n;
    bool bold = false, italic = false;
    const int dash = n.lastIndexOf(QLatin1Char('-'));
    if (dash > 0) {
        auto [b, i] = style(n.mid(dash + 1));
        if (b || i || n.mid(dash + 1).compare(QLatin1String("Regular"), Qt::CaseInsensitive) == 0 ||
            n.mid(dash + 1).compare(QLatin1String("Roman"), Qt::CaseInsensitive) == 0) {
            fam = n.left(dash);
            bold = b;
            italic = i;
        }
    }
    for (const char* suf : {" Bold Italic", " Bold Oblique", " Bold", " Italic", " Oblique"}) {
        const QLatin1String s(suf);
        if (fam.endsWith(s, Qt::CaseInsensitive)) {
            auto [b, i] = style(s);
            bold = bold || b;
            italic = italic || i;
            fam.chop(s.size());
            break;
        }
    }
    return {fam, bold, italic};
}
}  // namespace

void registerBundledFonts()
{
    if (g_registered) return;
    g_registered = true;
    if (!QGuiApplication::instance()) return;
    cactusInitFontResources();
    QDir dir(QStringLiteral(":/fonts"));
    int ok = 0;
    for (const QString& f : dir.entryList(QStringList{QStringLiteral("*.otf"), QStringLiteral("*.ttf")})) {
        if (QFontDatabase::addApplicationFont(dir.filePath(f)) >= 0) ++ok;
    }
    g_available = ok > 0;
}

bool bundledFontsAvailable() { return g_available; }

const QStringList& standardFontNames()
{
    static const QStringList names{
        QStringLiteral("Times-Roman"),     QStringLiteral("Times-Italic"),          QStringLiteral("Times-Bold"),
        QStringLiteral("Times-BoldItalic"), QStringLiteral("Helvetica"),            QStringLiteral("Helvetica-Oblique"),
        QStringLiteral("Helvetica-Bold"),  QStringLiteral("Helvetica-BoldOblique"), QStringLiteral("Courier"),
        QStringLiteral("Courier-Oblique"), QStringLiteral("Courier-Bold"),          QStringLiteral("Courier-BoldOblique"),
        QStringLiteral("Symbol"),          QStringLiteral("ZapfDingbats")};
    return names;
}

QStringList availableFontNames()
{
    QStringList l = standardFontNames();
    if (QGuiApplication::instance()) {
        const QStringList fams = QFontDatabase::families();
        for (const QString& f : fams) {
            if (f.startsWith(QLatin1String("TeX Gyre"))) continue;
            l << f;
        }
    }
    return l;
}

QFont font(const QString& name, double sizeUnits, const QPaintDevice* device)
{
    const Mapped m = mapName(name.isEmpty() ? QStringLiteral("Helvetica") : name);
    QFont f(m.family);
    f.setBold(m.bold);
    f.setItalic(m.italic);
    f.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferOutline));
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setKerning(true);
    const double dpi = device ? device->logicalDpiY() : 72.0;
    f.setPointSizeF(std::max(0.01, sizeUnits * 72.0 / (dpi > 0 ? dpi : 72.0)));
    return f;
}

bool isSymbolFont(const QString& name) { return name.trimmed().compare(QLatin1String("Symbol"), Qt::CaseInsensitive) == 0; }
bool isDingbatsFont(const QString& name) { return name.trimmed().startsWith(QLatin1String("ZapfDingbats"), Qt::CaseInsensitive); }

QChar symbolToUnicode(QChar c)
{
    static const QHash<ushort, ushort> map = [] {
        QHash<ushort, ushort> m;
        const ushort upper[] = {0x0391, 0x0392, 0x03A7, 0x0394, 0x0395, 0x03A6, 0x0393, 0x0397, 0x0399, 0x03D1, 0x039A,
                                0x039B, 0x039C, 0x039D, 0x039F, 0x03A0, 0x0398, 0x03A1, 0x03A3, 0x03A4, 0x03A5, 0x03C2,
                                0x03A9, 0x039E, 0x03A8, 0x0396};
        const ushort lower[] = {0x03B1, 0x03B2, 0x03C7, 0x03B4, 0x03B5, 0x03C6, 0x03B3, 0x03B7, 0x03B9, 0x03D5, 0x03BA,
                                0x03BB, 0x03BC, 0x03BD, 0x03BF, 0x03C0, 0x03B8, 0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03D6,
                                0x03C9, 0x03BE, 0x03C8, 0x03B6};
        for (int i = 0; i < 26; ++i) {
            m.insert(ushort('A' + i), upper[i]);
            m.insert(ushort('a' + i), lower[i]);
        }
        const std::pair<ushort, ushort> misc[] = {
            {0x22, 0x2200}, {0x24, 0x2203}, {0x27, 0x220B}, {0x2A, 0x2217}, {0x2D, 0x2212}, {0x40, 0x2245},
            {0x5C, 0x2234}, {0x5E, 0x22A5}, {0x60, 0xF8E5}, {0x7E, 0x223C}, {0xA1, 0x03D2}, {0xA2, 0x2032},
            {0xA3, 0x2264}, {0xA4, 0x2044}, {0xA5, 0x221E}, {0xA6, 0x0192}, {0xA7, 0x2663}, {0xA8, 0x2666},
            {0xA9, 0x2665}, {0xAA, 0x2660}, {0xAB, 0x2194}, {0xAC, 0x2190}, {0xAD, 0x2191}, {0xAE, 0x2192},
            {0xAF, 0x2193}, {0xB0, 0x00B0}, {0xB1, 0x00B1}, {0xB2, 0x2033}, {0xB3, 0x2265}, {0xB4, 0x00D7},
            {0xB5, 0x221D}, {0xB6, 0x2202}, {0xB7, 0x2022}, {0xB8, 0x00F7}, {0xB9, 0x2260}, {0xBA, 0x2261},
            {0xBB, 0x2248}, {0xBC, 0x2026}, {0xBD, 0x23D0}, {0xBE, 0x23AF}, {0xBF, 0x21B5}, {0xC0, 0x2135},
            {0xC1, 0x2111}, {0xC2, 0x211C}, {0xC3, 0x2118}, {0xC4, 0x2297}, {0xC5, 0x2295}, {0xC6, 0x2205},
            {0xC7, 0x2229}, {0xC8, 0x222A}, {0xC9, 0x2283}, {0xCA, 0x2287}, {0xCB, 0x2284}, {0xCC, 0x2282},
            {0xCD, 0x2286}, {0xCE, 0x2208}, {0xCF, 0x2209}, {0xD0, 0x2220}, {0xD1, 0x2207}, {0xD2, 0x00AE},
            {0xD3, 0x00A9}, {0xD4, 0x2122}, {0xD5, 0x220F}, {0xD6, 0x221A}, {0xD7, 0x22C5}, {0xD8, 0x00AC},
            {0xD9, 0x2227}, {0xDA, 0x2228}, {0xDB, 0x21D4}, {0xDC, 0x21D0}, {0xDD, 0x21D1}, {0xDE, 0x21D2},
            {0xDF, 0x21D3}, {0xE0, 0x25CA}, {0xE1, 0x2329}, {0xE2, 0x00AE}, {0xE3, 0x00A9}, {0xE4, 0x2122},
            {0xE5, 0x2211}, {0xE6, 0x239B}, {0xE7, 0x239C}, {0xE8, 0x239D}, {0xE9, 0x23A1}, {0xEA, 0x23A2},
            {0xEB, 0x23A3}, {0xEC, 0x23A7}, {0xED, 0x23A8}, {0xEE, 0x23A9}, {0xEF, 0x23AA}, {0xF1, 0x232A},
            {0xF2, 0x222B}, {0xF3, 0x2320}, {0xF4, 0x23AE}, {0xF5, 0x2321}, {0xF6, 0x239E}, {0xF7, 0x239F},
            {0xF8, 0x23A0}, {0xF9, 0x23A4}, {0xFA, 0x23A5}, {0xFB, 0x23A6}, {0xFC, 0x23AB}, {0xFD, 0x23AC},
            {0xFE, 0x23AD}};
        for (const auto& p : misc) m.insert(p.first, p.second);
        return m;
    }();
    auto it = map.constFind(c.unicode());
    return it == map.constEnd() ? c : QChar(*it);
}

QChar dingbatToUnicode(QChar c)
{
    const ushort u = c.unicode();
    if (u == 0x20) return c;
    if (u >= 0x21 && u <= 0x7E) return QChar(ushort(0x2701 + (u - 0x21)));
    if (u >= 0xA1 && u <= 0xFE) return QChar(ushort(0x2761 + (u - 0xA1)));
    return c;
}

}  // namespace cactus::fonts
