// SPDX-License-Identifier: GPL-2.0-or-later
#include "math/Expr.h"

#include <QSet>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "math/SpecialFunctions.h"

namespace cactus::expr {

// ------------------------------------------------------------------ Value

QVector<double> Value::toVector(int n) const
{
    if (isVector) return v;
    return QVector<double>(n, s);
}

bool Context::lookup(const QString& lowerName, Value* out) const
{
    auto it = m_vars.constFind(lowerName);
    if (it != m_vars.constEnd()) {
        *out = *it;
        return true;
    }
    if (m_resolver) return m_resolver(lowerName, out);
    return false;
}

// ------------------------------------------------------------------ random

namespace {
std::mt19937_64& rng()
{
    static std::mt19937_64 g(12345u);
    return g;
}
}  // namespace

void seedRandom(quint64 seed) { rng().seed(seed); }
double randomUniform()
{
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(rng());
}
double randomNormal(double mean, double sigma)
{
    std::normal_distribution<double> d(mean, sigma > 0 ? sigma : 1e-300);
    return d(rng());
}

// ------------------------------------------------------------------ AST

enum class NodeKind { Number, Ident, Unary, Binary, Ternary, Call, Index };

enum Op {
    OpAdd, OpSub, OpMul, OpDiv, OpMod, OpPow,
    OpEq, OpNe, OpLt, OpLe, OpGt, OpGe,
    OpAnd, OpOr, OpNot, OpNeg, OpPlus
};

struct Node {
    NodeKind kind = NodeKind::Number;
    double num = 0.0;
    QString name;
    int op = 0;
    std::vector<std::unique_ptr<Node>> kids;
};

namespace {

// ------------------------------------------------------------------ lexer

enum class Tok { End, Number, Ident, Op, LParen, RParen, LBracket, RBracket, Comma, Question, Colon };

struct Token {
    Tok type = Tok::End;
    double num = 0.0;
    QString text;
    int pos = 0;
};

class Lexer {
public:
    explicit Lexer(const QString& s) : m_s(s) {}

    bool tokenize(std::vector<Token>* out, QString* err)
    {
        int i = 0;
        const int n = int(m_s.size());
        while (i < n) {
            const QChar c = m_s[i];
            if (c.isSpace()) { ++i; continue; }
            Token t;
            t.pos = i;
            if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && m_s[i + 1].isDigit())) {
                int j = i;
                while (j < n && (m_s[j].isDigit() || m_s[j] == QLatin1Char('.'))) ++j;
                if (j < n && (m_s[j] == QLatin1Char('e') || m_s[j] == QLatin1Char('E'))) {
                    int k = j + 1;
                    if (k < n && (m_s[k] == QLatin1Char('+') || m_s[k] == QLatin1Char('-'))) ++k;
                    if (k < n && m_s[k].isDigit()) {
                        j = k;
                        while (j < n && m_s[j].isDigit()) ++j;
                    }
                }
                bool ok = false;
                t.type = Tok::Number;
                t.num = m_s.mid(i, j - i).toDouble(&ok);
                if (!ok) { *err = QStringLiteral("bad number at %1").arg(i); return false; }
                i = j;
            } else if (c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$')) {
                int j = i;
                while (j < n && (m_s[j].isLetterOrNumber() || m_s[j] == QLatin1Char('_') || m_s[j] == QLatin1Char('$') ||
                                 (m_s[j] == QLatin1Char('.') && j + 1 < n && (m_s[j + 1].isLetter() || m_s[j+1] == QLatin1Char('$')))))
                    ++j;
                t.type = Tok::Ident;
                t.text = m_s.mid(i, j - i).toLower();
                i = j;
                // keyword operators
                static const QHash<QString, QString> kw = {
                    {QStringLiteral("and"), QStringLiteral("&&")}, {QStringLiteral("or"), QStringLiteral("||")},
                    {QStringLiteral("not"), QStringLiteral("!")},  {QStringLiteral("eq"), QStringLiteral("==")},
                    {QStringLiteral("ne"), QStringLiteral("!=")},  {QStringLiteral("lt"), QStringLiteral("<")},
                    {QStringLiteral("le"), QStringLiteral("<=")},  {QStringLiteral("gt"), QStringLiteral(">")},
                    {QStringLiteral("ge"), QStringLiteral(">=")}};
                auto it = kw.constFind(t.text);
                if (it != kw.constEnd()) {
                    t.type = Tok::Op;
                    t.text = *it;
                }
            } else {
                ++i;
                switch (c.unicode()) {
                case '(': t.type = Tok::LParen; break;
                case ')': t.type = Tok::RParen; break;
                case '[': t.type = Tok::LBracket; break;
                case ']': t.type = Tok::RBracket; break;
                case ',': t.type = Tok::Comma; break;
                case '?': t.type = Tok::Question; break;
                case ':': t.type = Tok::Colon; break;
                case '+': case '-': case '/': case '%': case '^':
                    t.type = Tok::Op; t.text = QString(c); break;
                case '*':
                    t.type = Tok::Op;
                    if (i < n && m_s[i] == QLatin1Char('*')) { t.text = QStringLiteral("^"); ++i; }
                    else t.text = QStringLiteral("*");
                    break;
                case '=': case '!': case '<': case '>':
                    t.type = Tok::Op;
                    t.text = QString(c);
                    if (i < n && m_s[i] == QLatin1Char('=')) { t.text += QLatin1Char('='); ++i; }
                    else if (c == QLatin1Char('=')) { t.text = QStringLiteral("=="); }
                    break;
                case '&':
                    if (i < n && m_s[i] == QLatin1Char('&')) ++i;
                    t.type = Tok::Op; t.text = QStringLiteral("&&"); break;
                case '|':
                    if (i < n && m_s[i] == QLatin1Char('|')) ++i;
                    t.type = Tok::Op; t.text = QStringLiteral("||"); break;
                default:
                    *err = QStringLiteral("unexpected character '%1' at %2").arg(c).arg(i - 1);
                    return false;
                }
            }
            out->push_back(t);
        }
        Token e;
        e.type = Tok::End;
        e.pos = n;
        out->push_back(e);
        return true;
    }

private:
    QString m_s;
};

// ------------------------------------------------------------------ parser

class Parser {
public:
    Parser(std::vector<Token> toks) : m_t(std::move(toks)) {}

    std::unique_ptr<Node> parse(QString* err)
    {
        auto n = ternary();
        if (!n) { *err = m_err; return nullptr; }
        if (peek().type != Tok::End) {
            *err = QStringLiteral("unexpected token at position %1").arg(peek().pos);
            return nullptr;
        }
        return n;
    }

private:
    const Token& peek() const { return m_t[m_i]; }
    Token next() { return m_t[m_i++]; }
    bool isOp(const char* s) const { return peek().type == Tok::Op && peek().text == QLatin1String(s); }
    std::unique_ptr<Node> fail(const QString& msg)
    {
        if (m_err.isEmpty()) m_err = msg;
        return nullptr;
    }

    static std::unique_ptr<Node> bin(int op, std::unique_ptr<Node> a, std::unique_ptr<Node> b)
    {
        auto n = std::make_unique<Node>();
        n->kind = NodeKind::Binary;
        n->op = op;
        n->kids.push_back(std::move(a));
        n->kids.push_back(std::move(b));
        return n;
    }

    std::unique_ptr<Node> ternary()
    {
        auto c = orExpr();
        if (!c) return nullptr;
        if (peek().type == Tok::Question) {
            next();
            auto a = ternary();
            if (!a) return nullptr;
            if (peek().type != Tok::Colon) return fail(QStringLiteral("expected ':' in ternary expression"));
            next();
            auto b = ternary();
            if (!b) return nullptr;
            auto n = std::make_unique<Node>();
            n->kind = NodeKind::Ternary;
            n->kids.push_back(std::move(c));
            n->kids.push_back(std::move(a));
            n->kids.push_back(std::move(b));
            return n;
        }
        return c;
    }
    std::unique_ptr<Node> orExpr()
    {
        auto a = andExpr();
        while (a && isOp("||")) { next(); auto b = andExpr(); if (!b) return nullptr; a = bin(OpOr, std::move(a), std::move(b)); }
        return a;
    }
    std::unique_ptr<Node> andExpr()
    {
        auto a = eqExpr();
        while (a && isOp("&&")) { next(); auto b = eqExpr(); if (!b) return nullptr; a = bin(OpAnd, std::move(a), std::move(b)); }
        return a;
    }
    std::unique_ptr<Node> eqExpr()
    {
        auto a = relExpr();
        while (a && (isOp("==") || isOp("!="))) {
            const int op = isOp("==") ? OpEq : OpNe;
            next();
            auto b = relExpr();
            if (!b) return nullptr;
            a = bin(op, std::move(a), std::move(b));
        }
        return a;
    }
    std::unique_ptr<Node> relExpr()
    {
        auto a = addExpr();
        while (a && (isOp("<") || isOp("<=") || isOp(">") || isOp(">="))) {
            int op = OpLt;
            if (isOp("<=")) op = OpLe; else if (isOp(">")) op = OpGt; else if (isOp(">=")) op = OpGe;
            next();
            auto b = addExpr();
            if (!b) return nullptr;
            a = bin(op, std::move(a), std::move(b));
        }
        return a;
    }
    std::unique_ptr<Node> addExpr()
    {
        auto a = mulExpr();
        while (a && (isOp("+") || isOp("-"))) {
            const int op = isOp("+") ? OpAdd : OpSub;
            next();
            auto b = mulExpr();
            if (!b) return nullptr;
            a = bin(op, std::move(a), std::move(b));
        }
        return a;
    }
    std::unique_ptr<Node> mulExpr()
    {
        auto a = unary();
        while (a && (isOp("*") || isOp("/") || isOp("%"))) {
            int op = OpMul;
            if (isOp("/")) op = OpDiv; else if (isOp("%")) op = OpMod;
            next();
            auto b = unary();
            if (!b) return nullptr;
            a = bin(op, std::move(a), std::move(b));
        }
        return a;
    }
    std::unique_ptr<Node> unary()
    {
        if (isOp("-") || isOp("+") || isOp("!")) {
            const int op = isOp("-") ? OpNeg : (isOp("+") ? OpPlus : OpNot);
            next();
            auto a = unary();
            if (!a) return nullptr;
            auto n = std::make_unique<Node>();
            n->kind = NodeKind::Unary;
            n->op = op;
            n->kids.push_back(std::move(a));
            return n;
        }
        return power();
    }
    std::unique_ptr<Node> power()
    {
        auto a = postfix();
        if (a && isOp("^")) {
            next();
            auto b = unary();  // right associative, allows 2^-1
            if (!b) return nullptr;
            return bin(OpPow, std::move(a), std::move(b));
        }
        return a;
    }
    std::unique_ptr<Node> postfix()
    {
        auto a = primary();
        while (a && peek().type == Tok::LBracket) {
            next();
            auto idx = ternary();
            if (!idx) return nullptr;
            if (peek().type != Tok::RBracket) return fail(QStringLiteral("expected ']'"));
            next();
            auto n = std::make_unique<Node>();
            n->kind = NodeKind::Index;
            n->kids.push_back(std::move(a));
            n->kids.push_back(std::move(idx));
            a = std::move(n);
        }
        return a;
    }
    std::unique_ptr<Node> primary()
    {
        const Token& t = peek();
        if (t.type == Tok::Number) {
            next();
            auto n = std::make_unique<Node>();
            n->kind = NodeKind::Number;
            n->num = t.num;
            return n;
        }
        if (t.type == Tok::LParen) {
            next();
            auto n = ternary();
            if (!n) return nullptr;
            if (peek().type != Tok::RParen) return fail(QStringLiteral("expected ')'"));
            next();
            return n;
        }
        if (t.type == Tok::Ident) {
            Token id = next();
            if (peek().type == Tok::LParen) {
                next();
                auto n = std::make_unique<Node>();
                n->kind = NodeKind::Call;
                n->name = id.text;
                if (peek().type != Tok::RParen) {
                    while (true) {
                        auto a = ternary();
                        if (!a) return nullptr;
                        n->kids.push_back(std::move(a));
                        if (peek().type == Tok::Comma) { next(); continue; }
                        break;
                    }
                }
                if (peek().type != Tok::RParen) return fail(QStringLiteral("expected ')' after arguments of %1").arg(id.text));
                next();
                return n;
            }
            auto n = std::make_unique<Node>();
            n->kind = NodeKind::Ident;
            n->name = id.text;
            return n;
        }
        if (t.type == Tok::End) return fail(QStringLiteral("unexpected end of expression"));
        return fail(QStringLiteral("unexpected token at position %1").arg(t.pos));
    }

    std::vector<Token> m_t;
    size_t m_i = 0;
    QString m_err;
};

// ------------------------------------------------------------------ functions

using Args = std::vector<Value>;
using Fn = std::function<Value(const Args&, QString*)>;

struct FuncDef {
    int minArgs;
    int maxArgs;
    Fn fn;
};

int broadcastSize(const Args& a)
{
    int n = 1;
    bool any = false;
    for (const Value& v : a) {
        if (v.isVector) {
            n = any ? std::min(n, v.size()) : v.size();
            any = true;
        }
    }
    return any ? n : -1;
}

template <class F>
Value mapN(const Args& a, F f)
{
    const int n = broadcastSize(a);
    if (n < 0) {
        double xs[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < a.size() && i < 4; ++i) xs[i] = a[i].s;
        return Value(f(xs));
    }
    QVector<double> out(n);
    double xs[4] = {0, 0, 0, 0};
    for (int k = 0; k < n; ++k) {
        for (size_t i = 0; i < a.size() && i < 4; ++i) xs[i] = a[i].at(k);
        out[k] = f(xs);
    }
    return Value(out);
}

FuncDef f1(double (*fn)(double))
{
    return {1, 1, [fn](const Args& a, QString*) { return mapN(a, [fn](const double* x) { return fn(x[0]); }); }};
}
FuncDef f2(double (*fn)(double, double))
{
    return {2, 2, [fn](const Args& a, QString*) { return mapN(a, [fn](const double* x) { return fn(x[0], x[1]); }); }};
}
FuncDef f3(double (*fn)(double, double, double))
{
    return {3, 3, [fn](const Args& a, QString*) { return mapN(a, [fn](const double* x) { return fn(x[0], x[1], x[2]); }); }};
}
FuncDef f4(double (*fn)(double, double, double, double))
{
    return {4, 4, [fn](const Args& a, QString*) { return mapN(a, [fn](const double* x) { return fn(x[0], x[1], x[2], x[3]); }); }};
}

QVector<double> vec(const Value& v) { return v.isVector ? v.v : QVector<double>{v.s}; }

double sgn(double x) { return x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0); }
double sqr(double x) { return x * x; }
double fac(double n) { return n < 0 ? NAN : std::tgamma(std::floor(n) + 1.0); }
double rint_(double x) { return std::nearbyint(x); }
double maxof(double a, double b) { return std::max(a, b); }
double minof(double a, double b) { return std::min(a, b); }
double fmod_(double a, double b) { return std::fmod(a, b); }
double trunc_(double a) { return std::trunc(a); }
double deg(double a) { return a * 180.0 / M_PI; }
double rad(double a) { return a * M_PI / 180.0; }
double ln_(double a) { return std::log(a); }
double norm_(double x) { return std::exp(-0.5 * x * x) / std::sqrt(2.0 * M_PI); }
double heaviside(double x) { return x >= 0 ? 1.0 : 0.0; }
double gauss(double x, double x0, double s) { return std::exp(-0.5 * (x - x0) * (x - x0) / (s * s)) / (s * std::sqrt(2 * M_PI)); }
double lorentz(double x, double x0, double g) { return g / (M_PI * ((x - x0) * (x - x0) + g * g)); }

const QHash<QString, FuncDef>& functions()
{
    static const QHash<QString, FuncDef> table = [] {
        QHash<QString, FuncDef> t;
        // elementary
        t.insert(QStringLiteral("abs"), f1([](double x) { return std::fabs(x); }));
        t.insert(QStringLiteral("acos"), f1([](double x) { return std::acos(x); }));
        t.insert(QStringLiteral("acosh"), f1([](double x) { return std::acosh(x); }));
        t.insert(QStringLiteral("asin"), f1([](double x) { return std::asin(x); }));
        t.insert(QStringLiteral("asinh"), f1([](double x) { return std::asinh(x); }));
        t.insert(QStringLiteral("atan"), f1([](double x) { return std::atan(x); }));
        t.insert(QStringLiteral("atan2"), f2([](double y, double x) { return std::atan2(y, x); }));
        t.insert(QStringLiteral("atanh"), f1([](double x) { return std::atanh(x); }));
        t.insert(QStringLiteral("ceil"), f1([](double x) { return std::ceil(x); }));
        t.insert(QStringLiteral("cos"), f1([](double x) { return std::cos(x); }));
        t.insert(QStringLiteral("cosh"), f1([](double x) { return std::cosh(x); }));
        t.insert(QStringLiteral("exp"), f1([](double x) { return std::exp(x); }));
        t.insert(QStringLiteral("fac"), f1(fac));
        t.insert(QStringLiteral("floor"), f1([](double x) { return std::floor(x); }));
        t.insert(QStringLiteral("ln"), f1(ln_));
        t.insert(QStringLiteral("log"), f1(ln_));
        t.insert(QStringLiteral("log10"), f1([](double x) { return std::log10(x); }));
        t.insert(QStringLiteral("log2"), f1([](double x) { return std::log2(x); }));
        t.insert(QStringLiteral("maxof"), f2(maxof));
        t.insert(QStringLiteral("minof"), f2(minof));
        t.insert(QStringLiteral("mod"), f2(fmod_));
        t.insert(QStringLiteral("rint"), f1(rint_));
        t.insert(QStringLiteral("round"), f1([](double x) { return std::round(x); }));
        t.insert(QStringLiteral("trunc"), f1(trunc_));
        t.insert(QStringLiteral("sgn"), f1(sgn));
        t.insert(QStringLiteral("sign"), f1(sgn));
        t.insert(QStringLiteral("sin"), f1([](double x) { return std::sin(x); }));
        t.insert(QStringLiteral("sinh"), f1([](double x) { return std::sinh(x); }));
        t.insert(QStringLiteral("sqr"), f1(sqr));
        t.insert(QStringLiteral("sqrt"), f1([](double x) { return std::sqrt(x); }));
        t.insert(QStringLiteral("cbrt"), f1([](double x) { return std::cbrt(x); }));
        t.insert(QStringLiteral("tan"), f1([](double x) { return std::tan(x); }));
        t.insert(QStringLiteral("tanh"), f1([](double x) { return std::tanh(x); }));
        t.insert(QStringLiteral("hypot"), f2([](double a, double b) { return std::hypot(a, b); }));
        t.insert(QStringLiteral("pow"), f2([](double a, double b) { return std::pow(a, b); }));
        t.insert(QStringLiteral("deg"), f1(deg));
        t.insert(QStringLiteral("rad"), f1(rad));
        t.insert(QStringLiteral("heaviside"), f1(heaviside));
        t.insert(QStringLiteral("gauss"), f3(gauss));
        t.insert(QStringLiteral("lorentz"), f3(lorentz));
        t.insert(QStringLiteral("isnan"), f1([](double x) { return std::isnan(x) ? 1.0 : 0.0; }));
        // statistics
        t.insert(QStringLiteral("erf"), f1([](double x) { return std::erf(x); }));
        t.insert(QStringLiteral("erfc"), f1([](double x) { return std::erfc(x); }));
        t.insert(QStringLiteral("norm"), f1(norm_));
        t.insert(QStringLiteral("ndtr"), f1(special::ndtr));
        t.insert(QStringLiteral("ndtri"), f1(special::ndtri));
        t.insert(QStringLiteral("chdtr"), f2(special::chdtr));
        t.insert(QStringLiteral("chdtrc"), f2(special::chdtrc));
        t.insert(QStringLiteral("chdtri"), f2(special::chdtri));
        t.insert(QStringLiteral("fdtr"), f3(special::fdtr));
        t.insert(QStringLiteral("fdtrc"), f3(special::fdtrc));
        t.insert(QStringLiteral("fdtri"), f3(special::fdtri));
        t.insert(QStringLiteral("gdtr"), f3(special::gdtr));
        t.insert(QStringLiteral("gdtrc"), f3(special::gdtrc));
        t.insert(QStringLiteral("pdtr"), f2(special::pdtr));
        t.insert(QStringLiteral("pdtrc"), f2(special::pdtrc));
        t.insert(QStringLiteral("pdtri"), f2(special::pdtri));
        t.insert(QStringLiteral("stdtr"), f2(special::stdtr));
        t.insert(QStringLiteral("stdtri"), f2(special::stdtri));
        // special functions
        t.insert(QStringLiteral("ai"), f1(special::airyAi));
        t.insert(QStringLiteral("bi"), f1(special::airyBi));
        t.insert(QStringLiteral("beta"), f2(special::beta));
        t.insert(QStringLiteral("lbeta"), f2(special::lbeta));
        t.insert(QStringLiteral("chi"), f1(special::chi));
        t.insert(QStringLiteral("ci"), f1(special::ci));
        t.insert(QStringLiteral("shi"), f1(special::shi));
        t.insert(QStringLiteral("si"), f1(special::si));
        t.insert(QStringLiteral("dawsn"), f1(special::dawsn));
        t.insert(QStringLiteral("ellie"), f2(special::ellie));
        t.insert(QStringLiteral("ellik"), f2(special::ellik));
        t.insert(QStringLiteral("ellpe"), f1(special::ellpe));
        t.insert(QStringLiteral("ellpk"), f1(special::ellpk));
        t.insert(QStringLiteral("expn"), f2(special::expn));
        t.insert(QStringLiteral("fresnlc"), f1(special::fresnlc));
        t.insert(QStringLiteral("fresnls"), f1(special::fresnls));
        t.insert(QStringLiteral("gamma"), f1([](double x) { return std::tgamma(x); }));
        t.insert(QStringLiteral("lgamma"), f1([](double x) { return std::lgamma(x); }));
        t.insert(QStringLiteral("rgamma"), f1(special::rgamma));
        t.insert(QStringLiteral("psi"), f1(special::psi));
        t.insert(QStringLiteral("hyp2f1"), f4(special::hyp2f1));
        t.insert(QStringLiteral("hyperg"), f3(special::hyperg));
        t.insert(QStringLiteral("i0e"), f1(special::i0e));
        t.insert(QStringLiteral("i1e"), f1(special::i1e));
        t.insert(QStringLiteral("k0e"), f1(special::k0e));
        t.insert(QStringLiteral("k1e"), f1(special::k1e));
        t.insert(QStringLiteral("igam"), f2(special::igam));
        t.insert(QStringLiteral("igamc"), f2(special::igamc));
        t.insert(QStringLiteral("igami"), f2(special::igami));
        t.insert(QStringLiteral("incbet"), f3(special::incbet));
        t.insert(QStringLiteral("incbi"), f3(special::incbi));
        t.insert(QStringLiteral("iv"), f2(special::iv));
        t.insert(QStringLiteral("jv"), f2(special::jv));
        t.insert(QStringLiteral("yv"), f2(special::yv));
        t.insert(QStringLiteral("kn"), f2(special::kv));
        t.insert(QStringLiteral("kv"), f2(special::kv));
        t.insert(QStringLiteral("j0"), f1([](double x) { return special::jv(0, x); }));
        t.insert(QStringLiteral("j1"), f1([](double x) { return special::jv(1, x); }));
        t.insert(QStringLiteral("jn"), f2(special::jv));
        t.insert(QStringLiteral("y0"), f1([](double x) { return special::yv(0, x); }));
        t.insert(QStringLiteral("y1"), f1([](double x) { return special::yv(1, x); }));
        t.insert(QStringLiteral("yn"), f2(special::yv));
        t.insert(QStringLiteral("spence"), f1(special::spence));
        t.insert(QStringLiteral("struve"), f2(special::struve));
        t.insert(QStringLiteral("voigt"), f3(special::voigt));
        t.insert(QStringLiteral("zeta"), f2(special::zeta));
        t.insert(QStringLiteral("zetac"), f1(special::zetac));

        // random
        t.insert(QStringLiteral("rand"), {0, 1, [](const Args& a, QString*) -> Value {
                     if (a.empty()) return Value(randomUniform());
                     const int n = std::max(0, int(a[0].scalar()));
                     QVector<double> v(n);
                     for (double& d : v) d = randomUniform();
                     return Value(v);
                 }});
        t.insert(QStringLiteral("irand"), {1, 1, [](const Args& a, QString*) {
                     return mapN(a, [](const double* x) { return std::floor(randomUniform() * x[0]); });
                 }});
        t.insert(QStringLiteral("rnorm"), {2, 2, [](const Args& a, QString*) {
                     return mapN(a, [](const double* x) { return randomNormal(x[0], x[1]); });
                 }});
        // arrays
        t.insert(QStringLiteral("mesh"), {1, 3, [](const Args& a, QString* err) -> Value {
                     if (a.size() == 2) { *err = QStringLiteral("mesh() takes 1 or 3 arguments"); return Value(); }
                     if (a.size() == 1) {
                         const int n = std::max(0, int(a[0].scalar()));
                         QVector<double> v(n);
                         for (int i = 0; i < n; ++i) v[i] = i;
                         return Value(v);
                     }
                     const double x1 = a[0].scalar(), x2 = a[1].scalar();
                     const int n = std::max(0, int(a[2].scalar()));
                     QVector<double> v(n);
                     for (int i = 0; i < n; ++i) v[i] = n > 1 ? x1 + (x2 - x1) * i / (n - 1) : x1;
                     return Value(v);
                 }});
        t.insert(QStringLiteral("rsum"), {1, 1, [](const Args& a, QString*) -> Value {
                     QVector<double> v = vec(a[0]);
                     double s = 0;
                     for (double& d : v) { s += d; d = s; }
                     return Value(v);
                 }});
        t.insert(QStringLiteral("len"), {1, 1, [](const Args& a, QString*) { return Value(double(a[0].size())); }});
        t.insert(QStringLiteral("length"), {1, 1, [](const Args& a, QString*) { return Value(double(a[0].size())); }});
        // aggregates
        t.insert(QStringLiteral("min"), {1, 2, [](const Args& a, QString*) -> Value {
                     if (a.size() == 2) return mapN(a, [](const double* x) { return std::min(x[0], x[1]); });
                     const auto v = vec(a[0]);
                     return v.isEmpty() ? Value(NAN) : Value(*std::min_element(v.begin(), v.end()));
                 }});
        t.insert(QStringLiteral("max"), {1, 2, [](const Args& a, QString*) -> Value {
                     if (a.size() == 2) return mapN(a, [](const double* x) { return std::max(x[0], x[1]); });
                     const auto v = vec(a[0]);
                     return v.isEmpty() ? Value(NAN) : Value(*std::max_element(v.begin(), v.end()));
                 }});
        t.insert(QStringLiteral("imin"), {1, 1, [](const Args& a, QString*) -> Value {
                     const auto v = vec(a[0]);
                     return v.isEmpty() ? Value(NAN) : Value(double(std::min_element(v.begin(), v.end()) - v.begin()));
                 }});
        t.insert(QStringLiteral("imax"), {1, 1, [](const Args& a, QString*) -> Value {
                     const auto v = vec(a[0]);
                     return v.isEmpty() ? Value(NAN) : Value(double(std::max_element(v.begin(), v.end()) - v.begin()));
                 }});
        t.insert(QStringLiteral("sum"), {1, 1, [](const Args& a, QString*) {
                     const auto v = vec(a[0]);
                     return Value(std::accumulate(v.begin(), v.end(), 0.0));
                 }});
        t.insert(QStringLiteral("avg"), {1, 1, [](const Args& a, QString*) {
                     const auto v = vec(a[0]);
                     return Value(v.isEmpty() ? NAN : std::accumulate(v.begin(), v.end(), 0.0) / v.size());
                 }});
        t.insert(QStringLiteral("mean"), t.value(QStringLiteral("avg")));
        t.insert(QStringLiteral("sd"), {1, 1, [](const Args& a, QString*) {
                     const auto v = vec(a[0]);
                     if (v.size() < 2) return Value(0.0);
                     const double m = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
                     double s = 0;
                     for (double d : v) s += (d - m) * (d - m);
                     return Value(std::sqrt(s / (v.size() - 1)));
                 }});
        t.insert(QStringLiteral("int"), {1, 2, [](const Args& a, QString*) -> Value {
                     if (a.size() == 1) return mapN(a, [](const double* x) { return std::trunc(x[0]); });
                     const auto x = vec(a[0]);
                     const auto y = vec(a[1]);
                     const int n = std::min(x.size(), y.size());
                     double s = 0;
                     for (int i = 1; i < n; ++i) s += 0.5 * (y[i] + y[i - 1]) * (x[i] - x[i - 1]);
                     return Value(s);
                 }});
        t.insert(QStringLiteral("pi"), {0, 0, [](const Args&, QString*) { return Value(M_PI); }});
        return t;
    }();
    return table;
}

// ------------------------------------------------------------------ evaluation

Value binaryOp(int op, const Value& a, const Value& b)
{
    auto apply = [&](auto f) -> Value {
        if (!a.isVector && !b.isVector) return Value(f(a.s, b.s));
        const int n = (a.isVector && b.isVector) ? std::min(a.size(), b.size()) : (a.isVector ? a.size() : b.size());
        QVector<double> out(n);
        for (int i = 0; i < n; ++i) out[i] = f(a.at(i), b.at(i));
        return Value(out);
    };
    switch (op) {
    case OpAdd: return apply([](double x, double y) { return x + y; });
    case OpSub: return apply([](double x, double y) { return x - y; });
    case OpMul: return apply([](double x, double y) { return x * y; });
    case OpDiv: return apply([](double x, double y) { return x / y; });
    case OpMod: return apply([](double x, double y) { return std::fmod(x, y); });
    case OpPow: return apply([](double x, double y) { return std::pow(x, y); });
    case OpEq: return apply([](double x, double y) { return x == y ? 1.0 : 0.0; });
    case OpNe: return apply([](double x, double y) { return x != y ? 1.0 : 0.0; });
    case OpLt: return apply([](double x, double y) { return x < y ? 1.0 : 0.0; });
    case OpLe: return apply([](double x, double y) { return x <= y ? 1.0 : 0.0; });
    case OpGt: return apply([](double x, double y) { return x > y ? 1.0 : 0.0; });
    case OpGe: return apply([](double x, double y) { return x >= y ? 1.0 : 0.0; });
    case OpAnd: return apply([](double x, double y) { return (x != 0 && y != 0) ? 1.0 : 0.0; });
    case OpOr: return apply([](double x, double y) { return (x != 0 || y != 0) ? 1.0 : 0.0; });
    default: return Value(NAN);
    }
}

class Evaluator {
public:
    Evaluator(const Context& ctx, QString* err) : m_ctx(ctx), m_err(err) {}

    Value eval(const Node* n)
    {
        switch (n->kind) {
        case NodeKind::Number: return Value(n->num);
        case NodeKind::Ident: {
            Value v;
            if (m_ctx.lookup(n->name, &v)) return v;
            const auto& fns = functions();
            auto it = fns.constFind(n->name);
            if (it != fns.constEnd() && it->minArgs == 0) return it->fn(Args{}, m_err);
            if (n->name == QLatin1String("inf")) return Value(HUGE_VAL);
            if (n->name == QLatin1String("nan")) return Value(NAN);
            if (m_err->isEmpty()) *m_err = QStringLiteral("unknown identifier '%1'").arg(n->name);
            return Value(NAN);
        }
        case NodeKind::Unary: {
            Value a = eval(n->kids[0].get());
            if (n->op == OpPlus) return a;
            auto f = [&](double x) { return n->op == OpNeg ? -x : (x == 0.0 ? 1.0 : 0.0); };
            if (!a.isVector) return Value(f(a.s));
            for (double& d : a.v) d = f(d);
            return a;
        }
        case NodeKind::Binary: return binaryOp(n->op, eval(n->kids[0].get()), eval(n->kids[1].get()));
        case NodeKind::Ternary: {
            const Value c = eval(n->kids[0].get());
            if (!c.isVector) return c.s != 0.0 ? eval(n->kids[1].get()) : eval(n->kids[2].get());
            const Value a = eval(n->kids[1].get());
            const Value b = eval(n->kids[2].get());
            QVector<double> out(c.size());
            for (int i = 0; i < c.size(); ++i) out[i] = c.v[i] != 0.0 ? a.at(i) : b.at(i);
            return Value(out);
        }
        case NodeKind::Index: {
            const Value base = eval(n->kids[0].get());
            const Value idx = eval(n->kids[1].get());
            auto get = [&](double di) {
                const int i = int(std::lround(di));
                if (!base.isVector) return i == 0 ? base.s : NAN;
                return (i >= 0 && i < base.v.size()) ? base.v[i] : NAN;
            };
            if (!idx.isVector) return Value(get(idx.s));
            QVector<double> out(idx.size());
            for (int i = 0; i < idx.size(); ++i) out[i] = get(idx.v[i]);
            return Value(out);
        }
        case NodeKind::Call: {
            const auto& fns = functions();
            auto it = fns.constFind(n->name);
            if (it == fns.constEnd()) {
                if (m_err->isEmpty()) *m_err = QStringLiteral("unknown function '%1'").arg(n->name);
                return Value(NAN);
            }
            const int na = int(n->kids.size());
            if (na < it->minArgs || na > it->maxArgs) {
                if (m_err->isEmpty()) *m_err = QStringLiteral("wrong number of arguments for %1()").arg(n->name);
                return Value(NAN);
            }
            Args args;
            args.reserve(na);
            for (const auto& k : n->kids) args.push_back(eval(k.get()));
            return it->fn(args, m_err);
        }
        }
        return Value(NAN);
    }

private:
    const Context& m_ctx;
    QString* m_err;
};

void collectIdents(const Node* n, QSet<QString>* out)
{
    if (!n) return;
    if (n->kind == NodeKind::Ident) out->insert(n->name);
    for (const auto& k : n->kids) collectIdents(k.get(), out);
}

}  // namespace

// ------------------------------------------------------------------ Expression

Expression::Expression() = default;
Expression::~Expression() = default;
Expression::Expression(Expression&&) noexcept = default;
Expression& Expression::operator=(Expression&&) noexcept = default;

bool Expression::parse(const QString& text)
{
    m_text = text;
    m_root.reset();
    m_error.clear();
    std::vector<Token> toks;
    Lexer lx(text);
    if (!lx.tokenize(&toks, &m_error)) return false;
    Parser p(std::move(toks));
    m_root = p.parse(&m_error);
    return m_root != nullptr;
}

Value Expression::eval(const Context& ctx) const
{
    m_evalError.clear();
    if (!m_root) {
        m_evalError = m_error.isEmpty() ? QStringLiteral("empty expression") : m_error;
        return Value(NAN);
    }
    Evaluator ev(ctx, &m_evalError);
    return ev.eval(m_root.get());
}

QStringList Expression::identifiers() const
{
    QSet<QString> s;
    collectIdents(m_root.get(), &s);
    QStringList l(s.begin(), s.end());
    l.sort();
    return l;
}

bool evaluate(const QString& text, const Context& ctx, Value* out, QString* error)
{
    Expression e;
    if (!e.parse(text)) {
        if (error) *error = e.error();
        return false;
    }
    *out = e.eval(ctx);
    if (!e.evalError().isEmpty()) {
        if (error) *error = e.evalError();
        return false;
    }
    return true;
}

QStringList functionNames()
{
    QStringList l = functions().keys();
    l.sort();
    return l;
}

}  // namespace cactus::expr
