// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - curve fitting: linear/polynomial/linearisable regressions and
// bounded Levenberg-Marquardt non-linear least squares with parameters a0..a9.
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <limits>

namespace cactus::fit {

constexpr int kMaxParams = 10;

struct Param {
    double value = 1.0;
    bool constrained = false;
    double lo = -std::numeric_limits<double>::infinity();
    double hi = std::numeric_limits<double>::infinity();
    bool fixed = false;
};

enum class Weighting { None, InverseY, InverseY2, InverseDY2, Custom };

struct Options {
    QString formula;  ///< "y = a0 + a1*x" or just the right-hand side
    int nparams = 2;
    std::array<Param, kMaxParams> params;
    int maxIterations = 200;
    double tolerance = 1e-10;
    Weighting weighting = Weighting::None;
    QString customWeight;  ///< expression in x, y (and dy) for Weighting::Custom
};

struct Result {
    bool ok = false;
    QString message;
    QVector<double> values;
    QVector<double> errors;      ///< standard errors (scaled by sqrt(reduced chi^2) when unweighted)
    QVector<double> covariance;  ///< n x n row-major
    double chi2 = 0.0;           ///< weighted sum of squared residuals
    double reducedChi2 = 0.0;
    double rms = 0.0;
    double r2 = 0.0;
    double correlation = 0.0;    ///< Pearson r between data and model
    int dof = 0;
    int iterations = 0;
    int npoints = 0;
    QString report(const QString& formula = QString()) const;
};

/// Non-linear least squares fit of y(x) using the expression engine.
/// `dy` (may be empty) is used for InverseDY2 weighting.
Result nonlinear(const QVector<double>& x, const QVector<double>& y, const QVector<double>& dy, const Options& o);

/// Evaluate a fit formula on x for given parameters (returns NaN vector on error).
QVector<double> evaluate(const QString& formula, const QVector<double>& x, const QVector<double>& params, QString* error = nullptr);

enum class Regression {
    Linear,
    Polynomial,       ///< degree given separately
    Exponential,      ///< y = A exp(B x)
    Power,            ///< y = A x^B
    Logarithmic,      ///< y = A + B ln x
    Inverse,          ///< y = 1 / (A + B x)
    InverseExponential  ///< y = A (1 - exp(-B x))... fitted non-linearly
};

struct RegressionResult {
    bool ok = false;
    QString message;
    QVector<double> coefficients;  ///< polynomial: c0 + c1 x + ...; others: A, B
    QVector<double> errors;
    double r2 = 0.0;
    double rms = 0.0;
    double chi2 = 0.0;
    int npoints = 0;
    QString formula;  ///< Grace expression of the fitted curve in x
    QString report() const;
};

RegressionResult regression(const QVector<double>& x, const QVector<double>& y, Regression type, int degree = 1);
QVector<double> evaluateRegression(const RegressionResult& r, Regression type, const QVector<double>& x);

/// Solve the least squares problem A c = b (A is m x n, row-major) by Householder QR.
bool leastSquares(QVector<double> A, int m, int n, QVector<double> b, QVector<double>* c, QVector<double>* covUnscaled);

}  // namespace cactus::fit
