// SPDX-License-Identifier: GPL-2.0-or-later
#include "math/Fit.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>

#include "math/Expr.h"

namespace cactus::fit {

namespace {

QString rhsOf(const QString& formula)
{
    QString f = formula.trimmed();
    // accept "y = ...", "y=..." and plain expressions
    static const QRegularExpression lhs(QStringLiteral("^\\s*(y|s\\d+\\.y)\\s*=(?!=)\\s*"), QRegularExpression::CaseInsensitiveOption);
    f.remove(lhs);
    return f;
}

/// Solve the n x n symmetric system M d = g with Gaussian elimination and partial pivoting.
bool solve(QVector<double> M, QVector<double> g, int n, QVector<double>* d)
{
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::fabs(M[r * n + c]) > std::fabs(M[piv * n + c])) piv = r;
        if (std::fabs(M[piv * n + c]) < 1e-300) return false;
        if (piv != c) {
            for (int k = 0; k < n; ++k) std::swap(M[c * n + k], M[piv * n + k]);
            std::swap(g[c], g[piv]);
        }
        for (int r = c + 1; r < n; ++r) {
            const double f = M[r * n + c] / M[c * n + c];
            for (int k = c; k < n; ++k) M[r * n + k] -= f * M[c * n + k];
            g[r] -= f * g[c];
        }
    }
    d->resize(n);
    for (int r = n - 1; r >= 0; --r) {
        double s = g[r];
        for (int k = r + 1; k < n; ++k) s -= M[r * n + k] * (*d)[k];
        (*d)[r] = s / M[r * n + r];
    }
    return true;
}

bool invert(const QVector<double>& M, int n, QVector<double>* inv)
{
    *inv = QVector<double>(n * n, 0.0);
    for (int c = 0; c < n; ++c) {
        QVector<double> e(n, 0.0), col;
        e[c] = 1.0;
        if (!solve(M, e, n, &col)) return false;
        for (int r = 0; r < n; ++r) (*inv)[r * n + c] = col[r];
    }
    return true;
}

double clampParam(double v, const Param& p)
{
    if (!p.constrained) return v;
    return std::clamp(v, std::min(p.lo, p.hi), std::max(p.lo, p.hi));
}

}  // namespace

QVector<double> evaluate(const QString& formula, const QVector<double>& x, const QVector<double>& params, QString* error)
{
    expr::Expression e;
    if (!e.parse(rhsOf(formula))) {
        if (error) *error = e.error();
        return QVector<double>(x.size(), NAN);
    }
    expr::Context ctx;
    ctx.setVector(QStringLiteral("x"), x);
    for (int i = 0; i < params.size(); ++i) ctx.setScalar(QStringLiteral("a%1").arg(i), params[i]);
    const expr::Value v = e.eval(ctx);
    if (!e.evalError().isEmpty()) {
        if (error) *error = e.evalError();
        return QVector<double>(x.size(), NAN);
    }
    return v.toVector(x.size());
}

Result nonlinear(const QVector<double>& x0, const QVector<double>& y0, const QVector<double>& dy0, const Options& o)
{
    Result res;
    const int np = std::clamp(o.nparams, 1, kMaxParams);
    expr::Expression e;
    if (!e.parse(rhsOf(o.formula))) {
        res.message = QStringLiteral("formula error: %1").arg(e.error());
        return res;
    }
    // data and weights (drop non-finite points)
    QVector<double> x, y, w;
    expr::Expression wexpr;
    if (o.weighting == Weighting::Custom && !wexpr.parse(o.customWeight)) {
        res.message = QStringLiteral("weight expression error: %1").arg(wexpr.error());
        return res;
    }
    for (int i = 0; i < std::min(x0.size(), y0.size()); ++i) {
        if (!std::isfinite(x0[i]) || !std::isfinite(y0[i])) continue;
        double wi = 1.0;
        switch (o.weighting) {
        case Weighting::None: break;
        case Weighting::InverseY: wi = y0[i] != 0 ? 1.0 / std::fabs(y0[i]) : 0.0; break;
        case Weighting::InverseY2: wi = y0[i] != 0 ? 1.0 / (y0[i] * y0[i]) : 0.0; break;
        case Weighting::InverseDY2: {
            const double d = i < dy0.size() ? dy0[i] : 0.0;
            wi = d != 0 ? 1.0 / (d * d) : 0.0;
            break;
        }
        case Weighting::Custom: {
            expr::Context c;
            c.setScalar(QStringLiteral("x"), x0[i]);
            c.setScalar(QStringLiteral("y"), y0[i]);
            c.setScalar(QStringLiteral("dy"), i < dy0.size() ? dy0[i] : 0.0);
            wi = wexpr.evalScalar(c);
            break;
        }
        }
        if (!std::isfinite(wi) || wi < 0) wi = 0;
        x.push_back(x0[i]);
        y.push_back(y0[i]);
        w.push_back(wi);
    }
    const int n = x.size();
    QVector<int> freeIdx;
    for (int k = 0; k < np; ++k) if (!o.params[k].fixed) freeIdx.push_back(k);
    const int nf = freeIdx.size();
    res.npoints = n;
    if (n < nf || nf == 0) {
        res.message = QStringLiteral("not enough data points (%1) for %2 free parameters").arg(n).arg(nf);
        return res;
    }
    QVector<double> p(np);
    for (int k = 0; k < np; ++k) p[k] = clampParam(o.params[k].value, o.params[k]);

    expr::Context ctx;
    ctx.setVector(QStringLiteral("x"), x);
    auto model = [&](const QVector<double>& pp, QVector<double>* f) {
        for (int k = 0; k < np; ++k) ctx.setScalar(QStringLiteral("a%1").arg(k), pp[k]);
        for (int k = np; k < kMaxParams; ++k) ctx.setScalar(QStringLiteral("a%1").arg(k), 0.0);
        const expr::Value v = e.eval(ctx);
        if (!e.evalError().isEmpty()) return false;
        *f = v.toVector(n);
        for (double d : *f) if (!std::isfinite(d)) return false;
        return true;
    };
    auto chi2Of = [&](const QVector<double>& f) {
        double s = 0;
        for (int i = 0; i < n; ++i) s += w[i] * (y[i] - f[i]) * (y[i] - f[i]);
        return s;
    };

    QVector<double> f;
    if (!model(p, &f)) {
        res.message = QStringLiteral("cannot evaluate the model at the initial parameters: %1").arg(e.evalError().isEmpty() ? QStringLiteral("non-finite values") : e.evalError());
        return res;
    }
    double chi2 = chi2Of(f);
    double lambda = 1e-3;
    QVector<double> J(n * nf), JtJ(nf * nf), Jtr(nf);
    int it = 0;
    bool converged = false;
    for (; it < o.maxIterations; ++it) {
        // numerical Jacobian (central differences)
        for (int c = 0; c < nf; ++c) {
            const int k = freeIdx[c];
            const double h = 1e-6 * std::max(1e-8, std::fabs(p[k])) + 1e-10;
            QVector<double> pp = p, pm = p, fp, fm;
            pp[k] += h;
            pm[k] -= h;
            const bool okp = model(pp, &fp), okm = model(pm, &fm);
            for (int i = 0; i < n; ++i) {
                double d;
                if (okp && okm) d = (fp[i] - fm[i]) / (2 * h);
                else if (okp) d = (fp[i] - f[i]) / h;
                else if (okm) d = (f[i] - fm[i]) / h;
                else d = 0;
                J[i * nf + c] = d;
            }
        }
        std::fill(JtJ.begin(), JtJ.end(), 0.0);
        std::fill(Jtr.begin(), Jtr.end(), 0.0);
        for (int i = 0; i < n; ++i) {
            const double r = y[i] - f[i];
            for (int a = 0; a < nf; ++a) {
                const double ja = J[i * nf + a] * w[i];
                Jtr[a] += ja * r;
                for (int b = 0; b <= a; ++b) JtJ[a * nf + b] += ja * J[i * nf + b];
            }
        }
        for (int a = 0; a < nf; ++a)
            for (int b = a + 1; b < nf; ++b) JtJ[a * nf + b] = JtJ[b * nf + a];

        bool improved = false;
        for (int tries = 0; tries < 30; ++tries) {
            QVector<double> M = JtJ, d;
            for (int a = 0; a < nf; ++a) M[a * nf + a] += lambda * std::max(JtJ[a * nf + a], 1e-12);
            if (!solve(M, Jtr, nf, &d)) {
                lambda *= 10;
                continue;
            }
            QVector<double> pn = p;
            for (int c = 0; c < nf; ++c) pn[freeIdx[c]] = clampParam(p[freeIdx[c]] + d[c], o.params[freeIdx[c]]);
            QVector<double> fn;
            if (model(pn, &fn)) {
                const double cn = chi2Of(fn);
                if (cn <= chi2) {
                    const double rel = (chi2 - cn) / std::max(chi2, 1e-300);
                    double step = 0;
                    for (int c = 0; c < nf; ++c) step = std::max(step, std::fabs(d[c]) / (std::fabs(p[freeIdx[c]]) + 1e-12));
                    p = pn;
                    f = fn;
                    chi2 = cn;
                    lambda = std::max(lambda / 10, 1e-15);
                    improved = true;
                    if (rel < o.tolerance || step < o.tolerance) converged = true;
                    break;
                }
            }
            lambda *= 10;
            if (lambda > 1e16) break;
        }
        if (!improved || converged) {
            converged = true;
            break;
        }
    }

    res.ok = true;
    res.iterations = it + 1;
    res.values = p;
    res.chi2 = chi2;
    res.dof = n - nf;
    res.reducedChi2 = res.dof > 0 ? chi2 / res.dof : 0.0;
    // covariance
    QVector<double> cov;
    res.errors = QVector<double>(np, 0.0);
    res.covariance = QVector<double>(np * np, 0.0);
    if (invert(JtJ, nf, &cov)) {
        const double scale = (o.weighting == Weighting::None || o.weighting == Weighting::InverseY ||
                              o.weighting == Weighting::InverseY2 || o.weighting == Weighting::Custom)
                                 ? res.reducedChi2
                                 : 1.0;
        for (int a = 0; a < nf; ++a) {
            for (int b = 0; b < nf; ++b) res.covariance[freeIdx[a] * np + freeIdx[b]] = cov[a * nf + b] * scale;
            res.errors[freeIdx[a]] = std::sqrt(std::max(0.0, cov[a * nf + a] * scale));
        }
    }
    double ss = 0, mean = 0;
    for (double v : y) mean += v;
    mean /= n;
    double sres = 0, sxy = 0, sff = 0, mf = 0;
    for (double v : f) mf += v;
    mf /= n;
    for (int i = 0; i < n; ++i) {
        ss += (y[i] - mean) * (y[i] - mean);
        sres += (y[i] - f[i]) * (y[i] - f[i]);
        sxy += (y[i] - mean) * (f[i] - mf);
        sff += (f[i] - mf) * (f[i] - mf);
    }
    res.rms = std::sqrt(sres / n);
    res.r2 = ss > 0 ? 1.0 - sres / ss : 1.0;
    res.correlation = (ss > 0 && sff > 0) ? sxy / std::sqrt(ss * sff) : 0.0;
    res.message = converged ? QStringLiteral("converged") : QStringLiteral("maximum number of iterations reached");
    return res;
}

QString Result::report(const QString& formula) const
{
    QString s;
    if (!formula.isEmpty()) s += QStringLiteral("Formula: %1\n").arg(formula);
    s += QStringLiteral("Status: %1 after %2 iterations\n").arg(message).arg(iterations);
    for (int k = 0; k < values.size(); ++k)
        s += QStringLiteral("  a%1 = %2  ± %3\n").arg(k).arg(values[k], 0, 'g', 10).arg(errors.value(k), 0, 'g', 4);
    s += QStringLiteral("Points: %1   Degrees of freedom: %2\n").arg(npoints).arg(dof);
    s += QStringLiteral("Chi-square: %1   Reduced chi-square: %2\n").arg(chi2, 0, 'g', 8).arg(reducedChi2, 0, 'g', 8);
    s += QStringLiteral("RMS residual: %1   R²: %2   Correlation: %3\n").arg(rms, 0, 'g', 6).arg(r2, 0, 'f', 8).arg(correlation, 0, 'f', 8);
    return s;
}

bool leastSquares(QVector<double> A, int m, int n, QVector<double> b, QVector<double>* c, QVector<double>* covUnscaled)
{
    if (m < n || n <= 0) return false;
    // column scaling for conditioning
    QVector<double> scale(n, 1.0);
    for (int j = 0; j < n; ++j) {
        double s = 0;
        for (int i = 0; i < m; ++i) s = std::max(s, std::fabs(A[i * n + j]));
        scale[j] = s > 0 ? s : 1.0;
        for (int i = 0; i < m; ++i) A[i * n + j] /= scale[j];
    }
    QVector<double> R = A;
    // Householder QR, applying reflections to b
    for (int k = 0; k < n; ++k) {
        double norm = 0;
        for (int i = k; i < m; ++i) norm += R[i * n + k] * R[i * n + k];
        norm = std::sqrt(norm);
        if (norm == 0) return false;
        const double alpha = R[k * n + k] > 0 ? -norm : norm;
        QVector<double> v(m, 0.0);
        for (int i = k; i < m; ++i) v[i] = R[i * n + k];
        v[k] -= alpha;
        double vn = 0;
        for (int i = k; i < m; ++i) vn += v[i] * v[i];
        if (vn == 0) continue;
        for (int j = k; j < n; ++j) {
            double d = 0;
            for (int i = k; i < m; ++i) d += v[i] * R[i * n + j];
            d = 2 * d / vn;
            for (int i = k; i < m; ++i) R[i * n + j] -= d * v[i];
        }
        double d = 0;
        for (int i = k; i < m; ++i) d += v[i] * b[i];
        d = 2 * d / vn;
        for (int i = k; i < m; ++i) b[i] -= d * v[i];
    }
    c->resize(n);
    for (int k = n - 1; k >= 0; --k) {
        double s = b[k];
        for (int j = k + 1; j < n; ++j) s -= R[k * n + j] * (*c)[j];
        if (std::fabs(R[k * n + k]) < 1e-300) return false;
        (*c)[k] = s / R[k * n + k];
    }
    if (covUnscaled) {
        // (R^T R)^-1 = R^-1 R^-T
        QVector<double> Rinv(n * n, 0.0);
        for (int j = 0; j < n; ++j) {
            Rinv[j * n + j] = 1.0 / R[j * n + j];
            for (int i = j - 1; i >= 0; --i) {
                double s = 0;
                for (int k = i + 1; k <= j; ++k) s += R[i * n + k] * Rinv[k * n + j];
                Rinv[i * n + j] = -s / R[i * n + i];
            }
        }
        *covUnscaled = QVector<double>(n * n, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                double s = 0;
                for (int k = 0; k < n; ++k) s += Rinv[i * n + k] * Rinv[j * n + k];
                (*covUnscaled)[i * n + j] = s / (scale[i] * scale[j]);
            }
    }
    for (int j = 0; j < n; ++j) (*c)[j] /= scale[j];
    return true;
}

RegressionResult regression(const QVector<double>& x0, const QVector<double>& y0, Regression type, int degree)
{
    RegressionResult r;
    QVector<double> x, y;
    for (int i = 0; i < std::min(x0.size(), y0.size()); ++i) {
        if (!std::isfinite(x0[i]) || !std::isfinite(y0[i])) continue;
        double tx = x0[i], ty = y0[i];
        switch (type) {
        case Regression::Exponential: if (ty <= 0) continue; ty = std::log(ty); break;
        case Regression::Power: if (tx <= 0 || ty <= 0) continue; tx = std::log(tx); ty = std::log(ty); break;
        case Regression::Logarithmic: if (tx <= 0) continue; tx = std::log(tx); break;
        case Regression::Inverse: if (ty == 0) continue; ty = 1.0 / ty; break;
        default: break;
        }
        x.push_back(tx);
        y.push_back(ty);
    }
    if (type == Regression::InverseExponential) {
        Options o;
        o.formula = QStringLiteral("a0 * (1 - exp(-a1 * x))");
        o.nparams = 2;
        double ymax = 0;
        for (double v : y0) ymax = std::max(ymax, std::fabs(v));
        o.params[0].value = ymax > 0 ? ymax : 1.0;
        double xr = 0;
        for (double v : x0) xr = std::max(xr, std::fabs(v));
        o.params[1].value = xr > 0 ? 1.0 / xr : 1.0;
        const Result nr = nonlinear(x0, y0, {}, o);
        r.ok = nr.ok;
        r.message = nr.message;
        r.coefficients = nr.values;
        r.errors = nr.errors;
        r.r2 = nr.r2;
        r.rms = nr.rms;
        r.chi2 = nr.chi2;
        r.npoints = nr.npoints;
        r.formula = QStringLiteral("%1 * (1 - exp(-%2 * x))").arg(r.coefficients.value(0), 0, 'g', 12).arg(r.coefficients.value(1), 0, 'g', 12);
        return r;
    }
    const int deg = (type == Regression::Polynomial) ? std::clamp(degree, 0, 10) : 1;
    const int n = deg + 1, m = x.size();
    r.npoints = m;
    if (m < n) {
        r.message = QStringLiteral("not enough valid points for this regression");
        return r;
    }
    QVector<double> A(m * n);
    for (int i = 0; i < m; ++i) {
        double p = 1.0;
        for (int j = 0; j < n; ++j) {
            A[i * n + j] = p;
            p *= x[i];
        }
    }
    QVector<double> c, cov;
    if (!leastSquares(A, m, n, y, &c, &cov)) {
        r.message = QStringLiteral("singular system (are all x values equal?)");
        return r;
    }
    double sres = 0, mean = 0;
    for (double v : y) mean += v;
    mean /= m;
    double ss = 0;
    for (int i = 0; i < m; ++i) {
        double f = 0, p = 1;
        for (int j = 0; j < n; ++j) { f += c[j] * p; p *= x[i]; }
        sres += (y[i] - f) * (y[i] - f);
        ss += (y[i] - mean) * (y[i] - mean);
    }
    const double s2 = m > n ? sres / (m - n) : 0.0;
    r.errors.resize(n);
    for (int j = 0; j < n; ++j) r.errors[j] = std::sqrt(std::max(0.0, cov[j * n + j] * s2));
    r.r2 = ss > 0 ? 1.0 - sres / ss : 1.0;
    r.rms = std::sqrt(sres / m);
    r.chi2 = sres;
    r.ok = true;
    r.message = QStringLiteral("ok");
    auto g = [](double v) { return QString::number(v, 'g', 12); };
    switch (type) {
    case Regression::Linear:
    case Regression::Polynomial: {
        r.coefficients = c;
        QStringList terms;
        for (int j = 0; j < n; ++j) {
            if (j == 0) terms << g(c[j]);
            else if (j == 1) terms << QStringLiteral("(%1)*x").arg(g(c[j]));
            else terms << QStringLiteral("(%1)*x^%2").arg(g(c[j])).arg(j);
        }
        r.formula = terms.join(QStringLiteral(" + "));
        break;
    }
    case Regression::Exponential:
        r.coefficients = {std::exp(c[0]), c[1]};
        r.formula = QStringLiteral("%1 * exp(%2 * x)").arg(g(r.coefficients[0]), g(c[1]));
        break;
    case Regression::Power:
        r.coefficients = {std::exp(c[0]), c[1]};
        r.formula = QStringLiteral("%1 * x^(%2)").arg(g(r.coefficients[0]), g(c[1]));
        break;
    case Regression::Logarithmic:
        r.coefficients = {c[0], c[1]};
        r.formula = QStringLiteral("%1 + (%2) * ln(x)").arg(g(c[0]), g(c[1]));
        break;
    case Regression::Inverse:
        r.coefficients = {c[0], c[1]};
        r.formula = QStringLiteral("1 / (%1 + (%2) * x)").arg(g(c[0]), g(c[1]));
        break;
    default: break;
    }
    // R^2 in the original space for linearised models
    if (type != Regression::Linear && type != Regression::Polynomial) {
        const QVector<double> fx = evaluateRegression(r, type, x0);
        double s1 = 0, s0 = 0, mn = 0;
        int k = 0;
        for (int i = 0; i < std::min(x0.size(), y0.size()); ++i) if (std::isfinite(fx[i])) { mn += y0[i]; ++k; }
        mn /= std::max(1, k);
        for (int i = 0; i < std::min(x0.size(), y0.size()); ++i) {
            if (!std::isfinite(fx[i])) continue;
            s1 += (y0[i] - fx[i]) * (y0[i] - fx[i]);
            s0 += (y0[i] - mn) * (y0[i] - mn);
        }
        r.r2 = s0 > 0 ? 1.0 - s1 / s0 : 1.0;
        r.rms = std::sqrt(s1 / std::max(1, k));
        r.chi2 = s1;
    }
    return r;
}

QVector<double> evaluateRegression(const RegressionResult& r, Regression type, const QVector<double>& x)
{
    QVector<double> out(x.size(), NAN);
    if (!r.ok) return out;
    const auto& c = r.coefficients;
    for (int i = 0; i < x.size(); ++i) {
        const double v = x[i];
        switch (type) {
        case Regression::Linear:
        case Regression::Polynomial: {
            double s = 0, p = 1;
            for (double cj : c) { s += cj * p; p *= v; }
            out[i] = s;
            break;
        }
        case Regression::Exponential: out[i] = c[0] * std::exp(c[1] * v); break;
        case Regression::Power: out[i] = c[0] * std::pow(v, c[1]); break;
        case Regression::Logarithmic: out[i] = c[0] + c[1] * std::log(v); break;
        case Regression::Inverse: out[i] = 1.0 / (c[0] + c[1] * v); break;
        case Regression::InverseExponential: out[i] = c[0] * (1.0 - std::exp(-c[1] * v)); break;
        }
    }
    return out;
}

QString RegressionResult::report() const
{
    QString s = QStringLiteral("Fitted curve: y = %1\n").arg(formula);
    for (int k = 0; k < coefficients.size(); ++k)
        s += QStringLiteral("  c%1 = %2  ± %3\n").arg(k).arg(coefficients[k], 0, 'g', 10).arg(errors.value(k), 0, 'g', 4);
    s += QStringLiteral("Points: %1   R²: %2   RMS residual: %3\n").arg(npoints).arg(r2, 0, 'f', 8).arg(rms, 0, 'g', 6);
    return s;
}

}  // namespace cactus::fit
