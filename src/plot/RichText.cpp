// SPDX-License-Identifier: GPL-2.0-or-later
#include "plot/RichText.h"

#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QRegularExpression>
#include <cmath>

#include "core/color/Colors.h"
#include "plot/Fonts.h"

namespace cactus::richtext {

namespace {

const QHash<QString, QString>& texSymbols()
{
    static const QHash<QString, QString> m = [] {
        QHash<QString, QString> h;
        const char* greek[][2] = {
            {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"}, {"epsilon", "ϵ"},
            {"varepsilon", "ε"}, {"zeta", "ζ"}, {"eta", "η"}, {"theta", "θ"}, {"vartheta", "ϑ"},
            {"iota", "ι"}, {"kappa", "κ"}, {"lambda", "λ"}, {"mu", "μ"}, {"nu", "ν"},
            {"xi", "ξ"}, {"pi", "π"}, {"varpi", "ϖ"}, {"rho", "ρ"}, {"varrho", "ϱ"},
            {"sigma", "σ"}, {"varsigma", "ς"}, {"tau", "τ"}, {"upsilon", "υ"}, {"phi", "ϕ"},
            {"varphi", "φ"}, {"chi", "χ"}, {"psi", "ψ"}, {"omega", "ω"},
            {"Gamma", "Γ"}, {"Delta", "Δ"}, {"Theta", "Θ"}, {"Lambda", "Λ"}, {"Xi", "Ξ"},
            {"Pi", "Π"}, {"Sigma", "Σ"}, {"Upsilon", "Υ"}, {"Phi", "Φ"}, {"Psi", "Ψ"},
            {"Omega", "Ω"},
            {"pm", "±"}, {"mp", "∓"}, {"times", "×"}, {"div", "÷"}, {"cdot", "·"},
            {"ast", "∗"}, {"star", "⋆"}, {"circ", "∘"}, {"bullet", "•"},
            {"infty", "∞"}, {"partial", "∂"}, {"nabla", "∇"}, {"hbar", "ℏ"}, {"ell", "ℓ"},
            {"Re", "ℜ"}, {"Im", "ℑ"}, {"aleph", "ℵ"}, {"emptyset", "∅"},
            {"approx", "≈"}, {"sim", "∼"}, {"simeq", "≃"}, {"cong", "≅"}, {"equiv", "≡"},
            {"neq", "≠"}, {"ne", "≠"}, {"leq", "≤"}, {"le", "≤"}, {"geq", "≥"},
            {"ge", "≥"}, {"ll", "≪"}, {"gg", "≫"}, {"propto", "∝"}, {"perp", "⊥"},
            {"parallel", "∥"}, {"in", "∈"}, {"notin", "∉"}, {"subset", "⊂"}, {"supset", "⊃"},
            {"subseteq", "⊆"}, {"supseteq", "⊇"}, {"cup", "∪"}, {"cap", "∩"}, {"forall", "∀"},
            {"exists", "∃"}, {"neg", "¬"}, {"wedge", "∧"}, {"vee", "∨"},
            {"leftarrow", "←"}, {"rightarrow", "→"}, {"to", "→"}, {"uparrow", "↑"},
            {"downarrow", "↓"}, {"leftrightarrow", "↔"}, {"Leftarrow", "⇐"}, {"Rightarrow", "⇒"},
            {"Leftrightarrow", "⇔"}, {"rightleftharpoons", "⇌"}, {"mapsto", "↦"},
            {"sum", "∑"}, {"prod", "∏"}, {"int", "∫"}, {"oint", "∮"}, {"sqrt", "√"},
            {"degree", "°"}, {"deg", "°"}, {"angstrom", "Å"}, {"AA", "Å"}, {"prime", "′"},
            {"dagger", "†"}, {"ddagger", "‡"}, {"langle", "⟨"}, {"rangle", "⟩"},
            {"ldots", "…"}, {"cdots", "⋯"}, {"dots", "…"}, {"permil", "‰"}, {"micro", "µ"},
            {"odot", "⊙"}, {"oplus", "⊕"}, {"otimes", "⊗"}, {"quad", " "}, {",", " "},
            {";", " "}, {"!", ""}, {" ", " "}, {"%", "%"}, {"$", "$"}, {"{", "{"}, {"}", "}"}, {"_", "_"},
            {"&", "&"}, {"#", "#"}};
        for (const auto& g : greek) h.insert(QString::fromLatin1(g[0]), QString::fromUtf8(g[1]));
        const char* upright[] = {"sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan", "sinh",
                                 "cosh", "tanh", "log", "ln", "lg", "exp", "max", "min", "lim", "det", "dim",
                                 "sup", "inf", "arg", "deg_", "Pr", "gcd", "mod"};
        for (const char* u : upright) h.insert(QString::fromLatin1(u), QString::fromLatin1(u));
        return h;
    }();
    return m;
}

/// Convert the inside of a math group into Grace markup.
QString mathToGrace(const QString& s, int* pos, bool untilBrace);

QString mathAtom(const QString& s, int* pos)
{
    // a single atom for ^/_: a {group}, a \command or one character
    if (*pos >= s.size()) return QString();
    const QChar c = s[*pos];
    if (c == QLatin1Char('{')) {
        ++*pos;
        return mathToGrace(s, pos, true);
    }
    if (c == QLatin1Char('\\')) {
        int start = *pos;
        int e = *pos + 1;
        while (e < s.size() && s[e].isLetter()) ++e;
        if (e == *pos + 1 && e < s.size()) ++e;
        QString sub = s.mid(start, e - start);
        *pos = e;
        int p2 = 0;
        return mathToGrace(sub, &p2, false);
    }
    ++*pos;
    QString one = QString(c);
    int p2 = 0;
    return mathToGrace(one, &p2, false);
}

QString mathToGrace(const QString& s, int* pos, bool untilBrace)
{
    QString out;
    bool italicOn = false;
    auto setItalic = [&](bool on) {
        if (on != italicOn) {
            out += QStringLiteral("\\I");
            italicOn = on;
        }
    };
    while (*pos < s.size()) {
        const QChar c = s[*pos];
        if (untilBrace && c == QLatin1Char('}')) {
            ++*pos;
            break;
        }
        if (c == QLatin1Char('^') || c == QLatin1Char('_')) {
            setItalic(false);
            ++*pos;
            const QString atom = mathAtom(s, pos);
            const QString first = QStringLiteral("\\[") + (c == QLatin1Char('^') ? QStringLiteral("\\S") : QStringLiteral("\\s")) +
                                  atom + QStringLiteral("\\]");
            const QChar other = c == QLatin1Char('^') ? QLatin1Char('_') : QLatin1Char('^');
            if (*pos < s.size() && s[*pos] == other) {
                // stacked sub- and superscript: both start at the same position
                ++*pos;
                const QString atom2 = mathAtom(s, pos);
                const QString second = QStringLiteral("\\[") + (other == QLatin1Char('^') ? QStringLiteral("\\S") : QStringLiteral("\\s")) +
                                       atom2 + QStringLiteral("\\]");
                out += QStringLiteral("\\m{99}") + first + QStringLiteral("\\m{98}\\M{99}") + second + QStringLiteral("\\X{98}");
            } else {
                out += first;
            }
            continue;
        }
        if (c == QLatin1Char('{')) {
            setItalic(false);
            ++*pos;
            out += QStringLiteral("\\[") + mathToGrace(s, pos, true) + QStringLiteral("\\]");
            continue;
        }
        if (c == QLatin1Char('\\')) {
            setItalic(false);
            int e = *pos + 1;
            while (e < s.size() && s[e].isLetter()) ++e;
            if (e == *pos + 1 && e < s.size()) ++e;  // single non-letter command like \, or \{
            const QString cmd = s.mid(*pos + 1, e - *pos - 1);
            *pos = e;
            if (cmd == QLatin1String("frac")) {
                QString a, b;
                while (*pos < s.size() && s[*pos].isSpace()) ++*pos;
                a = mathAtom(s, pos);
                while (*pos < s.size() && s[*pos].isSpace()) ++*pos;
                b = mathAtom(s, pos);
                out += a + QStringLiteral("/") + b;
            } else if (cmd == QLatin1String("sqrt")) {
                while (*pos < s.size() && s[*pos].isSpace()) ++*pos;
                out += QString(QChar(0x221A)) + QStringLiteral("\\o") + mathAtom(s, pos) + QStringLiteral("\\O");
            } else if (cmd == QLatin1String("mathrm") || cmd == QLatin1String("text") || cmd == QLatin1String("rm") ||
                       cmd == QLatin1String("mathbf") || cmd == QLatin1String("mathit") || cmd == QLatin1String("operatorname")) {
                if (*pos < s.size() && s[*pos] == QLatin1Char('{')) {
                    ++*pos;
                    int depth = 1;
                    QString lit;
                    while (*pos < s.size()) {
                        const QChar d = s[*pos];
                        if (d == QLatin1Char('{')) ++depth;
                        if (d == QLatin1Char('}') && --depth == 0) { ++*pos; break; }
                        lit += d;
                        ++*pos;
                    }
                    if (cmd == QLatin1String("mathit")) out += QStringLiteral("\\I") + lit + QStringLiteral("\\I");
                    else out += lit;
                }
            } else if (cmd == QLatin1String("left") || cmd == QLatin1String("right") || cmd == QLatin1String("displaystyle")) {
                // ignore sizing commands
            } else {
                auto it = texSymbols().constFind(cmd);
                if (it != texSymbols().constEnd()) out += *it;
                else out += QStringLiteral("\\\\") + cmd;
            }
            continue;
        }
        if (c.isLetter() && c.unicode() < 128) {
            setItalic(true);
            out += c;
        } else {
            setItalic(false);
            if (c == QLatin1Char('-')) out += QChar(0x2212);
            else if (c == QLatin1Char('*')) out += QChar(0x2217);
            else out += c;
        }
        ++*pos;
    }
    setItalic(false);
    return out;
}

struct State {
    QString font;
    double scale = 1.0;
    double vshift = 0.0;
    QColor color;
    bool underline = false;
    bool overline = false;
    QTransform m;  // linear glyph transform (y down)
    bool upper = false;
    bool italic = false;
};

/// Grace matrix (y up) -> Qt linear transform (y down).
QTransform graceMatrix(double a, double b, double c, double d) { return QTransform(a, -c, -b, d, 0, 0); }

QString braceArg(const QString& s, int* i)
{
    // expects s[*i] == '{'
    if (*i >= s.size() || s[*i] != QLatin1Char('{')) return QString();
    int j = *i + 1;
    QString out;
    while (j < s.size() && s[j] != QLatin1Char('}')) out += s[j++];
    *i = (j < s.size()) ? j + 1 : j;
    return out;
}

}  // namespace

QString texToGrace(const QString& text)
{
    if (!text.contains(QLatin1Char('$'))) return text;
    QString out;
    int i = 0;
    while (i < text.size()) {
        const QChar c = text[i];
        if (c == QLatin1Char('\\') && i + 1 < text.size() && text[i + 1] == QLatin1Char('$')) {
            out += QLatin1Char('$');
            i += 2;
            continue;
        }
        if (c == QLatin1Char('$')) {
            const int end = text.indexOf(QLatin1Char('$'), i + 1);
            if (end < 0) {
                out += text.mid(i);
                break;
            }
            const QString math = text.mid(i + 1, end - i - 1);
            int p = 0;
            out += mathToGrace(math, &p, false);
            i = end + 1;
            continue;
        }
        out += c;
        ++i;
    }
    return out;
}

QString plainText(const QString& markup)
{
    const QString s = texToGrace(markup);
    QString out;
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] == QLatin1Char('\\') && i + 1 < s.size()) {
            const QChar e = s[i + 1];
            if (e == QLatin1Char('\\')) { out += e; ++i; continue; }
            i += 1;
            if (QStringLiteral("fRtzZrlvVhmMT#").contains(e) && i + 1 < s.size() && s[i + 1] == QLatin1Char('{')) {
                int k = i + 1;
                braceArg(s, &k);
                i = k - 1;
            } else if (e == QLatin1Char('d') || e == QLatin1Char('F')) {
                ++i;
            }
            continue;
        }
        out += s[i];
    }
    return out;
}

TextLayout layout(const QString& rawMarkup, const QString& baseFont, double sizeUnits, const QColor& color,
                  const Project* project, const QPaintDevice* device)
{
    TextLayout L;
    const QString s = texToGrace(rawMarkup);
    State st;
    st.font = baseFont;
    st.color = color;
    const State initial = st;
    QVector<State> stack;
    QHash<int, double> marks;

    double x = 0.0;
    double penY = 0.0;  // vertical pen drift caused by transformed runs
    double lineY = 0.0;
    const double lineSpacing = sizeUnits * 1.25;
    QString buf;

    auto fontNow = [&]() {
        QFont f = fonts::font(st.font, sizeUnits * st.scale, device);
        if (st.italic) f.setItalic(!f.italic());
        return f;
    };
    auto flush = [&]() {
        if (buf.isEmpty()) return;
        TextRunLayout r;
        r.text = buf;
        r.font = fontNow();
        r.color = st.color;
        r.underline = st.underline;
        r.overline = st.overline;
        r.xf = st.m;
        QFontMetricsF fm(r.font, const_cast<QPaintDevice*>(device));
        r.width = fm.horizontalAdvance(buf);
        r.ascent = fm.ascent();
        r.descent = fm.descent();
        r.pos = QPointF(x, lineY + penY - st.vshift);
        const QPointF adv = st.m.map(QPointF(r.width, 0.0));
        x += adv.x();
        penY += adv.y();
        L.runs.push_back(r);
        buf.clear();
    };
    auto addChar = [&](QChar ch) {
        if (st.upper && ch.unicode() < 128) ch = QChar(ushort(ch.unicode() + 128));
        if (fonts::isSymbolFont(st.font)) ch = fonts::symbolToUnicode(ch);
        else if (fonts::isDingbatsFont(st.font)) ch = fonts::dingbatToUnicode(ch);
        buf += ch;
    };

    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c == QLatin1Char('\n')) {
            flush();
            x = 0;
            lineY += lineSpacing;
            continue;
        }
        if (c != QLatin1Char('\\') || i + 1 >= s.size()) {
            addChar(c);
            continue;
        }
        const QChar e = s[++i];
        auto arg = [&]() {
            int k = i + 1;
            if (k < s.size() && s[k] == QLatin1Char('{')) {
                QString a = braceArg(s, &k);
                i = k - 1;
                return a;
            }
            return QString();
        };
        auto hasArg = [&]() { return i + 1 < s.size() && s[i + 1] == QLatin1Char('{'); };
        switch (e.unicode()) {
        case '\\': addChar(QLatin1Char('\\')); break;
        case 'n': flush(); x = 0; penY = 0; lineY += lineSpacing; break;
        case 'f': {
            flush();
            if (!hasArg()) break;
            const QString a = arg().trimmed();
            if (a.isEmpty()) st.font = initial.font;
            else {
                bool ok = false;
                const int n = a.toInt(&ok);
                st.font = ok ? (project ? project->fontAt(n) : fonts::standardFontNames().value(n, initial.font)) : a;
            }
            break;
        }
        case 'x': flush(); st.font = QStringLiteral("Symbol"); break;
        case 'R': {
            flush();
            const QString a = arg().trimmed();
            if (a.isEmpty()) st.color = initial.color;
            else {
                bool ok = false;
                const int n = a.toInt(&ok);
                QColor col;
                if (ok) col = project ? project->colorAt(n) : colors::graceColor(n);
                else if (!colors::parse(a, &col)) col = st.color;
                st.color = col;
            }
            break;
        }
        case '#': {
            const QString a = arg();
            for (int k = 0; k + 1 < a.size(); k += 2) {
                bool ok = false;
                const int code = a.mid(k, 2).toInt(&ok, 16);
                if (ok) addChar(QChar(ushort(code)));
            }
            break;
        }
        case 'z': {
            flush();
            const QString a = arg();
            if (a.trimmed().isEmpty()) st.scale = 1.0;
            else st.scale *= a.toDouble();
            break;
        }
        case 'Z': {
            flush();
            const QString a = arg();
            st.scale = a.trimmed().isEmpty() ? 1.0 : a.toDouble();
            break;
        }
        case '+': flush(); st.scale *= 1.19; break;
        case '-': flush(); st.scale *= 0.84; break;
        case 's': flush(); st.vshift -= 0.4 * sizeUnits * st.scale; st.scale *= 0.71; break;
        case 'S': flush(); st.vshift += 0.6 * sizeUnits * st.scale; st.scale *= 0.71; break;
        case 'v':
        case 'V': {
            flush();
            const QString a = arg();
            if (a.trimmed().isEmpty()) st.vshift = 0;
            else st.vshift += a.toDouble() * sizeUnits * st.scale;
            break;
        }
        case 'h': {
            flush();
            x += arg().toDouble() * sizeUnits * st.scale;
            break;
        }
        case 'N': flush(); st.vshift = 0; st.scale = 1.0; st.m = QTransform(); break;
        case 'u': flush(); st.underline = true; break;
        case 'U': flush(); st.underline = false; break;
        case 'o': flush(); st.overline = true; break;
        case 'O': flush(); st.overline = false; break;
        case 'q': flush(); st.m = graceMatrix(1, 0.25, 0, 1) * st.m; break;
        case 'Q': flush(); st.m = graceMatrix(1, -0.25, 0, 1) * st.m; break;
        case 'l': flush(); st.m = graceMatrix(1, arg().toDouble(), 0, 1) * st.m; break;
        case 'c': flush(); st.upper = true; break;
        case 'C': flush(); st.upper = false; break;
        case 'I': flush(); st.italic = !st.italic; break;  // CactusPlot private: italic toggle
        case '[': flush(); stack.push_back(st); break;     // CactusPlot private: push state
        case ']':
            flush();
            if (!stack.isEmpty()) {
                const State back = stack.takeLast();
                st = back;
            }
            break;
        case 'm': {
            flush();
            marks.insert(arg().toInt(), x);
            break;
        }
        case 'M': {
            flush();
            const int k = arg().toInt();
            if (marks.contains(k)) x = marks.value(k);
            break;
        }
        case 'X': {  // CactusPlot private: advance to the furthest of the current position and a mark
            flush();
            const int k = arg().toInt();
            if (marks.contains(k)) x = std::max(x, marks.value(k));
            break;
        }
        case 't':
        case 'T': {
            flush();
            const QString a = arg();
            if (e == QLatin1Char('T') || a.trimmed().isEmpty()) st.m = QTransform();
            const QStringList parts = a.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
            if (parts.size() == 4)
                st.m = graceMatrix(parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble(), parts[3].toDouble()) * st.m;
            break;
        }
        case 'r': {
            flush();
            const double ang = arg().toDouble() * M_PI / 180.0;
            st.m = graceMatrix(std::cos(ang), -std::sin(ang), std::sin(ang), std::cos(ang)) * st.m;
            break;
        }
        case 'd': ++i; break;             // direction control: ignored
        case 'F': ++i; break;             // kerning/ligature control: ignored
        default:
            if (e.isDigit()) {
                flush();
                const int n = e.digitValue();
                st.font = project ? project->fontAt(n) : fonts::standardFontNames().value(n, initial.font);
            } else {
                addChar(QLatin1Char('\\'));
                addChar(e);
            }
            break;
        }
    }
    flush();

    QRectF b;
    double w = 0.0;
    for (const TextRunLayout& r : L.runs) {
        QRectF rr = r.xf.mapRect(QRectF(0, -r.ascent, r.width, r.ascent + r.descent));
        rr.translate(r.pos);
        b = b.isNull() ? rr : b.united(rr);
        w = std::max(w, rr.right());
    }
    if (L.runs.isEmpty()) {
        QFontMetricsF fm(fonts::font(baseFont, sizeUnits, device), const_cast<QPaintDevice*>(device));
        b = QRectF(0, -fm.ascent(), 0, fm.ascent() + fm.descent());
    }
    L.bounds = b;
    L.width = w;
    return L;
}

namespace {
QPointF justOffset(const TextLayout& l, int hjust, int vjust)
{
    double dx = 0.0, dy = 0.0;
    if (hjust == 1) dx = -l.width;
    else if (hjust == 2) dx = -l.width / 2.0;
    switch (vjust) {
    case 1: dy = -l.bounds.bottom(); break;
    case 2: dy = -l.bounds.center().y(); break;
    case 3: dy = -l.bounds.top(); break;
    default: break;
    }
    return QPointF(dx, dy);
}
}  // namespace

void draw(QPainter* p, const TextLayout& l, const QPointF& anchor, double angleDeg, int hjust, int vjust)
{
    if (l.runs.isEmpty()) return;
    p->save();
    p->translate(anchor);
    if (angleDeg != 0.0) p->rotate(-angleDeg);
    p->translate(justOffset(l, hjust, vjust));
    for (const TextRunLayout& r : l.runs) {
        p->setFont(r.font);
        p->setPen(r.color);
        if (!r.xf.isIdentity()) {
            p->save();
            p->translate(r.pos);
            p->setTransform(r.xf, true);
            p->drawText(QPointF(0, 0), r.text);
            p->restore();
        } else {
            p->drawText(r.pos, r.text);
        }
        if (r.underline || r.overline) {
            QPen pen(r.color, std::max(0.05, r.ascent * 0.07));
            pen.setCapStyle(Qt::FlatCap);
            p->setPen(pen);
            if (r.underline) {
                const double y = r.pos.y() + r.descent * 0.45;
                p->drawLine(QPointF(r.pos.x(), y), QPointF(r.pos.x() + r.width, y));
            }
            if (r.overline) {
                const double y = r.pos.y() - r.ascent * 0.95;
                p->drawLine(QPointF(r.pos.x(), y), QPointF(r.pos.x() + r.width, y));
            }
        }
    }
    p->restore();
}

QRectF boundingRect(const TextLayout& l, const QPointF& anchor, double angleDeg, int hjust, int vjust)
{
    QTransform t;
    t.translate(anchor.x(), anchor.y());
    if (angleDeg != 0.0) t.rotate(-angleDeg);
    const QPointF o = justOffset(l, hjust, vjust);
    t.translate(o.x(), o.y());
    QRectF b = l.bounds;
    b.setLeft(std::min(b.left(), 0.0));
    b.setRight(std::max(b.right(), l.width));
    return t.mapRect(b);
}

QRectF drawText(QPainter* p, const QString& markup, const TextStyle& style, double sizeUnits, const Project* project,
                const QPointF& anchor, double angleDeg, int hjust, int vjust)
{
    const TextLayout l = layout(markup, style.font, sizeUnits, style.color, project, p->device());
    draw(p, l, anchor, angleDeg, hjust, vjust);
    return boundingRect(l, anchor, angleDeg, hjust, vjust);
}

}  // namespace cactus::richtext
