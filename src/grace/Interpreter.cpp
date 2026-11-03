// SPDX-License-Identifier: GPL-2.0-or-later
#include "grace/Interpreter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <algorithm>
#include <cmath>

#include "core/Presets.h"
#include "core/color/Colors.h"
#include "grace/ProjectIO.h"
#include "io/DataReader.h"
#include "math/Transforms.h"

namespace cactus {

// ====================================================================== tokens

struct Interpreter::Tok {
    enum Type { Word, Number, String, Punct };
    Type type = Word;
    QString text;
    QString lower;
    int begin = 0, end = 0;
};

class Interpreter::Cursor {
public:
    Cursor(const QString& s, QVector<Tok> toks) : src(s), t(std::move(toks)) {}

    QString src;
    QVector<Tok> t;
    int i = 0;

    bool atEnd() const { return i >= t.size(); }
    const Tok* peek(int k = 0) const { return (i + k < t.size()) ? &t[i + k] : nullptr; }
    bool is(const char* w, int k = 0) const
    {
        const Tok* p = peek(k);
        return p && p->type == Tok::Word && p->lower == QLatin1String(w);
    }
    bool isPunct(const char* s, int k = 0) const
    {
        const Tok* p = peek(k);
        return p && p->type == Tok::Punct && p->text == QLatin1String(s);
    }
    bool isString(int k = 0) const
    {
        const Tok* p = peek(k);
        return p && p->type == Tok::String;
    }
    bool isNumberLike(int k = 0) const
    {
        const Tok* p = peek(k);
        return p && (p->type == Tok::Number || (p->type == Tok::Punct && (p->text == QLatin1String("-") || p->text == QLatin1String("+") || p->text == QLatin1String("("))));
    }
    bool accept(const char* w)
    {
        if (is(w)) { ++i; return true; }
        return false;
    }
    bool acceptPunct(const char* s)
    {
        if (isPunct(s)) { ++i; return true; }
        return false;
    }
    QString word() const { return (peek() && peek()->type == Tok::Word) ? peek()->lower : QString(); }
    QString rest() const { return atEnd() ? QString() : src.mid(t[i].begin).trimmed(); }
    /// Raw text of tokens until a top-level ',' (consumed) or the end.
    QString takeArg()
    {
        if (atEnd()) return QString();
        const int start = t[i].begin;
        int end = start;
        int depth = 0;
        bool prevOperandEnd = false;
        bool prevWord = false;
        while (i < t.size()) {
            const Tok& k = t[i];
            // two operands next to each other ("612 792") separate arguments
            const bool operandStart = k.type == Tok::Number || k.type == Tok::Word || k.type == Tok::String ||
                                      (k.type == Tok::Punct && (k.text == QLatin1String("(")));
            if (depth == 0 && prevOperandEnd && operandStart && !(prevWord && k.type == Tok::Punct)) break;
            prevOperandEnd = k.type == Tok::Number || k.type == Tok::Word || k.type == Tok::String ||
                             (k.type == Tok::Punct && (k.text == QLatin1String(")") || k.text == QLatin1String("]")));
            prevWord = k.type == Tok::Word;
            if (k.type == Tok::Punct) {
                if (k.text == QLatin1String("(") || k.text == QLatin1String("[")) ++depth;
                else if (k.text == QLatin1String(")") || k.text == QLatin1String("]")) --depth;
                else if (k.text == QLatin1String(",") && depth <= 0) {
                    ++i;
                    break;
                }
            }
            end = k.end;
            ++i;
        }
        return src.mid(start, end - start).trimmed();
    }
    bool hasTopLevelComma() const
    {
        int depth = 0;
        for (int k = i; k < t.size(); ++k) {
            if (t[k].type != Tok::Punct) continue;
            if (t[k].text == QLatin1String("(") || t[k].text == QLatin1String("[")) ++depth;
            else if (t[k].text == QLatin1String(")") || t[k].text == QLatin1String("]")) --depth;
            else if (t[k].text == QLatin1String(",") && depth == 0) return true;
        }
        return false;
    }
};

namespace {

bool isWordStart(QChar c) { return c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$'); }

QStringList splitStatements(const QString& line)
{
    QStringList out;
    QString cur;
    bool inStr = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == QLatin1Char('"') && (i == 0 || line[i - 1] != QLatin1Char('\\'))) inStr = !inStr;
        if (c == QLatin1Char(';') && !inStr) {
            out << cur;
            cur.clear();
            continue;
        }
        cur += c;
    }
    out << cur;
    return out;
}

/// Find '=' used as assignment (not ==, <=, >=, !=) at top level, outside strings.
int assignmentPos(const QString& s)
{
    bool inStr = false;
    int depth = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c == QLatin1Char('"')) inStr = !inStr;
        if (inStr) continue;
        if (c == QLatin1Char('(') || c == QLatin1Char('[')) ++depth;
        if (c == QLatin1Char(')') || c == QLatin1Char(']')) --depth;
        if (c == QLatin1Char('=') && depth == 0) {
            const QChar prev = i > 0 ? s[i - 1] : QChar();
            const QChar next = i + 1 < s.size() ? s[i + 1] : QChar();
            if (next == QLatin1Char('=') || prev == QLatin1Char('=') || prev == QLatin1Char('<') || prev == QLatin1Char('>') ||
                prev == QLatin1Char('!'))
                continue;
            return i;
        }
    }
    return -1;
}

}  // namespace

static QVector<Interpreter::Tok> tokenize(const QString& s)
{
    QVector<Interpreter::Tok> out;
    int i = 0;
    const int n = s.size();
    while (i < n) {
        const QChar c = s[i];
        if (c.isSpace()) { ++i; continue; }
        Interpreter::Tok t;
        t.begin = i;
        if (c == QLatin1Char('"')) {
            ++i;
            QString str;
            while (i < n && s[i] != QLatin1Char('"')) {
                if (s[i] == QLatin1Char('\\') && i + 1 < n && s[i + 1] == QLatin1Char('"')) {
                    str += QLatin1Char('"');
                    i += 2;
                    continue;
                }
                str += s[i++];
            }
            ++i;
            t.type = Interpreter::Tok::String;
            t.text = str;
        } else if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && s[i + 1].isDigit())) {
            int j = i;
            while (j < n && (s[j].isDigit() || s[j] == QLatin1Char('.'))) ++j;
            if (j < n && (s[j] == QLatin1Char('e') || s[j] == QLatin1Char('E'))) {
                int k = j + 1;
                if (k < n && (s[k] == QLatin1Char('+') || s[k] == QLatin1Char('-'))) ++k;
                if (k < n && s[k].isDigit()) {
                    j = k;
                    while (j < n && s[j].isDigit()) ++j;
                }
            }
            t.type = Interpreter::Tok::Number;
            t.text = s.mid(i, j - i);
            i = j;
        } else if (isWordStart(c)) {
            int j = i;
            while (j < n && (s[j].isLetterOrNumber() || s[j] == QLatin1Char('_') || s[j] == QLatin1Char('$') ||
                             (s[j] == QLatin1Char('.') && j + 1 < n && isWordStart(s[j + 1]))))
                ++j;
            t.type = Interpreter::Tok::Word;
            t.text = s.mid(i, j - i);
            i = j;
        } else {
            static const char* two[] = {"==", "!=", "<=", ">=", "&&", "||", "**"};
            QString p(c);
            for (const char* tw : two) {
                if (s.mid(i, 2) == QLatin1String(tw)) { p = QLatin1String(tw); break; }
            }
            i += p.size();
            t.type = Interpreter::Tok::Punct;
            t.text = p;
        }
        t.end = i;
        t.lower = t.text.toLower();
        out.push_back(t);
    }
    return out;
}

// ====================================================================== basics

Interpreter::Interpreter(Project* project) : m_p(project)
{
    m_fit.formula = QStringLiteral("y = a0 + a1*x");
    m_fit.nparams = 2;
}

LineType Interpreter::lineTypeFromGrace(int v)
{
    switch (v) {
    case 0: return LineType::None;
    case 1: return LineType::Straight;
    case 2: return LineType::LeftStairs;
    case 3: return LineType::RightStairs;
    case 4: return LineType::Segments;
    case 5: return LineType::ThreeSegments;
    case 6: return LineType::CenterStairs;
    default: return LineType::Straight;
    }
}

int Interpreter::lineTypeToGrace(LineType t)
{
    switch (t) {
    case LineType::None: return 0;
    case LineType::Straight: return 1;
    case LineType::LeftStairs: return 2;
    case LineType::RightStairs: return 3;
    case LineType::Segments: return 4;
    case LineType::ThreeSegments: return 5;
    case LineType::CenterStairs: return 6;
    }
    return 1;
}

void Interpreter::error(const QString& msg)
{
    const QString m = m_source.isEmpty() ? msg : QStringLiteral("%1: %2").arg(m_source, msg);
    m_errors << m;
    if (messageHandler) messageHandler(QStringLiteral("Error: ") + m);
}

void Interpreter::warn(const QString& msg)
{
    m_warnings << msg;
}

void Interpreter::info(const QString& msg)
{
    if (messageHandler) messageHandler(msg);
}

Graph& Interpreter::graph()
{
    const bool existed = m_p->graph(m_curGraph) != nullptr;
    Graph& g = m_p->ensureGraph(m_curGraph);
    if (!existed) {
        if (m_curGraph > 0 && m_p->graph(0)) {
            Graph tmpl = *m_p->graph(0);
            tmpl.sets.clear();
            tmpl.title.text.clear();
            tmpl.subtitle.text.clear();
            g = tmpl;
            g.active = true;
        } else {
            presets::styleGraph(g, m_p->stylePreset);
        }
    }
    return g;
}

// ====================================================================== running

bool Interpreter::runFile(const QString& path)
{
    QString full = path;
    if (QFileInfo(path).isRelative() && !workingDir.isEmpty()) full = QDir(workingDir).filePath(path);
    QString text, err;
    if (!datareader::readFile(full, &text, &err)) {
        error(err);
        return false;
    }
    return run(text, QFileInfo(full).fileName());
}

bool Interpreter::run(const QString& text, const QString& sourceName)
{
    if (++m_depth > 16) {
        --m_depth;
        error(QStringLiteral("scripts nested too deeply"));
        return false;
    }
    const QString oldSource = m_source;
    m_source = sourceName;
    const int errorsBefore = m_errors.size();
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith(QLatin1Char('#'))) continue;
        if (t.startsWith(QLatin1Char('&'))) {
            finishBlock();
            continue;
        }
        if (t.startsWith(QLatin1Char('@'))) {
            execute(t.mid(1));
            continue;
        }
        if (datareader::looksLikeData(t)) {
            addDataRow(t);
            continue;
        }
        execute(t);
    }
    finishBlock();
    if (m_haveObject) {
        // unterminated object definitions are kept (Grace semantics)
        m_p->objects.push_back(m_object);
        m_haveObject = false;
    }
    m_source = oldSource;
    --m_depth;
    return m_errors.size() == errorsBefore;
}

bool Interpreter::execute(const QString& line)
{
    bool ok = true;
    for (const QString& st : splitStatements(line)) {
        const QString s = st.trimmed();
        if (s.isEmpty() || s.startsWith(QLatin1Char('#'))) continue;
        if (!statement(s)) ok = false;
    }
    return ok;
}

void Interpreter::addDataRow(const QString& line)
{
    QVector<double> v;
    QString label;
    if (!datareader::parseLine(line, &v, &label, m_p->referenceDate)) return;
    m_rows.push_back(v);
    m_rowStrings.push_back(label);
}

void Interpreter::finishBlock()
{
    if (m_rows.isEmpty()) return;
    int gi = m_curGraph, si = -1;
    SetType type = m_blockType;
    if (m_haveTarget) {
        gi = m_targetGraph;
        si = m_targetSet;
    } else {
        type = m_nextSetType;
    }
    const int saved = m_curGraph;
    m_curGraph = gi;
    Graph& g = graph();
    m_curGraph = saved;
    if (si < 0) si = g.nextEmptySet();
    const bool existed = g.set(si) != nullptr;
    DataSet& s = g.ensureSet(si);
    if (!existed) presets::styleSet(s, si, m_p->stylePreset);
    const int ncols = setTypeColumns(type);
    s.type = type;
    for (auto& c : s.cols) c.clear();
    s.strings.clear();
    bool anyString = false;
    for (int r = 0; r < m_rows.size(); ++r) {
        const auto& row = m_rows[r];
        for (int c = 0; c < ncols; ++c) s.cols[c].push_back(c < row.size() ? row[c] : 0.0);
        s.strings.push_back(m_rowStrings.value(r));
        if (!m_rowStrings.value(r).isEmpty()) anyString = true;
    }
    if (!anyString) s.strings.clear();
    // a single column of numbers: use the index as x
    if (!m_haveTarget && type == SetType::XY) {
        bool oneCol = true;
        for (const auto& row : m_rows) if (row.size() > 1) { oneCol = false; break; }
        if (oneCol) {
            s.cols[1] = s.cols[0];
            for (int i = 0; i < s.cols[0].size(); ++i) s.cols[0][i] = i;
        }
    }
    m_lastSet = si;
    m_rows.clear();
    m_rowStrings.clear();
    m_haveTarget = false;
    m_blockType = SetType::XY;
}

// ====================================================================== helpers

double Interpreter::number(Cursor& c, bool* ok)
{
    const QString a = c.takeArg();
    expr::Value v;
    QString err;
    if (a.isEmpty() || !evaluate(a, &v, &err)) {
        if (ok) *ok = false;
        if (!a.isEmpty()) error(QStringLiteral("bad number '%1': %2").arg(a, err));
        else error(QStringLiteral("missing number"));
        return 0.0;
    }
    if (ok) *ok = true;
    return v.scalar();
}

QString Interpreter::string(Cursor& c, bool* ok)
{
    if (c.isString()) {
        const QString s = c.peek()->text;
        ++c.i;
        c.acceptPunct(",");
        if (ok) *ok = true;
        return s;
    }
    const QString a = c.takeArg();
    if (ok) *ok = !a.isEmpty();
    return a;
}

bool Interpreter::parseOnOff(Cursor& c, bool* out)
{
    const QString w = c.word();
    if (w == QLatin1String("on") || w == QLatin1String("true") || w == QLatin1String("yes")) { *out = true; ++c.i; return true; }
    if (w == QLatin1String("off") || w == QLatin1String("false") || w == QLatin1String("no")) { *out = false; ++c.i; return true; }
    return false;
}

bool Interpreter::parseColor(Cursor& c, QColor* out)
{
    if (c.isString()) {
        const QString name = c.peek()->text;
        ++c.i;
        for (const ColorMapEntry& e : m_p->colorMap) {
            if (e.name.compare(name, Qt::CaseInsensitive) == 0) { *out = e.color; return true; }
        }
        if (colors::parse(name, out)) return true;
        error(QStringLiteral("unknown colour '%1'").arg(name));
        return false;
    }
    if (c.is("rgb") || c.is("rgba") || c.isPunct("(")) {
        c.accept("rgb");
        c.accept("rgba");
        if (!c.acceptPunct("(")) return false;
        const QString inner = c.rest();
        const int close = inner.indexOf(QLatin1Char(')'));
        const QStringList parts = inner.left(close).split(QLatin1Char(','));
        while (!c.atEnd() && !c.isPunct(")")) ++c.i;
        c.acceptPunct(")");
        if (parts.size() < 3) return false;
        QColor col(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
        if (parts.size() > 3) col.setAlpha(parts[3].trimmed().toInt());
        *out = col;
        return true;
    }
    bool ok = false;
    const double v = number(c, &ok);
    if (!ok) return false;
    *out = m_p->colorAt(int(v));
    return true;
}

bool Interpreter::parseFont(Cursor& c, QString* out)
{
    if (c.isString()) {
        *out = c.peek()->text;
        ++c.i;
        return true;
    }
    bool ok = false;
    const double v = number(c, &ok);
    if (!ok) return false;
    *out = m_p->fontAt(int(v));
    return true;
}

bool Interpreter::resolveGraph(const QString& ref, int* graph)
{
    static const QRegularExpression re(QStringLiteral("^g(\\d+)$"), QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(ref);
    if (m.hasMatch()) {
        *graph = m.captured(1).toInt();
        return true;
    }
    return false;
}

bool Interpreter::resolveSet(const QString& ref, int* graph, int* set, bool create)
{
    static const QRegularExpression re(QStringLiteral("^(?:g(\\d+)\\.)?s(\\d+|_|\\$)$"), QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(ref.trimmed());
    if (!m.hasMatch()) return false;
    *graph = m.captured(1).isEmpty() ? m_curGraph : m.captured(1).toInt();
    const QString s = m.captured(2);
    if (s == QLatin1String("_") || s == QLatin1String("$")) *set = m_lastSet;
    else *set = s.toInt();
    if (*set < 0) return false;
    if (create) {
        const int saved = m_curGraph;
        m_curGraph = *graph;
        Graph& g = this->graph();
        m_curGraph = saved;
        const bool existed = g.set(*set) != nullptr;
        DataSet& ds = g.ensureSet(*set);
        if (!existed) presets::styleSet(ds, *set, m_p->stylePreset);
    }
    return true;
}

DataSet* Interpreter::setRef(const QString& ref, int* gi, int* si)
{
    int g, s;
    if (!resolveSet(ref, &g, &s)) return nullptr;
    if (gi) *gi = g;
    if (si) *si = s;
    Graph* gr = m_p->graph(g);
    return gr ? gr->set(s) : nullptr;
}

bool Interpreter::resolveIdent(const QString& name, expr::Value* out, int graph, int set) const
{
    static const QRegularExpression colRe(QStringLiteral("^(?:(?:g(\\d+)\\.)?s(\\d+|_|\\$)\\.)?(x|y|y1|y2|y3|y4|y0)$"));
    const auto m = colRe.match(name);
    if (m.hasMatch()) {
        int gi = m.captured(1).isEmpty() ? (graph >= 0 ? graph : m_curGraph) : m.captured(1).toInt();
        int si;
        if (m.captured(2).isEmpty()) si = set;
        else if (m.captured(2) == QLatin1String("_") || m.captured(2) == QLatin1String("$")) si = m_lastSet;
        else si = m.captured(2).toInt();
        const Graph* g = m_p->graph(gi);
        const DataSet* s = g ? g->set(si) : nullptr;
        if (!s) return false;
        const QString col = m.captured(3);
        int ci = 0;
        if (col == QLatin1String("x")) ci = 0;
        else if (col == QLatin1String("y") || col == QLatin1String("y0")) ci = 1;
        else ci = col.mid(1).toInt() + 1;
        *out = expr::Value(s->cols[ci].isEmpty() ? QVector<double>(s->size(), 0.0) : s->cols[ci]);
        return true;
    }
    if (name == QLatin1String("i") || name == QLatin1String("index") || name == QLatin1String("$i")) {
        const Graph* g = m_p->graph(graph >= 0 ? graph : m_curGraph);
        const DataSet* s = g ? g->set(set) : nullptr;
        if (!s) return false;
        QVector<double> v(s->size());
        for (int k = 0; k < v.size(); ++k) v[k] = k;
        *out = expr::Value(v);
        return true;
    }
    return false;
}

bool Interpreter::evaluate(const QString& text, expr::Value* out, QString* error, int graph, int set)
{
    expr::Context ctx = m_vars;
    const int gi = graph >= 0 ? graph : m_curGraph;
    const int si = set >= 0 ? set : (m_lastSet >= 0 ? m_lastSet : 0);
    ctx.setResolver([this, gi, si](const QString& n, expr::Value* v) { return resolveIdent(n, v, gi, si); });
    return expr::evaluate(text, ctx, out, error);
}

int Interpreter::newSet(int gi, const QVector<double>& x, const QVector<double>& y, const QString& comment)
{
    const int saved = m_curGraph;
    m_curGraph = gi;
    Graph& g = graph();
    m_curGraph = saved;
    const int si = g.nextFreeSet();
    DataSet& s = g.ensureSet(si);
    presets::styleSet(s, si, m_p->stylePreset);
    s.type = SetType::XY;
    s.cols[0] = x;
    s.cols[1] = y;
    s.comment = comment;
    m_lastSet = si;
    return si;
}

void Interpreter::applyAutoscaleOnRead(int gi)
{
    if (autoscaleOnRead == 0) return;
    if (Graph* g = m_p->graph(gi)) g->autoscale(autoscaleOnRead);
}

bool Interpreter::assign(const QString& lhsRaw, const QString& rhs)
{
    const QString lhs = lhsRaw.trimmed().toLower();
    // set column assignment
    static const QRegularExpression colRe(QStringLiteral("^(?:(?:g(\\d+)\\.)?s(\\d+|_|\\$)\\.)?(x|y|y0|y1|y2|y3|y4)(?:\\[(.+)\\])?$"));
    const auto m = colRe.match(lhs);
    if (m.hasMatch()) {
        const int gi = m.captured(1).isEmpty() ? m_curGraph : m.captured(1).toInt();
        int si;
        if (m.captured(2).isEmpty()) si = m_lastSet >= 0 ? m_lastSet : 0;
        else if (m.captured(2) == QLatin1String("_") || m.captured(2) == QLatin1String("$")) si = m_lastSet;
        else si = m.captured(2).toInt();
        int dummyG, dummyS;
        Q_UNUSED(dummyG);
        Q_UNUSED(dummyS);
        expr::Value v;
        QString err;
        if (!evaluate(rhs, &v, &err, gi, si)) {
            error(QStringLiteral("cannot evaluate '%1': %2").arg(rhs, err));
            return false;
        }
        const int saved = m_curGraph;
        m_curGraph = gi;
        Graph& g = graph();
        m_curGraph = saved;
        const bool existed = g.set(si) != nullptr;
        DataSet& s = g.ensureSet(si);
        if (!existed) presets::styleSet(s, si, m_p->stylePreset);
        const QString col = m.captured(3);
        int ci = col == QLatin1String("x") ? 0 : ((col == QLatin1String("y") || col == QLatin1String("y0")) ? 1 : col.mid(1).toInt() + 1);
        if (ci >= s.columns()) {
            // promote the set type so that the column exists
            static const SetType promote[] = {SetType::XY, SetType::XY, SetType::XYDY, SetType::XYDYDY, SetType::XYHILO, SetType::XYHILO};
            s.setType(promote[ci]);
        }
        if (!m.captured(4).isEmpty()) {
            expr::Value idx;
            if (!evaluate(m.captured(4), &idx, &err, gi, si)) {
                error(err);
                return false;
            }
            const int k = int(idx.scalar());
            if (k < 0) return false;
            if (k >= s.size()) s.resize(k + 1);
            s.cols[ci][k] = v.scalar();
            return true;
        }
        int n = s.size();
        if (v.isVector && (n == 0 || !existed)) {
            n = v.size();
            s.resize(n);
        }
        if (v.isVector && v.size() != n) {
            if (n == 0) { n = v.size(); s.resize(n); }
            else if (v.size() > n) s.resize(v.size()), n = v.size();
        }
        QVector<double> data = v.toVector(n);
        data.resize(n);
        s.cols[ci] = data;
        m_lastSet = si;
        return true;
    }
    // vector element assignment v[i] = ...
    static const QRegularExpression elemRe(QStringLiteral("^([a-z_$][a-z0-9_$]*)\\[(.+)\\]$"));
    const auto em = elemRe.match(lhs);
    expr::Value v;
    QString err;
    if (!evaluate(rhs, &v, &err)) {
        error(QStringLiteral("cannot evaluate '%1': %2").arg(rhs, err));
        return false;
    }
    if (em.hasMatch()) {
        expr::Value idx;
        if (!evaluate(em.captured(2), &idx, &err)) { error(err); return false; }
        auto& vars = m_vars.variables();
        expr::Value& target = vars[em.captured(1)];
        if (!target.isVector) target = expr::Value(QVector<double>());
        const int k = int(idx.scalar());
        if (k < 0) return false;
        if (k >= target.v.size()) target.v.resize(k + 1);
        target.v[k] = v.scalar();
        return true;
    }
    // fit parameter bounds: a0min / a0max
    static const QRegularExpression parRe(QStringLiteral("^a(\\d)(min|max)?$"));
    const auto pm = parRe.match(lhs);
    if (pm.hasMatch()) {
        const int k = pm.captured(1).toInt();
        if (pm.captured(2).isEmpty()) m_fit.params[k].value = v.scalar();
        else if (pm.captured(2) == QLatin1String("min")) m_fit.params[k].lo = v.scalar();
        else m_fit.params[k].hi = v.scalar();
    }
    m_vars.variables().insert(lhs, v);
    return true;
}

// ====================================================================== statements

bool Interpreter::statement(const QString& stmt)
{
    // assignments: "s0.y = s0.y * 2", "a0 = 1", "x = mesh(0, 1, 100)"
    const int eq = assignmentPos(stmt);
    if (eq > 0) {
        const QString lhs = stmt.left(eq).trimmed();
        static const QRegularExpression lv(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$.]*(\\[.+\\])?$"));
        if (lv.match(lhs).hasMatch()) return assign(lhs, stmt.mid(eq + 1));
    }
    Cursor c(stmt, tokenize(stmt));
    if (c.atEnd()) return true;
    const int errs = m_errors.size();
    const bool handled = dispatch(c);
    if (!handled) {
        warn(QStringLiteral("unsupported command ignored: %1").arg(stmt));
        return false;
    }
    return m_errors.size() == errs;
}

bool Interpreter::dispatch(Cursor& c)
{
    const QString w = c.word();
    if (w.isEmpty()) return false;
    Project& P = *m_p;

    // ---------------------------------------------------------- function-like commands: NAME ( args )
    if (c.isPunct("(", 1)) {
        const QString name = w;
        c.i += 2;
        QStringList args;
        int depth = 0;
        QString cur;
        const int start = c.atEnd() ? c.src.size() : c.t[c.i].begin;
        int pos = start;
        for (; pos < c.src.size(); ++pos) {
            const QChar ch = c.src[pos];
            if (ch == QLatin1Char('(')) ++depth;
            if (ch == QLatin1Char(')')) {
                if (depth == 0) break;
                --depth;
            }
            if (ch == QLatin1Char(',') && depth == 0) {
                args << cur.trimmed();
                cur.clear();
                continue;
            }
            cur += ch;
        }
        if (!cur.trimmed().isEmpty() || !args.isEmpty()) args << cur.trimmed();
        if (functionCommand(name, args)) return true;
        // fall back to evaluating an expression (e.g. "sin(1)" typed in the console)
        expr::Value v;
        QString err;
        if (evaluate(c.src, &v, &err)) {
            info(v.isVector ? QStringLiteral("[%1 values]").arg(v.size()) : QString::number(v.s, 'g', 15));
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------- file-level settings
    if (c.accept("version")) {
        const int v = int(number(c));
        if (v > 0) m_version = v;
        if (v > 0 && v < 50001) {
            // xmgr / Grace 5.0.0 font numbering (before font maps were written to files)
            P.fontMap = QStringList{QStringLiteral("Times-Roman"),     QStringLiteral("Times-Bold"),
                                    QStringLiteral("Times-Italic"),    QStringLiteral("Times-BoldItalic"),
                                    QStringLiteral("Helvetica"),       QStringLiteral("Helvetica-Bold"),
                                    QStringLiteral("Helvetica-Oblique"), QStringLiteral("Helvetica-BoldOblique"),
                                    QStringLiteral("Symbol"),          QStringLiteral("ZapfDingbats"),
                                    QStringLiteral("Courier"),         QStringLiteral("Courier-Bold"),
                                    QStringLiteral("Courier-Oblique"), QStringLiteral("Courier-BoldOblique")};
        }
        return true;
    }
    if (w == QLatin1String("page")) { return pageProps(c); }
    if (c.accept("link")) { return true; }  // "link page off", "link r0 to g0" handled below
    if (w == QLatin1String("map")) {
        ++c.i;
        if (c.accept("font")) {
            const int idx = c.peek() ? c.peek()->text.toInt() : 0;
            ++c.i;
            c.accept("to");
            const QString name = string(c);
            while (P.fontMap.size() <= idx) P.fontMap << QStringLiteral("Helvetica");
            P.fontMap[idx] = name;
            return true;
        }
        if (c.accept("color") || c.accept("colour")) {
            const int idx = c.peek() ? c.peek()->text.toInt() : 0;
            ++c.i;
            c.accept("to");
            if (!c.acceptPunct("(")) { error(QStringLiteral("expected (r, g, b)")); return true; }
            int comps[4] = {0, 0, 0, 255};
            int k = 0;
            while (!c.atEnd() && !c.isPunct(")") && k < 4) {
                if (c.isPunct(",")) { ++c.i; continue; }
                comps[k++] = c.peek()->text.toInt();
                ++c.i;
            }
            c.acceptPunct(")");
            c.acceptPunct(",");
            QString name = c.isString() ? c.peek()->text : QString();
            while (P.colorMap.size() <= idx) P.colorMap.push_back(ColorMapEntry{QColor(Qt::black), QString()});
            P.colorMap[idx] = ColorMapEntry{QColor(comps[0], comps[1], comps[2], comps[3]), name};
            return true;
        }
        return false;
    }
    if (c.accept("default")) {
        // defaults affect objects created later; most are informational in project files
        if (c.accept("sformat")) { string(c); return true; }
        c.i = c.t.size();
        return true;
    }
    if (c.accept("background")) {
        c.accept("color");
        QColor col;
        if (parseColor(c, &col)) P.page.background.color = col;
        return true;
    }
    if (c.accept("cactusplot")) {
        if (c.accept("preset")) { P.stylePreset = string(c); return true; }
        if (c.accept("pretty")) { parseOnOff(c, &P.prettyNumbers); return true; }
        if (c.accept("page")) {
            if (c.accept("gradient")) {
                if (c.accept("color")) { parseColor(c, &P.page.background.color2); return true; }
                const QString t = c.word();
                ++c.i;
                P.page.background.gradient = t == QLatin1String("vertical") ? GradientType::Vertical
                                           : t == QLatin1String("horizontal") ? GradientType::Horizontal
                                           : t == QLatin1String("radial") ? GradientType::Radial : GradientType::None;
                return true;
            }
        }
        c.i = c.t.size();
        return true;
    }
    if (c.accept("description")) { P.description += (P.description.isEmpty() ? QString() : QStringLiteral("\n")) + string(c); return true; }
    if (c.accept("reference")) {
        c.accept("date");
        P.referenceDate = number(c);
        return true;
    }
    if (w == QLatin1String("date")) {
        ++c.i;
        if (c.accept("wrap")) {
            if (c.accept("year")) P.dateWrapYear = int(number(c));
            else parseOnOff(c, &P.dateWrap);
        }
        return true;
    }
    if (c.accept("timestamp")) {
        Timestamp& t = P.timestamp;
        if (parseOnOff(c, &t.active)) return true;
        if (c.accept("color")) { parseColor(c, &t.text.color); return true; }
        if (c.accept("rot")) { t.angle = number(c); return true; }
        if (c.accept("font")) { parseFont(c, &t.text.font); return true; }
        if (c.accept("char")) { c.accept("size"); t.text.size = number(c); return true; }
        if (c.accept("def")) { t.value = string(c); return true; }
        const double x = number(c), y = number(c);
        t.pos = QPointF(x, y);
        return true;
    }
    // regions
    static const QRegularExpression regRe(QStringLiteral("^r([0-4])$"));
    if (regRe.match(w).hasMatch()) {
        const int ri = w.mid(1).toInt();
        ++c.i;
        return regionProps(c, P.regions[ri]);
    }

    // ---------------------------------------------------------- with / target / type
    if (c.accept("with")) {
        const QString what = c.word();
        int gi;
        if (resolveGraph(what, &gi)) {
            ++c.i;
            m_curGraph = gi;
            graph();
            return true;
        }
        if (what == QLatin1String("string") || what == QLatin1String("line") || what == QLatin1String("box") ||
            what == QLatin1String("ellipse")) {
            ++c.i;
            if (m_haveObject) P.objects.push_back(m_object);
            m_object = DrawObject();
            m_object.kind = what == QLatin1String("string") ? DrawObjectKind::Text
                          : what == QLatin1String("line")   ? DrawObjectKind::Line
                          : what == QLatin1String("box")    ? DrawObjectKind::Box
                                                            : DrawObjectKind::Ellipse;
            m_object.graph = m_curGraph;
            m_object.textStyle = TextStyle{P.stylePreset == QLatin1String("Grace Classic") ? QStringLiteral("Times-Roman") : QStringLiteral("Helvetica"), 1.0, QColor(Qt::black)};
            m_objectKind = what;
            m_haveObject = true;
            return true;
        }
        return false;
    }
    if (c.accept("target")) {
        finishBlock();
        int gi, si;
        const QString ref = c.word();
        if (!resolveSet(ref, &gi, &si, false)) { error(QStringLiteral("bad target %1").arg(ref)); return true; }
        ++c.i;
        m_haveTarget = true;
        m_targetGraph = gi;
        m_targetSet = si;
        m_curGraph = gi;
        return true;
    }
    if (w == QLatin1String("type")) {
        ++c.i;
        SetType st;
        GraphType gt;
        const QString what = c.word();
        if (parseSetType(what, &st)) {
            if (m_haveTarget) m_blockType = st;
            else {
                m_nextSetType = st;
                m_typeSeen = true;
            }
            ++c.i;
            return true;
        }
        if (parseGraphType(what, &gt)) {
            graph().type = gt;
            ++c.i;
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------- graph-level
    int gi;
    if (resolveGraph(w, &gi)) {
        ++c.i;
        const int saved = m_curGraph;
        m_curGraph = gi;
        Graph& g = graph();
        bool handled = true;
        bool b;
        if (parseOnOff(c, &b)) { g.active = true; if (!b) g.hidden = true; }  // "g0 off" hides the graph
        else if (c.accept("smartminor")) parseOnOff(c, &g.smartMinorTicks);
        else if (c.accept("hidden")) parseOnOff(c, &g.hidden);
        else if (c.accept("type")) {
            GraphType t;
            const QString tw = c.word();
            if (parseGraphType(tw, &t)) g.type = t;
            if (tw == QLatin1String("logx")) g.xscale = ScaleType::Log;
            if (tw == QLatin1String("logy")) g.yscale = ScaleType::Log;
            if (tw == QLatin1String("logxy")) { g.xscale = ScaleType::Log; g.yscale = ScaleType::Log; }
            ++c.i;
        } else if (c.accept("stacked")) parseOnOff(c, &g.stacked);
        else if (c.accept("bar")) { c.accept("hgap"); g.barHGap = number(c); }
        else if (c.accept("fixedpoint")) {
            if (parseOnOff(c, &g.fixedPoint.active)) {}
            else if (c.accept("type")) g.fixedPoint.type = int(number(c));
            else if (c.accept("xy")) { const double x = number(c), y = number(c); g.fixedPoint.xy = QPointF(x, y); }
            else if (c.accept("format")) {
                parseLabelFormat(c.word(), &g.fixedPoint.formatX); ++c.i;
                parseLabelFormat(c.word(), &g.fixedPoint.formatY); ++c.i;
            } else if (c.accept("prec")) { g.fixedPoint.precX = int(number(c)); g.fixedPoint.precY = int(number(c)); }
        } else if (c.accept("autoscale") || c.accept("label")) {
            c.i = c.t.size();
        } else {
            handled = graphProps(c, g);
            m_curGraph = gi;  // keep the focus on the addressed graph like Grace's "with"
            return handled;
        }
        m_curGraph = saved;
        return handled;
    }

    // ---------------------------------------------------------- set-level
    int sgi, ssi;
    if (resolveSet(w, &sgi, &ssi, false) && !c.isPunct("=", 1)) {
        ++c.i;
        // "s0 point x, y" and operations on sets
        resolveSet(w, &sgi, &ssi, true);
        Graph& g = m_p->graphs[sgi];
        DataSet& s = g.sets[ssi];
        if (c.accept("point")) {
            const double x = number(c), y = number(c);
            s.cols[0].push_back(x);
            s.cols[1].push_back(y);
            for (int k = 2; k < s.columns(); ++k) s.cols[k].push_back(0.0);
            return true;
        }
        if (c.accept("length")) {
            s.resize(int(number(c)));
            return true;
        }
        return setProps(c, s, ssi);
    }

    // ---------------------------------------------------------- properties of the current graph
    static const QSet<QString> graphWords = {
        QStringLiteral("world"),     QStringLiteral("view"),      QStringLiteral("title"),     QStringLiteral("subtitle"),
        QStringLiteral("xaxes"),     QStringLiteral("yaxes"),     QStringLiteral("xaxis"),     QStringLiteral("yaxis"),
        QStringLiteral("altxaxis"),  QStringLiteral("altyaxis"),  QStringLiteral("zeroxaxis"), QStringLiteral("zeroyaxis"),
        QStringLiteral("legend"),    QStringLiteral("frame"),     QStringLiteral("stack"),     QStringLiteral("znorm")};
    if (graphWords.contains(w)) return graphProps(c, graph());

    // ---------------------------------------------------------- drawing objects ("string ...", "line ...")
    if (w == QLatin1String("string") || w == QLatin1String("line") || w == QLatin1String("box") || w == QLatin1String("ellipse")) {
        ++c.i;
        if (!m_haveObject || m_objectKind != w) {
            if (m_haveObject) P.objects.push_back(m_object);
            m_object = DrawObject();
            m_object.kind = w == QLatin1String("string") ? DrawObjectKind::Text
                          : w == QLatin1String("line")   ? DrawObjectKind::Line
                          : w == QLatin1String("box")    ? DrawObjectKind::Box
                                                         : DrawObjectKind::Ellipse;
            m_object.graph = m_curGraph;
            m_objectKind = w;
            m_haveObject = true;
        }
        return objectProps(c, m_object, w);
    }

    // ---------------------------------------------------------- commands
    if (c.accept("autoscale")) {
        Graph& g = graph();
        if (c.accept("onread")) {
            const QString t = c.word();
            ++c.i;
            autoscaleOnRead = t == QLatin1String("none") ? 0 : t == QLatin1String("xaxes") ? 1 : t == QLatin1String("yaxes") ? 2 : 3;
            return true;
        }
        if (c.accept("xaxes")) { g.autoscale(1); return true; }
        if (c.accept("yaxes")) { g.autoscale(2); return true; }
        if (!c.atEnd()) {
            int ag, as;
            if (resolveSet(c.word(), &ag, &as)) {
                if (Graph* gg = m_p->graph(ag)) gg->autoscale(3, as);
                ++c.i;
                return true;
            }
        }
        g.autoscale(3);
        return true;
    }
    if (c.accept("autoticks")) { graph().autoticks(); return true; }
    if (c.accept("redraw") || c.accept("updateall") || c.accept("flush")) {
        if (redrawHandler) redrawHandler();
        return true;
    }
    if (c.accept("sleep")) {
        const double s = number(c);
        if (s > 0 && s < 3600) QThread::msleep(static_cast<unsigned long>(s * 1000));
        return true;
    }
    if (c.accept("exit")) {
        int code = 0;
        if (!c.atEnd()) code = int(number(c));
        if (exitHandler) exitHandler(code);
        return true;
    }
    if (c.accept("help")) return true;
    if (c.accept("print")) {
        if (c.accept("to")) {
            if (c.accept("device")) { m_hardcopy.printFile.clear(); return true; }
            m_hardcopy.printFile = string(c);
            return true;
        }
        if (printHandler) printHandler(m_hardcopy);
        else warn(QStringLiteral("PRINT ignored: no output handler"));
        return true;
    }
    if (c.accept("hardcopy")) {
        c.accept("device");
        m_hardcopy.device = string(c);
        return true;
    }
    if (w == QLatin1String("device")) return deviceCommand(c);
    if (c.accept("saveall")) {
        const QString f = string(c);
        if (safeMode) { error(QStringLiteral("SAVEALL is disabled in safe mode (use -nosafe)")); return true; }
        QString err;
        if (!projectio::save(*m_p, QDir(workingDir).filePath(f), &err)) error(err);
        return true;
    }
    if (c.accept("load")) {
        const QString f = string(c);
        if (loadHandler) loadHandler(QDir(workingDir).filePath(f));
        else runFile(f);
        return true;
    }
    if (c.accept("focus")) {
        int g;
        if (resolveGraph(c.word(), &g)) {
            m_curGraph = g;
            graph().hidden = false;
            m_p->currentGraph = g;
        }
        ++c.i;
        return true;
    }
    if (c.accept("kill")) {
        if (c.accept("block")) { m_block.clear(); m_blockStrings.clear(); return true; }
        const QString what = c.word();
        int g, s;
        if (resolveGraph(what, &g)) {
            if (g < m_p->graphs.size()) {
                m_p->graphs[g] = Graph();
            }
            ++c.i;
            return true;
        }
        if (resolveSet(what, &g, &s)) {
            if (Graph* gr = m_p->graph(g)) if (s < gr->sets.size()) gr->sets[s] = DataSet();
            ++c.i;
            return true;
        }
        if (c.accept("sets") || c.accept("all")) {
            graph().sets.clear();
            return true;
        }
        return false;
    }
    if (w == QLatin1String("copy") || w == QLatin1String("move") || w == QLatin1String("swap")) {
        ++c.i;
        return setOperation(c, w);
    }
    if (c.accept("read")) {
        ReadMode mode = ReadMode::Single;
        SetType type = m_nextSetType;
        if (c.accept("nxy")) mode = ReadMode::NXY;
        else if (c.accept("block")) mode = ReadMode::Block;
        else {
            SetType t;
            if (parseSetType(c.word(), &t)) { type = t; ++c.i; }
        }
        const QString f = string(c);
        readData(f, mode, type, m_curGraph);
        return true;
    }
    if (c.accept("block")) {
        SetType t = SetType::XY;
        if (parseSetType(c.word(), &t)) ++c.i;
        const QString spec = string(c);
        formBlockSet(t, spec, m_curGraph);
        return true;
    }
    if (c.accept("write")) {
        const QString ref = c.word();
        DataSet* s = setRef(ref);
        ++c.i;
        if (!s) { error(QStringLiteral("no such set %1").arg(ref)); return true; }
        QString file, fmt = QStringLiteral("%.8g");
        while (!c.atEnd()) {
            if (c.accept("file")) file = string(c);
            else if (c.accept("format")) fmt = string(c);
            else ++c.i;
        }
        QString out;
        const QByteArray f8 = fmt.toLatin1();
        for (int i = 0; i < s->size(); ++i) {
            QStringList row;
            for (int k = 0; k < s->columns(); ++k) {
                char buf[64];
                std::snprintf(buf, sizeof buf, f8.constData(), s->cols[k].value(i));
                row << QString::fromLatin1(buf);
            }
            out += row.join(QLatin1Char(' ')) + QLatin1Char('\n');
        }
        if (file.isEmpty()) info(out);
        else if (safeMode) error(QStringLiteral("WRITE FILE is disabled in safe mode"));
        else {
            QFile fo(QDir(workingDir).filePath(file));
            if (fo.open(QIODevice::WriteOnly | QIODevice::Text)) fo.write(out.toUtf8());
            else error(QStringLiteral("cannot write %1").arg(file));
        }
        return true;
    }
    if (c.accept("define")) {
        const QString name = c.word();
        ++c.i;
        if (c.acceptPunct("[")) {
            int n = 0;
            if (!c.isPunct("]")) n = int(number(c));
            c.acceptPunct("]");
            m_vars.setVector(name, QVector<double>(std::max(0, n), 0.0));
        } else {
            m_vars.setScalar(name, 0.0);
        }
        return true;
    }
    if (c.accept("clear")) {
        m_vars.remove(c.word());
        ++c.i;
        return true;
    }
    if (w == QLatin1String("fit") || w == QLatin1String("nonlfit")) return fitCommand(c);
    // a0 constraints on/off
    static const QRegularExpression parRe(QStringLiteral("^a(\\d)$"));
    if (parRe.match(w).hasMatch() && c.is("constraints", 1)) {
        const int k = w.mid(1).toInt();
        c.i += 2;
        parseOnOff(c, &m_fit.params[k].constrained);
        return true;
    }
    if (c.accept("sort")) {
        DataSet* s = setRef(c.word());
        ++c.i;
        if (!s) return true;
        transforms::SortKey key = transforms::SortKey::X;
        bool desc = false;
        while (!c.atEnd()) {
            if (c.accept("y")) key = transforms::SortKey::Y;
            else if (c.accept("x")) key = transforms::SortKey::X;
            else if (c.accept("descending")) desc = true;
            else ++c.i;
        }
        const auto perm = transforms::sortPermutation(s->cols[0], s->cols[1], key, desc);
        for (int k = 0; k < s->columns(); ++k) {
            QVector<double> nv(perm.size());
            for (int i = 0; i < perm.size(); ++i) nv[i] = s->cols[k].value(perm[i]);
            s->cols[k] = nv;
        }
        if (!s->strings.isEmpty()) {
            QStringList ns;
            for (int i : perm) ns << s->strings.value(i);
            s->strings = ns;
        }
        return true;
    }
    if (c.accept("reverse")) {
        DataSet* s = setRef(c.word());
        ++c.i;
        if (!s) return true;
        for (int k = 0; k < s->columns(); ++k) std::reverse(s->cols[k].begin(), s->cols[k].end());
        std::reverse(s->strings.begin(), s->strings.end());
        return true;
    }
    if (c.accept("join")) {
        DataSet* a = setRef(c.word());
        ++c.i;
        c.acceptPunct(",");
        DataSet* b = setRef(c.word());
        ++c.i;
        if (!a || !b || a == b) return true;
        for (int k = 0; k < a->columns(); ++k) a->cols[k] += b->cols[k].isEmpty() ? QVector<double>(b->size(), 0.0) : b->cols[k];
        *b = DataSet();
        return true;
    }
    if (c.accept("split")) {
        int g, s;
        if (!resolveSet(c.word(), &g, &s)) return true;
        ++c.i;
        const int len = int(number(c));
        Graph* gr = m_p->graph(g);
        if (!gr || !gr->set(s) || len <= 0) return true;
        DataSet src = gr->sets[s];
        for (int start = len; start < src.size(); start += len) {
            const int ni = gr->nextFreeSet();
            DataSet& d = gr->ensureSet(ni);
            d = src;
            d.active = true;
            for (int k = 0; k < 6; ++k) d.cols[k] = src.cols[k].mid(start, len);
            presets::styleSet(d, ni, m_p->stylePreset);
        }
        DataSet& first = gr->sets[s];
        for (int k = 0; k < 6; ++k) first.cols[k] = src.cols[k].mid(0, len);
        return true;
    }
    if (c.accept("drop")) {
        DataSet* s = setRef(c.word());
        ++c.i;
        const int a = int(number(c)), b = int(number(c));
        if (!s || a < 0 || b < a) return true;
        for (int k = 0; k < s->columns(); ++k) if (a < s->cols[k].size()) s->cols[k].remove(a, std::min(b, int(s->cols[k].size()) - 1) - a + 1);
        return true;
    }
    if (c.accept("arrange")) {
        // ARRANGE without parentheses: arrange rows cols offset hgap vgap
        const int r = int(number(c)), cc = int(number(c));
        const double off = c.atEnd() ? 0.1 : number(c), hg = c.atEnd() ? 0.15 : number(c), vg = c.atEnd() ? 0.15 : number(c);
        arrangeGraphs(*m_p, r, cc, off, hg, vg);
        return true;
    }
    // bare expression -> print its value (console convenience)
    expr::Value v;
    QString err;
    if (evaluate(c.src, &v, &err)) {
        if (v.isVector) {
            QStringList l;
            for (int i = 0; i < std::min(20, v.size()); ++i) l << QString::number(v.v[i], 'g', 10);
            info(QStringLiteral("[%1]%2").arg(l.join(QStringLiteral(", ")), v.size() > 20 ? QStringLiteral(" ... (%1 values)").arg(v.size()) : QString()));
        } else {
            info(QString::number(v.s, 'g', 15));
        }
        return true;
    }
    return false;
}

// ====================================================================== page / device

bool Interpreter::pageProps(Cursor& c)
{
    Project& P = *m_p;
    c.accept("page");
    if (c.accept("size")) {
        const double w = number(c), h = number(c);
        if (w > 0 && h > 0) { P.page.width = w; P.page.height = h; }
        return true;
    }
    if (c.accept("resize")) {
        const double w = number(c), h = number(c);
        if (w > 0 && h > 0) { P.page.width = w; P.page.height = h; }
        return true;
    }
    if (c.accept("background")) {
        if (c.accept("fill")) { parseOnOff(c, &P.page.backgroundFill); return true; }
        c.accept("color");
        QColor col;
        if (parseColor(c, &col)) P.page.background.color = col;
        return true;
    }
    if (c.accept("scroll") || c.accept("inout") || c.accept("layout")) { c.i = c.t.size(); return true; }
    // legacy "page landscape|portrait"
    if (c.accept("landscape")) { if (P.page.width < P.page.height) std::swap(P.page.width, P.page.height); return true; }
    if (c.accept("portrait")) { if (P.page.width > P.page.height) std::swap(P.page.width, P.page.height); return true; }
    c.i = c.t.size();
    return true;
}

bool Interpreter::deviceCommand(Cursor& c)
{
    c.accept("device");
    const QString dev = string(c);
    if (c.accept("page")) {
        c.accept("size");
        const double w = number(c), h = number(c);
        if (w > 0 && h > 0 && dev.compare(m_hardcopy.device, Qt::CaseInsensitive) == 0) {
            m_p->page.width = w;
            m_p->page.height = h;
        }
        return true;
    }
    if (c.accept("dpi")) {
        const double d = number(c);
        if (dev.compare(m_hardcopy.device, Qt::CaseInsensitive) == 0 || m_hardcopy.device.isEmpty()) m_hardcopy.dpi = d;
        return true;
    }
    if (c.accept("op")) {
        const QString op = string(c);
        if (dev.compare(m_hardcopy.device, Qt::CaseInsensitive) == 0) {
            m_hardcopy.options = op;
            if (op.contains(QLatin1String("grayscale"))) m_hardcopy.grayscale = true;
            if (op.contains(QLatin1String("transparent:on"))) m_hardcopy.transparent = true;
        }
        return true;
    }
    c.i = c.t.size();
    return true;
}

// ====================================================================== graph properties

bool Interpreter::graphProps(Cursor& c, Graph& g)
{
    if (c.accept("world")) {
        if (c.accept("xmin")) { g.wxmin = number(c); return true; }
        if (c.accept("xmax")) { g.wxmax = number(c); return true; }
        if (c.accept("ymin")) { g.wymin = number(c); return true; }
        if (c.accept("ymax")) { g.wymax = number(c); return true; }
        const double a = number(c), b = number(c), cc = number(c), d = number(c);
        g.wxmin = a; g.wymin = b; g.wxmax = cc; g.wymax = d;
        return true;
    }
    if (c.accept("stack")) { c.i = c.t.size(); return true; }
    if (c.accept("znorm")) { g.znorm = number(c); return true; }
    if (c.accept("view")) {
        if (c.accept("xmin")) { g.vxmin = number(c); return true; }
        if (c.accept("xmax")) { g.vxmax = number(c); return true; }
        if (c.accept("ymin")) { g.vymin = number(c); return true; }
        if (c.accept("ymax")) { g.vymax = number(c); return true; }
        const double a = number(c), b = number(c), cc = number(c), d = number(c);
        g.vxmin = a; g.vymin = b; g.vxmax = cc; g.vymax = d;
        return true;
    }
    if (c.is("title") || c.is("subtitle")) {
        TitleText& t = c.is("title") ? g.title : g.subtitle;
        ++c.i;
        if (c.accept("font")) { parseFont(c, &t.style.font); return true; }
        if (c.accept("size")) { t.style.size = number(c); return true; }
        if (c.accept("color")) { parseColor(c, &t.style.color); return true; }
        if (c.accept("linewidth")) { number(c); return true; }
        t.text = string(c);
        return true;
    }
    if (c.is("xaxes") || c.is("yaxes")) {
        const bool x = c.is("xaxes");
        ++c.i;
        if (c.accept("scale")) {
            ScaleType st;
            if (parseScaleType(c.word(), &st)) (x ? g.xscale : g.yscale) = st;
            ++c.i;
            return true;
        }
        if (c.accept("invert")) { parseOnOff(c, x ? &g.xinvert : &g.yinvert); return true; }
        return false;
    }
    static const QStringList axisNames = {QStringLiteral("xaxis"), QStringLiteral("yaxis"), QStringLiteral("altxaxis"), QStringLiteral("altyaxis")};
    const int ai = axisNames.indexOf(c.word());
    if (ai >= 0) {
        ++c.i;
        return axisProps(c, g.axes[ai]);
    }
    if (c.is("zeroxaxis") || c.is("zeroyaxis")) {
        // xmgr-era zero axes: map onto the alternate axis drawn at zero
        const bool x = c.is("zeroxaxis");
        ++c.i;
        Axis tmp = g.axes[x ? 2 : 3];
        const bool wasActive = tmp.active;
        const bool ok = axisProps(c, tmp);
        if (tmp.active && !wasActive) {
            tmp.zero = true;
            g.axes[x ? 2 : 3] = tmp;
        }
        return ok;
    }
    if (c.accept("legend")) return legendProps(c, g.legend, g);
    if (c.accept("frame")) return frameProps(c, g.frame);
    return false;
}

bool Interpreter::tickProps(Cursor& c, TickProps& tp)
{
    if (c.accept("size")) { tp.size = number(c); return true; }
    if (c.accept("color")) { parseColor(c, &tp.line.color); return true; }
    if (c.accept("linewidth")) { tp.line.width = number(c); return true; }
    if (c.accept("linestyle")) { tp.line.style = int(number(c)); return true; }
    if (c.accept("grid")) {
        parseOnOff(c, &tp.grid);
        if (tp.grid) {
            // Grace draws grid lines with the tick colour/style
            tp.gridLine.color = tp.line.color;
            tp.gridLine.style = tp.line.style;
            tp.gridLine.width = tp.line.width;
        }
        return true;
    }
    // CactusPlot extension: separate grid styling
    if (c.accept("gridcolor")) { parseColor(c, &tp.gridLine.color); return true; }
    if (c.accept("gridlinewidth")) { tp.gridLine.width = number(c); return true; }
    if (c.accept("gridlinestyle")) { tp.gridLine.style = int(number(c)); return true; }
    return false;
}

static bool parseSide(const QString& w, SideOp* out)
{
    if (w == QLatin1String("normal") || w == QLatin1String("bottom") || w == QLatin1String("left")) { *out = SideOp::Normal; return true; }
    if (w == QLatin1String("opposite") || w == QLatin1String("top") || w == QLatin1String("right")) { *out = SideOp::Opposite; return true; }
    if (w == QLatin1String("both")) { *out = SideOp::Both; return true; }
    return false;
}

bool Interpreter::axisProps(Cursor& c, Axis& ax)
{
    bool b;
    if (parseOnOff(c, &b)) { ax.active = b; return true; }
    if (c.accept("type")) {
        if (c.accept("zero")) { parseOnOff(c, &ax.zero); return true; }
        c.i = c.t.size();
        return true;
    }
    if (c.accept("offset")) {
        const double a = number(c), bb = number(c);
        ax.offset = QPointF(a, bb);
        return true;
    }
    if (c.accept("bar")) {
        if (parseOnOff(c, &ax.barOn)) return true;
        if (c.accept("color")) { parseColor(c, &ax.bar.color); return true; }
        if (c.accept("linestyle")) { ax.bar.style = int(number(c)); return true; }
        if (c.accept("linewidth")) { ax.bar.width = number(c); return true; }
        return false;
    }
    if (c.accept("label")) {
        if (c.accept("layout")) {
            ax.labelLayout = c.accept("perp") ? LabelLayout::Perpendicular : (c.accept("para"), LabelLayout::Parallel);
            return true;
        }
        if (c.accept("place")) {
            if (c.accept("auto")) { ax.labelPlaceAuto = true; return true; }
            if (c.accept("spec")) { ax.labelPlaceAuto = false; return true; }
            SideOp so;
            if (parseSide(c.word(), &so)) { ax.labelSide = so; ++c.i; return true; }
            const double x = number(c), y = number(c);
            ax.labelOffset = QPointF(x, y);
            return true;
        }
        if (c.accept("char")) { c.accept("size"); ax.labelText.size = number(c); return true; }
        if (c.accept("font")) { parseFont(c, &ax.labelText.font); return true; }
        if (c.accept("color")) { parseColor(c, &ax.labelText.color); return true; }
        if (c.accept("op")) {
            SideOp so;
            if (parseSide(c.word(), &so)) ax.labelSide = so;
            ++c.i;
            return true;
        }
        if (c.accept("linewidth")) { number(c); return true; }
        ax.label = string(c);
        return true;
    }
    if (c.accept("tick")) {
        if (parseOnOff(c, &ax.tickOn)) return true;
        if (c.accept("major")) {
            bool onoff;
            if (parseOnOff(c, &onoff)) return true;  // xmgr: "tick major on"
            if (tickProps(c, ax.majorProps)) return true;
            if (c.hasTopLevelComma()) {
                // special tick: index, value
                const int idx = int(number(c));
                const double v = number(c);
                while (ax.specTicks.size() <= idx) ax.specTicks.push_back(TickMark{0.0, true, QString()});
                ax.specTicks[idx].value = v;
                ax.specTicks[idx].major = true;
                return true;
            }
            ax.major = number(c);
            return true;
        }
        if (c.accept("minor")) {
            bool onoff;
            if (parseOnOff(c, &onoff)) { if (!onoff) ax.minorTicks = 0; return true; }
            if (c.accept("ticks")) { ax.minorTicks = int(number(c)); return true; }
            if (tickProps(c, ax.minorProps)) return true;
            if (c.hasTopLevelComma()) {
                const int idx = int(number(c));
                const double v = number(c);
                while (ax.specTicks.size() <= idx) ax.specTicks.push_back(TickMark{0.0, true, QString()});
                ax.specTicks[idx].value = v;
                ax.specTicks[idx].major = false;
                return true;
            }
            // legacy: minor spacing -> number of minor ticks
            const double v = number(c);
            if (v > 0 && ax.major > 0) ax.minorTicks = std::max(0, int(std::lround(ax.major / v)) - 1);
            return true;
        }
        if (c.accept("default")) { ax.autoTickCount = int(number(c)); return true; }
        if (c.accept("place")) {
            if (c.accept("rounded")) { parseOnOff(c, &ax.placeRounded); return true; }
            SideOp so;
            if (parseSide(c.word(), &so)) { ax.tickSide = so; ++c.i; return true; }
            c.i = c.t.size();
            return true;
        }
        if (c.accept("in")) { ax.tickDir = TickDirection::In; return true; }
        if (c.accept("out")) { ax.tickDir = TickDirection::Out; return true; }
        if (c.accept("both")) { ax.tickDir = TickDirection::Both; return true; }
        if (c.accept("op")) {
            SideOp so;
            if (parseSide(c.word(), &so)) ax.tickSide = so;
            ++c.i;
            return true;
        }
        if (c.accept("type")) {
            ax.tickAuto = c.accept("auto") || !c.accept("spec");
            return true;
        }
        if (c.accept("spec")) {
            if (c.accept("type")) {
                const QString t = c.word();
                ++c.i;
                if (t == QLatin1String("none")) { ax.tickAuto = true; ax.ticklabelAuto = true; }
                else if (t == QLatin1String("ticks")) { ax.tickAuto = false; ax.ticklabelAuto = true; }
                else if (t == QLatin1String("both")) { ax.tickAuto = false; ax.ticklabelAuto = false; }
                return true;
            }
            const int n = int(number(c));
            ax.specTicks.resize(std::max(0, n));
            return true;
        }
        // xmgr legacy
        if (c.accept("size")) { ax.majorProps.size = number(c); return true; }
        if (c.accept("color")) { parseColor(c, &ax.majorProps.line.color); ax.minorProps.line.color = ax.majorProps.line.color; return true; }
        if (c.accept("linewidth")) { ax.majorProps.line.width = number(c); return true; }
        if (c.accept("min") || c.accept("max") || c.accept("alt") || c.accept("offsetx") || c.accept("offsety") ||
            c.accept("log")) {
            c.i = c.t.size();
            return true;
        }
        return false;
    }
    if (c.accept("ticklabel")) {
        if (parseOnOff(c, &ax.ticklabelOn)) return true;
        if (c.accept("prec")) { ax.precision = int(number(c)); return true; }
        if (c.accept("format")) {
            LabelFormat f;
            if (parseLabelFormat(c.word(), &f)) ax.format = f;
            ++c.i;
            return true;
        }
        if (c.accept("append")) { ax.append = string(c); return true; }
        if (c.accept("prepend")) { ax.prepend = string(c); return true; }
        if (c.accept("angle")) { ax.ticklabelAngle = number(c); return true; }
        if (c.accept("skip")) { ax.ticklabelSkip = int(number(c)); return true; }
        if (c.accept("stagger")) { ax.ticklabelStagger = int(number(c)); return true; }
        if (c.accept("place") || c.accept("op")) {
            SideOp so;
            if (parseSide(c.word(), &so)) ax.ticklabelSide = so;
            ++c.i;
            return true;
        }
        if (c.accept("offset")) {
            if (c.accept("auto")) { ax.ticklabelOffsetAuto = true; return true; }
            if (c.accept("spec")) { ax.ticklabelOffsetAuto = false; return true; }
            const double x = number(c), y = number(c);
            ax.ticklabelOffset = QPointF(x, y);
            return true;
        }
        if (c.accept("sign")) {
            const QString s = c.word();
            ++c.i;
            ax.ticklabelSign = s == QLatin1String("absolute") ? TickSign::Absolute : s == QLatin1String("negate") ? TickSign::Negate : TickSign::Normal;
            return true;
        }
        if (c.accept("start")) {
            if (c.accept("type")) { ax.ticklabelStartAuto = c.accept("auto") || !c.accept("spec"); return true; }
            ax.ticklabelStart = number(c);
            return true;
        }
        if (c.accept("stop")) {
            if (c.accept("type")) { ax.ticklabelStopAuto = c.accept("auto") || !c.accept("spec"); return true; }
            ax.ticklabelStop = number(c);
            return true;
        }
        if (c.accept("char")) { c.accept("size"); ax.ticklabelText.size = number(c); return true; }
        if (c.accept("font")) { parseFont(c, &ax.ticklabelText.font); return true; }
        if (c.accept("color")) { parseColor(c, &ax.ticklabelText.color); return true; }
        if (c.accept("formula")) { ax.ticklabelFormula = string(c); return true; }
        if (c.accept("type")) { ax.ticklabelAuto = c.accept("auto") || !c.accept("spec"); return true; }
        if (c.accept("layout")) {
            if (c.accept("vertical")) ax.ticklabelAngle = 90;
            else if (c.accept("horizontal")) ax.ticklabelAngle = 0;
            else c.i = c.t.size();
            return true;
        }
        if (c.accept("linewidth") || c.accept("decimal") || c.accept("vgap") || c.accept("hgap")) { c.i = c.t.size(); return true; }
        if (c.hasTopLevelComma()) {
            const int idx = int(number(c));
            const QString lab = string(c);
            while (ax.specTicks.size() <= idx) ax.specTicks.push_back(TickMark{0.0, true, QString()});
            ax.specTicks[idx].label = lab;
            return true;
        }
        return false;
    }
    return false;
}

bool Interpreter::legendProps(Cursor& c, Legend& L, const Graph& g)
{
    Q_UNUSED(g);
    bool b;
    if (parseOnOff(c, &L.active)) return true;
    if (c.accept("loctype")) {
        L.loctype = c.accept("world") ? LocType::World : (c.accept("view"), LocType::View);
        L.autoPlace = false;
        return true;
    }
    if (c.accept("x1")) { L.pos.setX(number(c)); L.autoPlace = false; return true; }
    if (c.accept("y1")) { L.pos.setY(number(c)); L.autoPlace = false; return true; }
    if (c.accept("auto")) { parseOnOff(c, &L.autoPlace); return true; }
    if (c.accept("box")) {
        if (c.accept("fill")) {
            if (parseOnOff(c, &b)) { L.boxFill.pattern = b ? 1 : 0; return true; }
            if (c.accept("color")) { parseColor(c, &L.boxFill.color); return true; }
            if (c.accept("pattern")) { L.boxFill.pattern = int(number(c)); return true; }
            return false;
        }
        if (parseOnOff(c, &b)) { L.box.style = b ? std::max(1, L.box.style) : 0; return true; }
        if (c.accept("color")) { parseColor(c, &L.box.color); return true; }
        if (c.accept("pattern")) { const int p = int(number(c)); if (p == 0) L.box.style = 0; return true; }
        if (c.accept("linewidth")) { L.box.width = number(c); return true; }
        if (c.accept("linestyle")) { L.box.style = int(number(c)); return true; }
        if (c.accept("radius")) { L.cornerRadius = number(c); return true; }
        return false;
    }
    if (c.accept("font")) { parseFont(c, &L.text.font); return true; }
    if (c.accept("char")) { c.accept("size"); L.text.size = number(c); return true; }
    if (c.accept("color")) { parseColor(c, &L.text.color); return true; }
    if (c.accept("length")) { L.length = int(number(c)); return true; }
    if (c.accept("vgap")) { L.vgap = int(number(c)); return true; }
    if (c.accept("hgap")) { L.hgap = int(number(c)); return true; }
    if (c.accept("invert")) { parseOnOff(c, &L.invert); return true; }
    if (c.accept("layout") || c.accept("linestyle") || c.accept("linewidth")) { c.i = c.t.size(); return true; }
    if (c.accept("string")) {
        // legacy: legend string N "text"
        const int idx = int(number(c));
        const QString s = string(c);
        Graph& gr = graph();
        if (idx >= 0) {
            const bool existed = gr.set(idx) != nullptr;
            DataSet& ds = gr.ensureSet(idx);
            if (!existed) presets::styleSet(ds, idx, m_p->stylePreset);
            ds.legend = s;
        }
        return true;
    }
    if (c.isNumberLike()) {
        const double x = number(c), y = number(c);
        L.pos = QPointF(x, y);
        L.autoPlace = false;
        return true;
    }
    return false;
}

bool Interpreter::frameProps(Cursor& c, Frame& f)
{
    bool b;
    if (parseOnOff(c, &b)) { if (!b) f.line.style = 0; return true; }
    if (c.accept("type")) { f.type = FrameType(std::clamp(int(number(c)), 0, 5)); return true; }
    if (c.accept("linestyle")) { f.line.style = int(number(c)); return true; }
    if (c.accept("linewidth")) { f.line.width = number(c); return true; }
    if (c.accept("color")) { parseColor(c, &f.line.color); return true; }
    if (c.accept("pattern")) { const int p = int(number(c)); if (p == 0) f.line.style = 0; return true; }
    if (c.accept("background")) {
        if (c.accept("color")) { parseColor(c, &f.background.color); return true; }
        if (c.accept("pattern")) { f.background.pattern = int(number(c)); return true; }
        return false;
    }
    if (c.accept("fill")) { if (parseOnOff(c, &b)) f.background.pattern = b ? 1 : 0; return true; }
    return false;
}

// ====================================================================== set properties

bool Interpreter::setProps(Cursor& c, DataSet& s, int setIndex)
{
    Q_UNUSED(setIndex);
    bool b;
    if (c.accept("hidden")) { parseOnOff(c, &s.hidden); return true; }
    if (parseOnOff(c, &b)) { s.hidden = !b; return true; }
    if (c.accept("type")) {
        SetType t;
        if (parseSetType(c.word(), &t)) s.setType(t);
        ++c.i;
        return true;
    }
    if (c.accept("symbol")) {
        if (c.accept("size")) { s.symbol.size = number(c); return true; }
        if (c.accept("color")) { parseColor(c, &s.symbol.line.color); return true; }
        if (c.accept("pattern")) { const int p = int(number(c)); if (p == 0) s.symbol.line.style = 0; return true; }
        if (c.accept("fill")) {
            if (c.accept("color")) { parseColor(c, &s.symbol.fill.color); return true; }
            if (c.accept("pattern")) { s.symbol.fill.pattern = int(number(c)); return true; }
            // legacy: symbol fill N (0 none, 1 solid, 2 opaque)
            const int v = int(number(c));
            s.symbol.fill.pattern = v == 0 ? 0 : 1;
            if (v == 2) s.symbol.fill.color = m_p->page.background.color;
            else if (v == 1) s.symbol.fill.color = s.symbol.line.color;
            return true;
        }
        if (c.accept("linewidth")) { s.symbol.line.width = number(c); return true; }
        if (c.accept("linestyle")) { s.symbol.line.style = int(number(c)); return true; }
        if (c.accept("char")) {
            if (c.accept("font")) { parseFont(c, &s.symbol.charFont); return true; }
            s.symbol.ch = int(number(c));
            return true;
        }
        if (c.accept("skip")) { s.symbol.skip = int(number(c)); return true; }
        if (c.accept("center") || c.accept("dot")) { c.i = c.t.size(); return true; }
        int sym = int(number(c));
        if (m_version < 50000) {
            // xmgr numbering: 1 = dot, 2 = circle, ... 11 = splat
            if (sym == 1) { sym = int(Symbol::Circle); s.symbol.size *= 0.3; }
            else if (sym >= 2 && sym <= 11) sym -= 1;
            else if (sym > 11) sym = int(Symbol::Circle);
        }
        s.symbol.type = Symbol(std::clamp(sym, 0, int(Symbol::Count) - 1));
        return true;
    }
    if (c.accept("line")) {
        if (c.accept("type")) { s.lineType = lineTypeFromGrace(int(number(c))); return true; }
        if (c.accept("linestyle")) { s.line.style = int(number(c)); return true; }
        if (c.accept("linewidth")) { s.line.width = number(c); return true; }
        if (c.accept("color")) { parseColor(c, &s.line.color); return true; }
        if (c.accept("pattern")) { const int p = int(number(c)); if (p == 0) s.line.style = 0; return true; }
        return false;
    }
    if (c.accept("baseline")) {
        if (c.accept("type")) { s.baselineType = BaselineType(std::clamp(int(number(c)), 0, 5)); return true; }
        parseOnOff(c, &s.baselineShown);
        return true;
    }
    if (c.accept("dropline")) { parseOnOff(c, &s.dropLines); return true; }
    if (c.accept("fill")) {
        if (c.accept("type")) { s.fillType = FillType(std::clamp(int(number(c)), 0, 2)); return true; }
        if (c.accept("rule")) { s.fillRule = int(number(c)) ? FillRule::EvenOdd : FillRule::Winding; return true; }
        if (c.accept("color")) { parseColor(c, &s.fill.color); return true; }
        if (c.accept("pattern")) { s.fill.pattern = int(number(c)); return true; }
        if (c.accept("with")) { c.i = c.t.size(); return true; }  // xmgr "fill with color|pattern"
        if (c.accept("gradient")) {
            if (c.accept("color")) { parseColor(c, &s.fill.color2); return true; }
            const QString t = c.word();
            ++c.i;
            s.fill.gradient = t == QLatin1String("vertical") ? GradientType::Vertical
                            : t == QLatin1String("horizontal") ? GradientType::Horizontal
                            : t == QLatin1String("radial") ? GradientType::Radial : GradientType::None;
            return true;
        }
        // xmgr "fill N": 1 polygon, 2 to y=0, 3-5 to x limits (approximated), 6 to world ymin, 7 to world ymax
        const int v = int(number(c));
        switch (v) {
        case 0: s.fillType = FillType::None; break;
        case 1: s.fillType = FillType::Polygon; break;
        case 6: s.fillType = FillType::ToBaseline; s.baselineType = BaselineType::GraphMin; break;
        case 7: s.fillType = FillType::ToBaseline; s.baselineType = BaselineType::GraphMax; break;
        default: s.fillType = FillType::ToBaseline; s.baselineType = BaselineType::Zero; break;
        }
        return true;
    }
    if (c.accept("avalue")) {
        AnnotatedValues& a = s.avalue;
        if (parseOnOff(c, &a.active)) return true;
        if (c.accept("type")) { a.type = int(number(c)); return true; }
        if (c.accept("char")) { c.accept("size"); a.text.size = number(c); return true; }
        if (c.accept("font")) { parseFont(c, &a.text.font); return true; }
        if (c.accept("color")) { parseColor(c, &a.text.color); return true; }
        if (c.accept("rot")) { a.angle = number(c); return true; }
        if (c.accept("format")) { LabelFormat f; if (parseLabelFormat(c.word(), &f)) a.format = f; ++c.i; return true; }
        if (c.accept("prec")) { a.precision = int(number(c)); return true; }
        if (c.accept("prepend")) { a.prepend = string(c); return true; }
        if (c.accept("append")) { a.append = string(c); return true; }
        if (c.accept("offset")) { const double x = number(c), y = number(c); a.offset = QPointF(x, y); return true; }
        return false;
    }
    if (c.accept("errorbar")) {
        ErrorBarStyle& e = s.errorbar;
        if (parseOnOff(c, &e.active)) return true;
        if (c.accept("place") || c.accept("type")) {
            const QString t = c.word();
            ++c.i;
            if (t == QLatin1String("normal") || t == QLatin1String("top") || t == QLatin1String("plus")) e.placement = ErrorBarPlacement::Top;
            else if (t == QLatin1String("opposite") || t == QLatin1String("bottom") || t == QLatin1String("minus")) e.placement = ErrorBarPlacement::Bottom;
            else e.placement = ErrorBarPlacement::Both;
            return true;
        }
        if (c.accept("color")) { parseColor(c, &e.color); return true; }
        if (c.accept("pattern")) { e.pattern = int(number(c)); return true; }
        if (c.accept("size") || c.accept("length")) { e.size = number(c); return true; }
        if (c.accept("linewidth")) { e.width = number(c); return true; }
        if (c.accept("linestyle")) { e.style = int(number(c)); return true; }
        if (c.accept("riser")) {
            if (c.accept("linewidth")) { e.riserWidth = number(c); return true; }
            if (c.accept("linestyle")) { e.riserStyle = int(number(c)); return true; }
            if (c.accept("clip")) {
                if (c.accept("length")) { e.riserClipLength = number(c); return true; }
                parseOnOff(c, &e.riserClip);
                return true;
            }
            return false;
        }
        return false;
    }
    if (c.accept("comment")) { s.comment = string(c); return true; }
    if (c.accept("legend")) { s.legend = string(c); return true; }
    // CactusPlot extensions
    if (c.accept("colormap")) {
        if (c.accept("range")) {
            if (c.accept("auto")) { s.cmapAuto = true; return true; }
            s.cmapMin = number(c);
            s.cmapMax = number(c);
            s.cmapAuto = false;
            return true;
        }
        if (c.accept("reversed")) { parseOnOff(c, &s.cmapReversed); return true; }
        if (c.accept("index")) { bool idx = false; parseOnOff(c, &idx); s.colorFromMap = !idx; return true; }
        if (c.accept("bar")) { parseOnOff(c, &s.showColorBar); return true; }
        s.colormap = string(c);
        s.colorFromMap = true;
        return true;
    }
    // xmgr legacy set properties
    if (c.accept("linestyle")) { s.line.style = int(number(c)); return true; }
    if (c.accept("linewidth")) { s.line.width = number(c); return true; }
    if (c.accept("color")) { QColor col; if (parseColor(c, &col)) { s.line.color = col; s.symbol.line.color = col; } return true; }
    if (c.accept("skip")) { s.symbol.skip = int(number(c)); return true; }
    if (c.accept("font") || c.accept("char") || c.accept("prec") || c.accept("format") || c.accept("dataformat")) { c.i = c.t.size(); return true; }
    return false;
}

// ====================================================================== objects & regions

bool Interpreter::objectProps(Cursor& c, DrawObject& o, const QString& kind)
{
    Project& P = *m_p;
    bool b;
    if (parseOnOff(c, &o.active)) return true;
    if (c.accept("loctype")) {
        o.loctype = c.accept("world") ? LocType::World : (c.accept("view"), LocType::View);
        return true;
    }
    int gi;
    if (resolveGraph(c.word(), &gi)) { o.graph = gi; ++c.i; return true; }
    if (c.accept("color")) {
        QColor col;
        if (parseColor(c, &col)) { if (kind == QLatin1String("string")) o.textStyle.color = col; else o.line.color = col; }
        return true;
    }
    if (c.accept("linewidth")) { o.line.width = number(c); return true; }
    if (c.accept("linestyle")) { o.line.style = int(number(c)); return true; }
    if (c.accept("rot")) { const double a = number(c); if (kind == QLatin1String("string")) o.angle = a; else o.rotation = a; return true; }
    if (c.accept("font")) { parseFont(c, &o.textStyle.font); return true; }
    if (c.accept("just")) { o.just = int(number(c)); return true; }
    if (c.accept("char")) { c.accept("size"); o.textStyle.size = number(c); return true; }
    if (c.accept("arrow")) {
        if (c.accept("type")) { o.arrowType = int(number(c)); return true; }
        if (c.accept("length")) { o.arrowLength = number(c); return true; }
        if (c.accept("layout")) { o.arrowDL = number(c); o.arrowLL = number(c); return true; }
        if (c.accept("size")) { o.arrowLength = number(c) * 2.0; return true; }
        o.arrow = int(number(c));
        return true;
    }
    if (c.accept("fill")) {
        if (c.accept("color")) { parseColor(c, &o.fill.color); return true; }
        if (c.accept("pattern")) { o.fill.pattern = int(number(c)); return true; }
        if (parseOnOff(c, &b)) { o.fill.pattern = b ? 1 : 0; return true; }
        return false;
    }
    if (c.accept("def")) {
        if (kind == QLatin1String("string")) o.text = string(c);
        P.objects.push_back(o);
        m_haveObject = false;
        return true;
    }
    if (c.isNumberLike()) {
        if (kind == QLatin1String("string")) {
            const double x = number(c), y = number(c);
            o.pos = QPointF(x, y);
        } else {
            const double a = number(c), bb = number(c), cc = number(c), d = number(c);
            o.p1 = QPointF(a, bb);
            o.p2 = QPointF(cc, d);
        }
        return true;
    }
    return false;
}

bool Interpreter::regionProps(Cursor& c, Region& r)
{
    bool b;
    if (parseOnOff(c, &r.active)) return true;
    if (c.accept("type")) {
        const QString t = c.word();
        ++c.i;
        r.negate = false;
        if (t == QLatin1String("above")) r.type = RegionType::Above;
        else if (t == QLatin1String("below")) r.type = RegionType::Below;
        else if (t == QLatin1String("left")) r.type = RegionType::Left;
        else if (t == QLatin1String("right")) r.type = RegionType::Right;
        else if (t == QLatin1String("polyi")) r.type = RegionType::Polygon;
        else if (t == QLatin1String("polyo")) { r.type = RegionType::Polygon; r.negate = true; }
        else if (t == QLatin1String("horizi")) r.type = RegionType::Horizontal;
        else if (t == QLatin1String("horizo")) { r.type = RegionType::Horizontal; r.negate = true; }
        else if (t == QLatin1String("verti")) r.type = RegionType::Vertical;
        else if (t == QLatin1String("verto")) { r.type = RegionType::Vertical; r.negate = true; }
        return true;
    }
    if (c.accept("linestyle")) { r.line.style = int(number(c)); return true; }
    if (c.accept("linewidth")) { r.line.width = number(c); return true; }
    if (c.accept("color")) { parseColor(c, &r.line.color); return true; }
    if (c.accept("line")) {
        const double a = number(c), bb = number(c), cc = number(c), d = number(c);
        r.p1 = QPointF(a, bb);
        r.p2 = QPointF(cc, d);
        return true;
    }
    if (c.accept("xy")) {
        const double x = number(c), y = number(c);
        r.polygon.push_back(QPointF(x, y));
        return true;
    }
    if (c.accept("link") || c.accept("to")) {
        c.accept("to");
        int gi;
        if (resolveGraph(c.word(), &gi)) r.linkedGraph = gi;
        ++c.i;
        return true;
    }
    Q_UNUSED(b);
    return false;
}

// ====================================================================== set operations

bool Interpreter::setOperation(Cursor& c, const QString& op)
{
    int g1, s1, g2, s2;
    const QString a = c.word();
    ++c.i;
    c.accept("to");
    c.accept("and");
    const QString bref = c.word();
    ++c.i;
    if (!resolveSet(a, &g1, &s1)) {
        // graph operations: COPY G0 TO G1
        int ga, gb;
        if (resolveGraph(a, &ga) && resolveGraph(bref, &gb) && m_p->graph(ga)) {
            Graph src = *m_p->graph(ga);
            if (op == QLatin1String("swap")) {
                if (m_p->graph(gb)) std::swap(m_p->graphs[ga], m_p->graphs[gb]);
                return true;
            }
            m_p->ensureGraph(gb) = src;
            if (op == QLatin1String("move")) m_p->graphs[ga] = Graph();
            return true;
        }
        error(QStringLiteral("bad set %1").arg(a));
        return true;
    }
    if (!resolveSet(bref, &g2, &s2)) { error(QStringLiteral("bad set %1").arg(bref)); return true; }
    Graph* ga = m_p->graph(g1);
    if (!ga || !ga->set(s1)) { error(QStringLiteral("set %1 does not exist").arg(a)); return true; }
    const int saved = m_curGraph;
    m_curGraph = g2;
    Graph& gb = graph();
    m_curGraph = saved;
    ga = m_p->graph(g1);  // graph() may have reallocated
    if (op == QLatin1String("swap")) {
        gb.ensureSet(s2);
        std::swap(m_p->graphs[g1].sets[s1], m_p->graphs[g2].sets[s2]);
        return true;
    }
    const DataSet src = m_p->graphs[g1].sets[s1];
    gb.ensureSet(s2) = src;
    if (op == QLatin1String("move") && !(g1 == g2 && s1 == s2)) m_p->graphs[g1].sets[s1] = DataSet();
    return true;
}

// ====================================================================== data reading

bool Interpreter::readData(const QString& path, ReadMode mode, SetType type, int gi)
{
    QString full = path;
    if (path != QLatin1String("-") && QFileInfo(path).isRelative() && !workingDir.isEmpty()) full = QDir(workingDir).filePath(path);
    QString text, err;
    if (!datareader::readFile(full, &text, &err)) {
        error(err);
        return false;
    }
    return readDataText(text, mode, type, gi, QFileInfo(full).fileName());
}

bool Interpreter::readDataText(const QString& text, ReadMode mode, SetType type, int gi, const QString& sourceName)
{
    if (gi < 0) gi = m_curGraph;
    const int saved = m_curGraph;
    m_curGraph = gi;
    graph();
    QStringList commands;
    const QVector<DataTable> tables = datareader::parseText(text, &commands, m_p->referenceDate);
    // Set numbers in a data file (e.g. GROMACS '@ s1 legend "Angle"') refer to the sets that file
    // creates; shift them past the sets already holding data so the file can be added to a graph.
    int base = 0;
    {
        const Graph& g = m_p->graphs[gi];
        for (int i = 0; i < g.sets.size(); ++i)
            if (g.sets[i].active && g.sets[i].size() > 0) base = i + 1;
    }
    // GROMACS files carry an xmgrace page layout (view, legend placement); in the modern styles
    // the automatic layout looks better, so that is kept unless the Grace look is in use.
    const bool gromacs = sourceName.endsWith(QLatin1String(".xvg"), Qt::CaseInsensitive) ||
                         text.left(2048).contains(QLatin1String("GROMACS"));
    const bool keepLayout = !gromacs || m_p->stylePreset == QLatin1String("Grace Classic");
    static const QRegularExpression setNumRe(QStringLiteral("^\\s*s(\\d+)\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression layoutRe(
        QStringLiteral("^\\s*(view\\b|legend\\s+(loctype|box|length|vgap|hgap|char|font|color|[-+.0-9]))"),
        QRegularExpression::CaseInsensitiveOption);
    m_typeSeen = false;
    for (QString cmd : commands) {
        if (!keepLayout && layoutRe.match(cmd).hasMatch()) continue;
        if (base > 0) {
            const auto m = setNumRe.match(cmd);
            if (m.hasMatch()) cmd.replace(m.capturedStart(1), m.capturedLength(1), QString::number(m.captured(1).toInt() + base));
        }
        execute(cmd);
    }
    const bool fileTyped = m_typeSeen && fileTypeOverrides && mode == ReadMode::Single;
    if (fileTyped) type = m_nextSetType;
    if (m_typeSeen) {
        // TYPE applies to this file only
        m_nextSetType = SetType::XY;
        m_typeSeen = false;
    }
    int created = 0;
    for (const DataTable& t : tables) {
        if (mode == ReadMode::Block) {
            m_block = t.cols;
            m_blockStrings = t.strings;
            continue;
        }
        if (mode == ReadMode::NXY) {
            for (int c = 1; c < t.columns(); ++c) {
                // fill sets the file's own commands prepared (e.g. '@ s1 legend ...') first
                Graph& g = m_p->graphs[gi];
                const int si = g.nextEmptySet();
                const bool existed = g.set(si) != nullptr;
                DataSet& s = g.ensureSet(si);
                if (!existed) presets::styleSet(s, si, m_p->stylePreset);
                s.active = true;
                s.type = SetType::XY;
                s.cols[0] = t.cols[0];
                s.cols[1] = t.cols[c];
                s.comment = sourceName;
                m_lastSet = si;
                if (s.legend.isEmpty() && t.headers.size() == t.columns()) s.legend = t.headers[c];
                ++created;
            }
            continue;
        }
        // single set of the given type; with a TYPE line in the file, X followed by several
        // column groups (X Y1 Y2 ..., or X Y1 dY1 Y2 dY2 ...) gives one set per group
        const int ncols = setTypeColumns(type);
        int groups = 1;
        if (fileTyped && ncols > 1 && t.columns() > ncols && (t.columns() - 1) % (ncols - 1) == 0)
            groups = (t.columns() - 1) / (ncols - 1);
        for (int grp = 0; grp < groups; ++grp) {
            Graph& g = m_p->graphs[gi];
            const int si = g.nextEmptySet();
            const bool existed = g.set(si) != nullptr;
            DataSet& s = g.ensureSet(si);
            if (!existed) presets::styleSet(s, si, m_p->stylePreset);
            s.active = true;
            s.type = type;
            s.comment = sourceName;
            if (t.columns() == 1) {
                QVector<double> idx(t.rows());
                for (int i = 0; i < idx.size(); ++i) idx[i] = i;
                s.cols[0] = idx;
                s.cols[1] = t.cols[0];
            } else {
                for (int c = 0; c < ncols; ++c) {
                    const int src = c == 0 ? 0 : 1 + grp * (ncols - 1) + (c - 1);
                    s.cols[c] = src < t.columns() ? t.cols[src] : QVector<double>(t.rows(), 0.0);
                }
            }
            if (t.hasStrings()) s.strings = t.strings;
            // a header line ("# time counts error") names the set after its Y column
            if (s.legend.isEmpty() && t.columns() >= 2 && t.headers.size() == t.columns())
                s.legend = t.headers[1 + grp * (ncols - 1)];
            m_lastSet = si;
            ++created;
        }
    }
    if (mode == ReadMode::Block && m_block.isEmpty()) error(QStringLiteral("no block data in %1").arg(sourceName));
    if (created > 0) applyAutoscaleOnRead(gi);
    m_curGraph = saved;
    if (created == 0 && mode != ReadMode::Block && tables.isEmpty()) {
        error(QStringLiteral("no data found in %1").arg(sourceName));
        return false;
    }
    return true;
}

bool Interpreter::formBlockSet(SetType type, const QString& colspec, int gi)
{
    if (m_block.isEmpty()) {
        error(QStringLiteral("no block data available"));
        return false;
    }
    if (gi < 0) gi = m_curGraph;
    const QStringList parts = colspec.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    const int ncols = setTypeColumns(type);
    QVector<QVector<double>> cols;
    QStringList strings;
    for (const QString& p : parts) {
        const QString t = p.trimmed();
        if (t.compare(QLatin1String("{s}"), Qt::CaseInsensitive) == 0 || t == QLatin1String("s")) {
            strings = m_blockStrings;
            continue;
        }
        bool ok = false;
        int c = t.toInt(&ok);
        if (!ok || c < 0 || c >= m_block.size()) {
            error(QStringLiteral("bad block column '%1'").arg(t));
            return false;
        }
        cols.push_back(m_block[c]);
    }
    if (cols.size() == 1) {
        // single column: index as x
        QVector<double> idx(cols[0].size());
        for (int i = 0; i < idx.size(); ++i) idx[i] = i;
        cols.prepend(idx);
    }
    const int saved = m_curGraph;
    m_curGraph = gi;
    Graph& g = graph();
    m_curGraph = saved;
    const int si = g.nextFreeSet();
    DataSet& s = g.ensureSet(si);
    presets::styleSet(s, si, m_p->stylePreset);
    s.type = type;
    for (int c = 0; c < ncols; ++c) s.cols[c] = c < cols.size() ? cols[c] : QVector<double>(cols[0].size(), 0.0);
    s.strings = strings;
    s.comment = QStringLiteral("block columns %1").arg(colspec);
    m_lastSet = si;
    applyAutoscaleOnRead(gi);
    return true;
}

// ====================================================================== transformations (function syntax)

bool Interpreter::functionCommand(const QString& name, const QStringList& args)
{
    auto setArg = [&](int i, int* g, int* s) -> DataSet* {
        if (i >= args.size()) return nullptr;
        int gg, ss;
        if (!resolveSet(args[i], &gg, &ss)) return nullptr;
        if (g) *g = gg;
        if (s) *s = ss;
        Graph* gr = m_p->graph(gg);
        return gr ? gr->set(ss) : nullptr;
    };
    auto num = [&](int i, double def) {
        if (i >= args.size()) return def;
        expr::Value v;
        QString err;
        if (!evaluate(args[i], &v, &err)) { error(err); return def; }
        return v.scalar();
    };
    auto onoff = [&](int i, bool def) {
        if (i >= args.size()) return def;
        const QString a = args[i].trimmed().toLower();
        if (a == QLatin1String("on") || a == QLatin1String("true") || a == QLatin1String("1")) return true;
        if (a == QLatin1String("off") || a == QLatin1String("false") || a == QLatin1String("0")) return false;
        return def;
    };
    auto vecArg = [&](int i, int g, int s) {
        expr::Value v;
        QString err;
        if (i >= args.size() || !evaluate(args[i], &v, &err, g, s)) {
            if (!err.isEmpty()) error(err);
            return QVector<double>();
        }
        return v.isVector ? v.v : QVector<double>{v.s};
    };
    auto result = [&](int g, const transforms::XY& xy, const QString& comment) {
        if (xy.x.isEmpty()) { error(QStringLiteral("%1: the transformation produced no points").arg(name)); return; }
        newSet(g, xy.x, xy.y, comment);
    };
    const QString n = name.toLower();
    int g = m_curGraph, s = 0;

    if (n == QLatin1String("runavg") || n == QLatin1String("runmed") || n == QLatin1String("runmin") ||
        n == QLatin1String("runmax") || n == QLatin1String("runstd")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("%1: bad set").arg(name)); return true; }
        const transforms::RunStat st = n == QLatin1String("runavg") ? transforms::RunStat::Average
                                     : n == QLatin1String("runmed") ? transforms::RunStat::Median
                                     : n == QLatin1String("runmin") ? transforms::RunStat::Min
                                     : n == QLatin1String("runmax") ? transforms::RunStat::Max
                                                                    : transforms::RunStat::StdDev;
        const int w = int(num(1, 3));
        result(g, transforms::running(d->x(), d->y(), w, st), QStringLiteral("%1 of S%2 (%3 pts)").arg(name.toUpper()).arg(s).arg(w));
        return true;
    }
    if (n == QLatin1String("interpolate")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("INTERPOLATE: bad set")); return true; }
        const QVector<double> mesh = vecArg(1, g, s);
        const QString m = args.value(2).trimmed().toLower();
        const transforms::InterpMethod im = m == QLatin1String("spline") ? transforms::InterpMethod::Spline
                                           : m == QLatin1String("aspline") ? transforms::InterpMethod::Akima
                                                                           : transforms::InterpMethod::Linear;
        result(g, transforms::interpolate(d->x(), d->y(), mesh, im, onoff(3, false)), QStringLiteral("Interpolation of S%1").arg(s));
        return true;
    }
    if (n == QLatin1String("histogram")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("HISTOGRAM: bad set")); return true; }
        const QVector<double> bins = vecArg(1, g, s);
        result(g, transforms::histogram(d->y(), bins, onoff(2, false), onoff(3, false)), QStringLiteral("Histogram of S%1").arg(s));
        return true;
    }
    if (n == QLatin1String("integrate")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("INTEGRATE: bad set")); return true; }
        double total = 0;
        result(g, transforms::integrate(d->x(), d->y(), &total), QStringLiteral("Integral of S%1").arg(s));
        info(QStringLiteral("Integral of S%1 = %2").arg(s).arg(total, 0, 'g', 12));
        return true;
    }
    if (n == QLatin1String("xcor")) {
        int g2, s2;
        DataSet* a = setArg(0, &g, &s);
        DataSet* b = setArg(1, &g2, &s2);
        if (!a || !b) { error(QStringLiteral("XCOR: bad set")); return true; }
        result(g, transforms::crossCorrelation(a->y(), b->y(), int(num(2, a->size() - 1)), onoff(3, false)),
               QStringLiteral("Cross-correlation of S%1 and S%2").arg(s).arg(s2));
        return true;
    }
    if (n == QLatin1String("linconv")) {
        int g2, s2;
        DataSet* a = setArg(0, &g, &s);
        DataSet* b = setArg(1, &g2, &s2);
        if (!a || !b) { error(QStringLiteral("LINCONV: bad set")); return true; }
        result(g, transforms::linearConvolution(a->y(), b->y()), QStringLiteral("Convolution of S%1 and S%2").arg(s).arg(s2));
        return true;
    }
    if (n == QLatin1String("restrict")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("RESTRICT: bad set")); return true; }
        QVector<bool> keep(d->size(), true);
        static const QRegularExpression regRe(QStringLiteral("^r([0-4])$"), QRegularExpression::CaseInsensitiveOption);
        const auto rm = regRe.match(args.value(1).trimmed());
        if (rm.hasMatch()) {
            const Region& r = m_p->regions[rm.captured(1).toInt()];
            const bool neg = onoff(2, false);
            for (int i = 0; i < d->size(); ++i) keep[i] = r.contains(d->x()[i], d->y()[i]) != neg;
        } else {
            const QVector<double> cond = vecArg(1, g, s);
            for (int i = 0; i < d->size(); ++i) keep[i] = cond.value(i, cond.isEmpty() ? 1.0 : cond.last()) != 0.0;
        }
        const int n = d->size();  // fixed before the columns shrink
        for (int c = 0; c < d->columns(); ++c) {
            QVector<double> nv;
            for (int i = 0; i < n; ++i) if (keep[i]) nv.push_back(d->cols[c].value(i));
            d->cols[c] = nv;
        }
        if (!d->strings.isEmpty()) {
            QStringList ns;
            for (int i = 0; i < keep.size(); ++i) if (keep[i]) ns << d->strings.value(i);
            d->strings = ns;
        }
        return true;
    }
    if (n == QLatin1String("difference") || n == QLatin1String("diff") || n == QLatin1String("derivative")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("DIFFERENCE: bad set")); return true; }
        const QString t = args.value(1).trimmed().toLower();
        const transforms::DiffType dt = t == QLatin1String("backward") ? transforms::DiffType::Backward
                                      : t == QLatin1String("centered") || t == QLatin1String("centred") ? transforms::DiffType::Centered
                                                                                                        : transforms::DiffType::Forward;
        result(g, transforms::differentiate(d->x(), d->y(), dt), QStringLiteral("Derivative of S%1").arg(s));
        return true;
    }
    if (n == QLatin1String("seasonaldiff")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("SEASONALDIFF: bad set")); return true; }
        result(g, transforms::seasonalDifference(d->x(), d->y(), int(num(1, 1))), QStringLiteral("Seasonal difference of S%1").arg(s));
        return true;
    }
    if (n == QLatin1String("fft") || n == QLatin1String("dft") || n == QLatin1String("invfft") || n == QLatin1String("invdft")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("FFT: bad set")); return true; }
        transforms::FftOptions o;
        o.inverse = n.startsWith(QLatin1String("inv"));
        const QString out = args.value(1).trimmed().toLower();
        if (out == QLatin1String("phase")) o.output = transforms::FftOutput::Phase;
        else if (out == QLatin1String("power")) o.output = transforms::FftOutput::Power;
        else if (out == QLatin1String("real")) o.output = transforms::FftOutput::Real;
        else if (out == QLatin1String("imag") || out == QLatin1String("imaginary")) o.output = transforms::FftOutput::Imaginary;
        else if (out == QLatin1String("db")) o.output = transforms::FftOutput::DB;
        result(g, transforms::fourier(d->x(), d->y(), o), QStringLiteral("FFT of S%1").arg(s));
        return true;
    }
    if (n == QLatin1String("regress")) {
        DataSet* d = setArg(0, &g, &s);
        if (!d) { error(QStringLiteral("REGRESS: bad set")); return true; }
        const int deg = int(num(1, 1));
        const fit::RegressionResult r = fit::regression(d->x(), d->y(), deg == 1 ? fit::Regression::Linear : fit::Regression::Polynomial, deg);
        if (!r.ok) { error(r.message); return true; }
        info(r.report());
        QVector<double> xs = d->x();
        result(g, transforms::XY{xs, fit::evaluateRegression(r, deg == 1 ? fit::Regression::Linear : fit::Regression::Polynomial, xs)},
               QStringLiteral("Regression of S%1: y = %2").arg(s).arg(r.formula));
        return true;
    }
    if (n == QLatin1String("nonlfit")) {
        if (!setArg(0, &g, &s)) { error(QStringLiteral("NONLFIT: bad set")); return true; }
        runNonlinearFit(g, s, int(num(1, m_fit.maxIterations)));
        return true;
    }
    if (n == QLatin1String("arrange")) {
        const int r = int(num(0, 1)), cc = int(num(1, 1));
        arrangeGraphs(*m_p, r, cc, num(2, 0.1), num(3, 0.15), num(4, 0.15), onoff(5, false), onoff(6, false), onoff(7, false),
                      onoff(8, false));
        return true;
    }
    if (n == QLatin1String("exit")) {
        if (exitHandler) exitHandler(int(num(0, 0)));
        return true;
    }
    if (n == QLatin1String("sleep")) {
        const double sec = num(0, 0);
        if (sec > 0 && sec < 3600) QThread::msleep(static_cast<unsigned long>(sec * 1000));
        return true;
    }
    return false;
}

// ====================================================================== fitting

bool Interpreter::fitCommand(Cursor& c)
{
    if (c.accept("nonlfit")) {
        // "nonlfit (s0, 20)" handled by functionCommand; "nonlfit s0 20" here
        int g, s;
        if (resolveSet(c.word(), &g, &s)) {
            ++c.i;
            c.acceptPunct(",");
            const int it = c.atEnd() ? m_fit.maxIterations : int(number(c));
            runNonlinearFit(g, s, it);
            return true;
        }
        return false;
    }
    c.accept("fit");
    if (c.accept("formula")) { m_fit.formula = string(c); return true; }
    if (c.accept("with")) {
        m_fit.nparams = std::clamp(int(number(c)), 1, fit::kMaxParams);
        c.accept("parameters");
        return true;
    }
    if (c.accept("prec")) { m_fit.tolerance = number(c); return true; }
    if (c.accept("weight") || c.accept("weights")) {
        const QString w = c.word();
        ++c.i;
        if (w == QLatin1String("none")) m_fit.weighting = fit::Weighting::None;
        else if (w == QLatin1String("y")) m_fit.weighting = fit::Weighting::InverseY;
        else if (w == QLatin1String("y2")) m_fit.weighting = fit::Weighting::InverseY2;
        else if (w == QLatin1String("dy")) m_fit.weighting = fit::Weighting::InverseDY2;
        return true;
    }
    return false;
}

bool Interpreter::runNonlinearFit(int g, int s, int iterations)
{
    Graph* gr = m_p->graph(g);
    DataSet* d = gr ? gr->set(s) : nullptr;
    if (!d) { error(QStringLiteral("fit: no such set")); return false; }
    fit::Options o = m_fit;
    o.maxIterations = std::max(1, iterations);
    for (int k = 0; k < o.nparams; ++k) {
        expr::Value v;
        if (m_vars.lookup(QStringLiteral("a%1").arg(k), &v)) o.params[k].value = v.scalar();
    }
    QVector<double> dy;
    if (d->type == SetType::XYDY) dy = d->cols[2];
    const fit::Result r = fit::nonlinear(d->x(), d->y(), dy, o);
    if (!r.ok) { error(r.message); return false; }
    for (int k = 0; k < o.nparams; ++k) {
        m_fit.params[k].value = r.values[k];
        m_vars.setScalar(QStringLiteral("a%1").arg(k), r.values[k]);
    }
    info(r.report(o.formula));
    // fitted curve on a fine mesh over the data range
    double x0 = HUGE_VAL, x1 = -HUGE_VAL;
    for (double v : d->x()) if (std::isfinite(v)) { x0 = std::min(x0, v); x1 = std::max(x1, v); }
    const QVector<double> xs = transforms::linspace(x0, x1, std::max(200, d->size()));
    const QVector<double> ys = fit::evaluate(o.formula, xs, r.values);
    newSet(g, xs, ys, QStringLiteral("Fit of S%1: %2").arg(s).arg(o.formula));
    return true;
}

}  // namespace cactus
