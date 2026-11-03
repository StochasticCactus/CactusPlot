// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - vectorised expression engine with Grace-compatible syntax.
//
// Supports: numbers, + - * / % ^ (**), comparison (== != < <= > >= and the
// Grace keywords EQ NE LT LE GT GE), logic (&& || ! AND OR NOT), the ternary
// operator, all Grace numerical/statistical/special functions, aggregate
// functions (MIN, MAX, AVG, SD, SUM, INT, IMIN, IMAX), vector indexing
// (s0.y[3]) and data-set references resolved through a Context callback.
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace cactus::expr {

/// A scalar or vector value.
struct Value {
    bool isVector = false;
    double s = 0.0;
    QVector<double> v;

    Value() = default;
    Value(double d) : s(d) {}  // NOLINT(google-explicit-constructor)
    Value(QVector<double> vec) : isVector(true), v(std::move(vec)) {}  // NOLINT

    int size() const { return isVector ? int(v.size()) : 1; }
    double at(int i) const { return isVector ? (i < v.size() ? v[i] : 0.0) : s; }
    double scalar() const { return isVector ? (v.isEmpty() ? 0.0 : v[0]) : s; }
    QVector<double> toVector(int n) const;
};

class Context {
public:
    /// Callback resolving identifiers such as "x", "s0.y", "g1.s2.x", "y3".
    /// Return true and fill the value if the identifier is known.
    using Resolver = std::function<bool(const QString& lowerName, Value* out)>;

    void setScalar(const QString& name, double v) { m_vars.insert(name.toLower(), Value(v)); }
    void setVector(const QString& name, const QVector<double>& v) { m_vars.insert(name.toLower(), Value(v)); }
    void setResolver(Resolver r) { m_resolver = std::move(r); }
    bool lookup(const QString& lowerName, Value* out) const;
    bool has(const QString& name) const { return m_vars.contains(name.toLower()); }
    void remove(const QString& name) { m_vars.remove(name.toLower()); }
    const QHash<QString, Value>& variables() const { return m_vars; }
    QHash<QString, Value>& variables() { return m_vars; }

private:
    QHash<QString, Value> m_vars;
    Resolver m_resolver;
};

struct Node;

class Expression {
public:
    Expression();
    ~Expression();
    Expression(const Expression&) = delete;
    Expression& operator=(const Expression&) = delete;
    Expression(Expression&&) noexcept;
    Expression& operator=(Expression&&) noexcept;

    /// Parse text; on failure error() describes the problem.
    bool parse(const QString& text);
    bool isValid() const { return m_root != nullptr; }
    QString error() const { return m_error; }
    QString text() const { return m_text; }

    /// Evaluate; throws nothing - errors are reported via evalError().
    Value eval(const Context& ctx) const;
    double evalScalar(const Context& ctx) const { return eval(ctx).scalar(); }
    QString evalError() const { return m_evalError; }
    /// Identifiers referenced by the expression (lower case).
    QStringList identifiers() const;

private:
    std::unique_ptr<Node> m_root;
    QString m_error;
    QString m_text;
    mutable QString m_evalError;
};

/// Convenience: parse + evaluate. Returns false on error (message in *error).
bool evaluate(const QString& text, const Context& ctx, Value* out, QString* error = nullptr);

/// Names of all built-in functions (for completion/help).
QStringList functionNames();

/// Seed the random generator used by rand/irand/rnorm.
void seedRandom(quint64 seed);
double randomUniform();
double randomNormal(double mean, double sigma);

}  // namespace cactus::expr
