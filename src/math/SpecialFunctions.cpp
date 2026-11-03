// SPDX-License-Identifier: GPL-2.0-or-later
#include "math/SpecialFunctions.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cactus::special {

namespace {
constexpr double kEuler = 0.57721566490153286061;
constexpr double kEps = 1e-15;
const double kNaN = std::numeric_limits<double>::quiet_NaN();

const double xgk[8] = {0.991455371120812639206854697526329, 0.949107912342758524526189684047851,
                       0.864864423359769072789712788640926, 0.741531185599394439863864773280788,
                       0.586087235467691130294144845693013, 0.405845151377397166906606412076961,
                       0.207784955007898467600689403773245, 0.0};
const double wgk[8] = {0.022935322010529224963732008058970, 0.063092092629978553290700663189204,
                       0.104790010322250183839876322541518, 0.140653259715525918745189590510238,
                       0.169004726639267902826583426598550, 0.190350578064785409913256402421014,
                       0.204432940075298892414161999234649, 0.209482141084727828012999174891714};
const double wg[4] = {0.129484966168869693270611432679082, 0.279705391489276667901467771423780,
                      0.381830050505118944950369775488975, 0.417959183673469387755102040816327};

void gk15(const std::function<double(double)>& f, double a, double b, double* result, double* err)
{
    const double c = 0.5 * (a + b), h = 0.5 * (b - a);
    const double fc = f(c);
    double rk = fc * wgk[7];
    double rg = fc * wg[3];
    for (int j = 0; j < 7; ++j) {
        const double dx = h * xgk[j];
        const double f1 = f(c - dx), f2 = f(c + dx);
        rk += wgk[j] * (f1 + f2);
        if (j % 2 == 1) rg += wg[j / 2] * (f1 + f2);
    }
    *result = rk * h;
    *err = std::fabs((rk - rg) * h);
}

double adapt(const std::function<double(double)>& f, double a, double b, double whole, double err, double tolAbs, int depth)
{
    if (err <= tolAbs || depth <= 0 || !std::isfinite(whole) || std::fabs(b - a) < 1e-300) return whole;
    const double m = 0.5 * (a + b);
    double r1, e1, r2, e2;
    gk15(f, a, m, &r1, &e1);
    gk15(f, m, b, &r2, &e2);
    return adapt(f, a, m, r1, e1, tolAbs * 0.5, depth - 1) + adapt(f, m, b, r2, e2, tolAbs * 0.5, depth - 1);
}

double igamSeries(double a, double x)
{
    double ap = a, sum = 1.0 / a, del = sum;
    for (int n = 0; n < 10000; ++n) {
        ap += 1.0;
        del *= x / ap;
        sum += del;
        if (std::fabs(del) < std::fabs(sum) * kEps) break;
    }
    return sum * std::exp(-x + a * std::log(x) - std::lgamma(a));
}

double igamcFraction(double a, double x)
{
    const double tiny = 1e-300;
    double b = x + 1.0 - a, c = 1.0 / tiny, d = 1.0 / b, h = d;
    for (int i = 1; i < 10000; ++i) {
        const double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::fabs(d) < tiny) d = tiny;
        c = b + an / c;
        if (std::fabs(c) < tiny) c = tiny;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < kEps) break;
    }
    return std::exp(-x + a * std::log(x) - std::lgamma(a)) * h;
}

double betacf(double a, double b, double x)
{
    const double tiny = 1e-300;
    const double qab = a + b, qap = a + 1.0, qam = a - 1.0;
    double c = 1.0, d = 1.0 - qab * x / qap;
    if (std::fabs(d) < tiny) d = tiny;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m < 10000; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < tiny) d = tiny;
        c = 1.0 + aa / c;
        if (std::fabs(c) < tiny) c = tiny;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < tiny) d = tiny;
        c = 1.0 + aa / c;
        if (std::fabs(c) < tiny) c = tiny;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < kEps) break;
    }
    return h;
}

/// Solve g(x) = target for monotone g on [lo, hi] by bisection.
template <class G>
double bisect(G g, double target, double lo, double hi, bool increasing)
{
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double v = g(mid);
        if ((v < target) == increasing) lo = mid;
        else hi = mid;
        if (std::fabs(hi - lo) <= 1e-15 * std::max(1.0, std::fabs(mid))) break;
    }
    return 0.5 * (lo + hi);
}

bool isInteger(double v) { return std::floor(v) == v; }

double besselSeries(double v, double x, bool modified)
{
    // sum (+-1)^k (x/2)^(2k+v) / (k! Gamma(k+v+1))
    const double hx = 0.5 * x;
    double sum = 0.0;
    for (int k = 0; k < 500; ++k) {
        const double g = k + v + 1.0;
        if (g <= 0 && isInteger(g)) continue;  // 1/Gamma(non-positive integer) = 0
        const double lg = std::lgamma(g);
        double sgnG = 1.0;
        if (g < 0) sgnG = (int(std::floor(g)) % 2 == 0) ? 1.0 : -1.0;
        const double logTerm = (2 * k + v) * std::log(hx) - std::lgamma(k + 1.0) - lg;
        double term = std::exp(logTerm) * sgnG;
        if (!modified && (k % 2 == 1)) term = -term;
        sum += term;
        if (k > 2 && std::fabs(term) < 1e-17 * std::fabs(sum)) break;
    }
    return sum;
}
}  // namespace

double integrate(const std::function<double(double)>& f, double a, double b, double tol, int maxDepth)
{
    if (a == b) return 0.0;
    double r, e;
    gk15(f, a, b, &r, &e);
    const double tolAbs = std::max(tol * std::fabs(r), 1e-300);
    return adapt(f, a, b, r, e, tolAbs, maxDepth);
}

double integrateToInf(const std::function<double(double)>& f, double a, double tol)
{
    auto g = [&](double u) {
        const double om = 1.0 - u;
        const double t = a + u / om;
        const double v = f(t) / (om * om);
        return std::isfinite(v) ? v : 0.0;
    };
    return integrate(g, 0.0, 1.0, tol, 50);
}

// ------------------------------------------------------------------ gamma family

double rgamma(double x)
{
    if (x <= 0 && isInteger(x)) return 0.0;
    return 1.0 / std::tgamma(x);
}

double psi(double x)
{
    if (x <= 0 && isInteger(x)) return kNaN;
    if (x < 0) return psi(1.0 - x) - M_PI / std::tan(M_PI * x);
    double r = 0.0;
    while (x < 6.0) {
        r -= 1.0 / x;
        x += 1.0;
    }
    const double f = 1.0 / (x * x);
    return r + std::log(x) - 0.5 / x -
           f * (1.0 / 12 - f * (1.0 / 120 - f * (1.0 / 252 - f * (1.0 / 240 - f * (1.0 / 132)))));
}

double lbeta(double a, double b) { return std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b); }

double beta(double a, double b)
{
    if (a + b < 170.0 && a > 0 && b > 0) return std::tgamma(a) * std::tgamma(b) / std::tgamma(a + b);
    if (a > 0 && b > 0) return std::exp(lbeta(a, b));
    return std::tgamma(a) * std::tgamma(b) / std::tgamma(a + b);
}

double igam(double a, double x)
{
    if (x < 0 || a <= 0) return kNaN;
    if (x == 0) return 0.0;
    if (x < a + 1.0) return igamSeries(a, x);
    return 1.0 - igamcFraction(a, x);
}

double igamc(double a, double x)
{
    if (x < 0 || a <= 0) return kNaN;
    if (x == 0) return 1.0;
    if (x < a + 1.0) return 1.0 - igamSeries(a, x);
    return igamcFraction(a, x);
}

double igami(double a, double y)
{
    if (y <= 0) return HUGE_VAL;
    if (y >= 1) return 0.0;
    double hi = std::max(1.0, a);
    while (igamc(a, hi) > y && hi < 1e300) hi *= 2.0;
    return bisect([a](double x) { return igamc(a, x); }, y, 0.0, hi, false);
}

double incbet(double a, double b, double x)
{
    if (x <= 0) return 0.0;
    if (x >= 1) return 1.0;
    const double bt = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + a * std::log(x) + b * std::log1p(-x));
    if (x < (a + 1.0) / (a + b + 2.0)) return bt * betacf(a, b, x) / a;
    return 1.0 - bt * betacf(b, a, 1.0 - x) / b;
}

double incbi(double a, double b, double y)
{
    if (y <= 0) return 0.0;
    if (y >= 1) return 1.0;
    return bisect([a, b](double x) { return incbet(a, b, x); }, y, 0.0, 1.0, true);
}

// ------------------------------------------------------------------ distributions

double ndtr(double x) { return 0.5 * std::erfc(-x / M_SQRT2); }

double ndtri(double p)
{
    if (p <= 0) return -HUGE_VAL;
    if (p >= 1) return HUGE_VAL;
    // Acklam's rational approximation followed by one Halley refinement.
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                               1.383577518672690e+02,  -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                               6.680131188771972e+01,  -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00,  2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
                               3.754408661907416e+00};
    const double plow = 0.02425, phigh = 1 - plow;
    double x;
    if (p < plow) {
        const double q = std::sqrt(-2 * std::log(p));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    } else if (p <= phigh) {
        const double q = p - 0.5, r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    } else {
        const double q = std::sqrt(-2 * std::log(1 - p));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    }
    const double e = ndtr(x) - p;
    const double u = e * std::sqrt(2 * M_PI) * std::exp(x * x / 2);
    return x - u / (1 + x * u / 2);
}

double chdtr(double df, double x) { return igam(df / 2.0, x / 2.0); }
double chdtrc(double df, double x) { return igamc(df / 2.0, x / 2.0); }
double chdtri(double df, double y) { return 2.0 * igami(df / 2.0, y); }

double fdtr(double a, double b, double x)
{
    if (x < 0) return kNaN;
    return incbet(0.5 * a, 0.5 * b, a * x / (b + a * x));
}
double fdtrc(double a, double b, double x)
{
    if (x < 0) return kNaN;
    return incbet(0.5 * b, 0.5 * a, b / (b + a * x));
}
double fdtri(double a, double b, double y)
{
    const double w = incbi(0.5 * b, 0.5 * a, y);
    return b * (1.0 - w) / (a * w);
}

double gdtr(double a, double b, double x) { return x < 0 ? kNaN : igam(b, a * x); }
double gdtrc(double a, double b, double x) { return x < 0 ? kNaN : igamc(b, a * x); }
double pdtr(double k, double m) { return igamc(std::floor(k) + 1.0, m); }
double pdtrc(double k, double m) { return igam(std::floor(k) + 1.0, m); }
double pdtri(double k, double y) { return igami(std::floor(k) + 1.0, y); }

double stdtr(double k, double t)
{
    if (k <= 0) return kNaN;
    if (t == 0) return 0.5;
    const double z = k / (k + t * t);
    const double p = 0.5 * incbet(0.5 * k, 0.5, z);
    return t > 0 ? 1.0 - p : p;
}

double stdtri(double k, double p)
{
    if (p <= 0) return -HUGE_VAL;
    if (p >= 1) return HUGE_VAL;
    double lo = -1.0, hi = 1.0;
    while (stdtr(k, lo) > p && lo > -1e300) lo *= 2.0;
    while (stdtr(k, hi) < p && hi < 1e300) hi *= 2.0;
    return bisect([k](double t) { return stdtr(k, t); }, p, lo, hi, true);
}

// ------------------------------------------------------------------ Bessel

double jv(double v, double x)
{
    if (x < 0) {
        if (!isInteger(v)) return kNaN;
        const double r = jv(v, -x);
        return (int(std::fabs(v)) % 2 == 0) ? r : -r;
    }
    if (v < 0 && isInteger(v)) {
        const double r = jv(-v, x);
        return (int(-v) % 2 == 0) ? r : -r;
    }
    if (x == 0) return v == 0 ? 1.0 : (v > 0 ? 0.0 : HUGE_VAL);
    if (x < 12.0 || x < std::fabs(v)) return besselSeries(v, x, false);
    if (x > 2000.0 && x > 50 * v * v) {
        double mu = 4 * v * v;
        double P = 1 - (mu - 1) * (mu - 9) / (128 * x * x);
        double Q = (mu - 1) / (8 * x);
        const double chi = x - (0.5 * v + 0.25) * M_PI;
        return std::sqrt(2 / (M_PI * x)) * (P * std::cos(chi) - Q * std::sin(chi));
    }
    double r = integrate([v, x](double t) { return std::cos(v * t - x * std::sin(t)); }, 0.0, M_PI, 1e-13, 60) / M_PI;
    const double s = std::sin(v * M_PI);
    if (std::fabs(s) > 1e-15) r -= s / M_PI * integrateToInf([v, x](double t) { return std::exp(-x * std::sinh(t) - v * t); }, 0.0);
    return r;
}

double yv(double v, double x)
{
    if (x <= 0) return x == 0 ? -HUGE_VAL : kNaN;
    if (x > 2000.0 && x > 50 * v * v) {
        double mu = 4 * v * v;
        double P = 1 - (mu - 1) * (mu - 9) / (128 * x * x);
        double Q = (mu - 1) / (8 * x);
        const double chi = x - (0.5 * v + 0.25) * M_PI;
        return std::sqrt(2 / (M_PI * x)) * (P * std::sin(chi) + Q * std::cos(chi));
    }
    if (!isInteger(v) && x < 12.0) {
        // Y_v = (J_v cos(v pi) - J_{-v}) / sin(v pi)
        return (jv(v, x) * std::cos(v * M_PI) - jv(-v, x)) / std::sin(v * M_PI);
    }
    const double r1 = integrate([v, x](double t) { return std::sin(x * std::sin(t) - v * t); }, 0.0, M_PI, 1e-13, 60) / M_PI;
    const double cv = std::cos(v * M_PI);
    const double r2 = integrateToInf(
                          [v, x, cv](double t) {
                              const double e = std::exp(-x * std::sinh(t));
                              if (e == 0.0) return 0.0;
                              return (std::exp(v * t) + std::exp(-v * t) * cv) * e;
                          },
                          0.0) /
                      M_PI;
    return r1 - r2;
}

double iv(double v, double x)
{
    if (x < 0) {
        if (!isInteger(v)) return kNaN;
        const double r = iv(v, -x);
        return (int(std::fabs(v)) % 2 == 0) ? r : -r;
    }
    if (v < 0 && isInteger(v)) return iv(-v, x);
    if (x == 0) return v == 0 ? 1.0 : (v > 0 ? 0.0 : HUGE_VAL);
    if (x < 30.0) return besselSeries(v, x, true);
    double r = integrate([v, x](double t) { return std::exp(x * (std::cos(t) - 1.0)) * std::cos(v * t); }, 0.0, M_PI, 1e-13, 60) / M_PI;
    const double s = std::sin(v * M_PI);
    if (std::fabs(s) > 1e-15)
        r -= s / M_PI * integrateToInf([v, x](double t) { return std::exp(-x * std::cosh(t) - v * t - x); }, 0.0);
    return r * std::exp(x);
}

double kv(double v, double x)
{
    if (x <= 0) return x == 0 ? HUGE_VAL : kNaN;
    return std::exp(-x) * integrateToInf([v, x](double t) { return std::exp(-x * (std::cosh(t) - 1.0)) * std::cosh(v * t); }, 0.0);
}

double i0e(double x)
{
    const double ax = std::fabs(x);
    return integrate([ax](double t) { return std::exp(ax * (std::cos(t) - 1.0)); }, 0.0, M_PI) / M_PI;
}

double i1e(double x)
{
    const double ax = std::fabs(x);
    const double r = integrate([ax](double t) { return std::exp(ax * (std::cos(t) - 1.0)) * std::cos(t); }, 0.0, M_PI) / M_PI;
    return x < 0 ? -r : r;
}

double k0e(double x)
{
    if (x <= 0) return x == 0 ? HUGE_VAL : kNaN;
    return integrateToInf([x](double t) { return std::exp(-x * (std::cosh(t) - 1.0)); }, 0.0);
}

double k1e(double x)
{
    if (x <= 0) return x == 0 ? HUGE_VAL : kNaN;
    return integrateToInf([x](double t) { return std::exp(-x * (std::cosh(t) - 1.0)) * std::cosh(t); }, 0.0);
}

namespace {
void airySeries(double x, double* ai, double* bi)
{
    const double c1 = 0.355028053887817239260, c2 = 0.258819403792806798405;
    double f = 1.0, g = x, tf = 1.0, tg = x;
    const double x3 = x * x * x;
    for (int k = 1; k < 200; ++k) {
        tf *= x3 / ((3.0 * k - 1.0) * (3.0 * k));
        tg *= x3 / ((3.0 * k) * (3.0 * k + 1.0));
        f += tf;
        g += tg;
        if (std::fabs(tf) < 1e-17 * std::fabs(f) && std::fabs(tg) < 1e-17 * std::fabs(g)) break;
    }
    *ai = c1 * f - c2 * g;
    *bi = std::sqrt(3.0) * (c1 * f + c2 * g);
}
}  // namespace

double airyAi(double x)
{
    if (x > 2.0) {
        const double z = 2.0 / 3.0 * std::pow(x, 1.5);
        return std::sqrt(x / 3.0) * kv(1.0 / 3.0, z) / M_PI;
    }
    if (x < -6.0) {
        const double ax = -x, z = 2.0 / 3.0 * std::pow(ax, 1.5);
        return std::sqrt(ax) / 3.0 * (jv(1.0 / 3.0, z) + jv(-1.0 / 3.0, z));
    }
    double ai, bi;
    airySeries(x, &ai, &bi);
    return ai;
}

double airyBi(double x)
{
    if (x > 2.0) {
        const double z = 2.0 / 3.0 * std::pow(x, 1.5);
        return std::sqrt(x / 3.0) * (iv(-1.0 / 3.0, z) + iv(1.0 / 3.0, z));
    }
    if (x < -6.0) {
        const double ax = -x, z = 2.0 / 3.0 * std::pow(ax, 1.5);
        return std::sqrt(ax / 3.0) * (jv(-1.0 / 3.0, z) - jv(1.0 / 3.0, z));
    }
    double ai, bi;
    airySeries(x, &ai, &bi);
    return bi;
}

double struve(double v, double x)
{
    if (x == 0) return 0.0;
    const double hx = 0.5 * x;
    double sum = 0.0;
    for (int k = 0; k < 300; ++k) {
        const double lt = (2 * k + v + 1) * std::log(std::fabs(hx)) - std::lgamma(k + 1.5) - std::lgamma(k + v + 1.5);
        double term = std::exp(lt);
        if (std::tgamma(k + v + 1.5) < 0) term = -term;
        if (k % 2 == 1) term = -term;
        sum += term;
        if (k > 2 && std::fabs(term) < 1e-17 * std::fabs(sum)) break;
    }
    if (x < 0 && !isInteger(v)) return kNaN;
    if (x < 0) sum *= (int(v + 1) % 2 == 0) ? 1.0 : -1.0;
    return sum;
}

// ------------------------------------------------------------------ integrals

double dawsn(double x)
{
    const double ax = std::fabs(x);
    double r;
    if (ax > 50.0) {
        const double i2 = 1.0 / (2.0 * ax * ax);
        r = (1.0 + i2 + 3.0 * i2 * i2) / (2.0 * ax);
    } else {
        r = integrate([ax](double t) { return std::exp(t * t - ax * ax); }, 0.0, ax);
    }
    return x < 0 ? -r : r;
}

double ellik(double phi, double m)
{
    return integrate([m](double t) { const double s = std::sin(t); return 1.0 / std::sqrt(1.0 - m * s * s); }, 0.0, phi);
}

double ellie(double phi, double m)
{
    return integrate([m](double t) { const double s = std::sin(t); return std::sqrt(1.0 - m * s * s); }, 0.0, phi);
}

double ellpk(double m1)
{
    if (m1 <= 0) return HUGE_VAL;
    double a = 1.0, b = std::sqrt(m1);
    for (int i = 0; i < 60 && std::fabs(a - b) > 1e-16 * a; ++i) {
        const double an = 0.5 * (a + b);
        b = std::sqrt(a * b);
        a = an;
    }
    return M_PI / (2.0 * a);
}

double ellpe(double m1)
{
    if (m1 <= 0) return 1.0;
    return ellie(M_PI / 2.0, 1.0 - m1);
}

double expn(double n, double x)
{
    if (x < 0) return kNaN;
    if (x == 0) return n > 1 ? 1.0 / (n - 1.0) : HUGE_VAL;
    return integrateToInf([n, x](double t) { return std::exp(-x * t) / std::pow(t, n); }, 1.0);
}

namespace {
void fresnel(double x, double* S, double* C)
{
    const double ax = std::fabs(x);
    if (ax > 8.0) {
        const double pix = M_PI * ax, p2 = M_PI * M_PI;
        const double x4 = ax * ax * ax * ax;
        const double f = (1.0 - 3.0 / (p2 * x4) + 105.0 / (p2 * p2 * x4 * x4)) / pix;
        const double g = (1.0 - 15.0 / (p2 * x4)) / (p2 * ax * ax * ax);
        const double arg = 0.5 * M_PI * ax * ax;
        *C = 0.5 + f * std::sin(arg) - g * std::cos(arg);
        *S = 0.5 - f * std::cos(arg) - g * std::sin(arg);
    } else {
        *S = integrate([](double t) { return std::sin(0.5 * M_PI * t * t); }, 0.0, ax);
        *C = integrate([](double t) { return std::cos(0.5 * M_PI * t * t); }, 0.0, ax);
    }
    if (x < 0) { *S = -*S; *C = -*C; }
}

void sici(double x, double* s, double* c)
{
    const double ax = std::fabs(x);
    if (ax > 40.0) {
        const double z = 1.0 / (ax * ax);
        const double f = (1.0 - 2.0 * z + 24.0 * z * z - 720.0 * z * z * z) / ax;
        const double g = (1.0 - 6.0 * z + 120.0 * z * z - 5040.0 * z * z * z) * z;
        *s = M_PI / 2.0 - f * std::cos(ax) - g * std::sin(ax);
        *c = f * std::sin(ax) - g * std::cos(ax);
    } else {
        *s = integrate([](double t) { return t == 0 ? 1.0 : std::sin(t) / t; }, 0.0, ax);
        *c = kEuler + std::log(ax) + integrate([](double t) { return t == 0 ? 0.0 : (std::cos(t) - 1.0) / t; }, 0.0, ax);
    }
    if (x < 0) *s = -*s;
}
}  // namespace

double fresnls(double x) { double s, c; fresnel(x, &s, &c); return s; }
double fresnlc(double x) { double s, c; fresnel(x, &s, &c); return c; }
double si(double x) { double s, c; sici(x, &s, &c); return s; }
double ci(double x) { if (x == 0) return -HUGE_VAL; double s, c; sici(x, &s, &c); return c; }

double shi(double x)
{
    const double ax = std::fabs(x);
    const double r = integrate([](double t) { return t == 0 ? 1.0 : std::sinh(t) / t; }, 0.0, ax);
    return x < 0 ? -r : r;
}

double chi(double x)
{
    if (x == 0) return -HUGE_VAL;
    const double ax = std::fabs(x);
    return kEuler + std::log(ax) + integrate([](double t) { return t == 0 ? 0.0 : (std::cosh(t) - 1.0) / t; }, 0.0, ax);
}

double spence(double x)
{
    if (x < 0) return kNaN;
    if (x == 1) return 0.0;
    auto f = [](double t) {
        const double d = t - 1.0;
        if (std::fabs(d) < 1e-8) return 1.0 - d / 2.0;
        if (t <= 0) return 0.0;
        return std::log(t) / d;
    };
    return -integrate(f, 1.0, x);
}

// ------------------------------------------------------------------ hypergeometric & zeta

double hyp2f1(double a, double b, double c, double x)
{
    if (x > 1.0) return kNaN;
    if (x == 1.0) {
        if (c - a - b <= 0) return HUGE_VAL;
        return std::exp(std::lgamma(c) + std::lgamma(c - a - b) - std::lgamma(c - a) - std::lgamma(c - b));
    }
    if (x < 0.0) {
        // Pfaff transformation
        return std::pow(1.0 - x, -a) * hyp2f1(a, c - b, c, x / (x - 1.0));
    }
    double sum = 1.0, term = 1.0;
    for (int k = 0; k < 200000; ++k) {
        term *= (a + k) * (b + k) / ((c + k) * (k + 1.0)) * x;
        sum += term;
        if (std::fabs(term) < 1e-16 * std::fabs(sum)) break;
        if (term == 0.0) break;
    }
    return sum;
}

double hyperg(double a, double b, double x)
{
    if (x < -20.0) return std::exp(x) * hyperg(b - a, b, -x);  // Kummer transformation
    double sum = 1.0, term = 1.0;
    for (int k = 0; k < 100000; ++k) {
        term *= (a + k) / ((b + k) * (k + 1.0)) * x;
        sum += term;
        if (std::fabs(term) < 1e-16 * std::fabs(sum)) break;
        if (term == 0.0) break;
    }
    return sum;
}

double zeta(double x, double q)
{
    if (x == 1.0) return HUGE_VAL;
    if (x < 1.0 || q <= 0.0) return kNaN;
    const int N = 12;
    double sum = 0.0;
    for (int k = 0; k < N; ++k) sum += std::pow(k + q, -x);
    const double a = N + q;
    sum += std::pow(a, 1.0 - x) / (x - 1.0) + 0.5 * std::pow(a, -x);
    static const double B2j[] = {1.0 / 6, -1.0 / 30, 1.0 / 42, -1.0 / 30, 5.0 / 66, -691.0 / 2730, 7.0 / 6};
    double fact = 1.0;  // (2j)!
    double rising = x;  // x (x+1) ... (x+2j-2)
    double apow = std::pow(a, -x - 1.0);
    for (int j = 1; j <= 7; ++j) {
        fact *= (2.0 * j - 1.0) * (2.0 * j);
        sum += B2j[j - 1] / fact * rising * apow;
        rising *= (x + 2.0 * j - 1.0) * (x + 2.0 * j);
        apow /= a * a;
    }
    return sum;
}

double zetac(double x)
{
    if (x == 1.0) return HUGE_VAL;
    if (x > 1.0) {
        if (x > 60) return std::pow(2.0, -x);
        return zeta(x, 1.0) - 1.0;
    }
    if (x == 0.0) return -1.5;
    // reflection: zeta(x) = 2^x pi^(x-1) sin(pi x / 2) Gamma(1-x) zeta(1-x)
    const double z = std::pow(2.0, x) * std::pow(M_PI, x - 1.0) * std::sin(M_PI * x / 2.0) * std::tgamma(1.0 - x) *
                     (zetac(1.0 - x) + 1.0);
    return z - 1.0;
}

double voigt(double gamma, double sigma, double x)
{
    if (sigma <= 0) return gamma / (M_PI * (x * x + gamma * gamma));
    if (gamma <= 0) return std::exp(-0.5 * x * x / (sigma * sigma)) / (sigma * std::sqrt(2.0 * M_PI));
    auto f = [gamma, sigma, x](double t) {
        const double g = std::exp(-0.5 * t * t / (sigma * sigma)) / (sigma * std::sqrt(2.0 * M_PI));
        const double d = x - t;
        return g * gamma / (M_PI * (d * d + gamma * gamma));
    };
    // split at the Lorentzian peak to help the adaptive scheme
    const double lim = 12.0 * sigma;
    if (x > -lim && x < lim) return integrate(f, -lim, x, 1e-12) + integrate(f, x, lim, 1e-12);
    return integrate(f, -lim, lim, 1e-12);
}

}  // namespace cactus::special
