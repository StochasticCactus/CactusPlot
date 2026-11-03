// SPDX-License-Identifier: GPL-2.0-or-later
#include "math/Transforms.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace cactus::transforms {

namespace {
int nmin(const QVector<double>& a, const QVector<double>& b) { return int(std::min(a.size(), b.size())); }
}  // namespace

XY running(const QVector<double>& x, const QVector<double>& y, int window, RunStat stat)
{
    XY out;
    const int n = nmin(x, y);
    if (window < 1 || window > n) return out;
    QVector<double> buf(window);
    for (int i = 0; i + window <= n; ++i) {
        double xm = 0;
        for (int k = 0; k < window; ++k) {
            xm += x[i + k];
            buf[k] = y[i + k];
        }
        xm /= window;
        double v = 0;
        switch (stat) {
        case RunStat::Average: v = std::accumulate(buf.begin(), buf.end(), 0.0) / window; break;
        case RunStat::Min: v = *std::min_element(buf.begin(), buf.end()); break;
        case RunStat::Max: v = *std::max_element(buf.begin(), buf.end()); break;
        case RunStat::Median: {
            QVector<double> s = buf;
            std::sort(s.begin(), s.end());
            v = (window % 2) ? s[window / 2] : 0.5 * (s[window / 2 - 1] + s[window / 2]);
            break;
        }
        case RunStat::StdDev: {
            const double m = std::accumulate(buf.begin(), buf.end(), 0.0) / window;
            double ss = 0;
            for (double d : buf) ss += (d - m) * (d - m);
            v = window > 1 ? std::sqrt(ss / (window - 1)) : 0.0;
            break;
        }
        }
        out.x.push_back(xm);
        out.y.push_back(v);
    }
    return out;
}

XY differentiate(const QVector<double>& x, const QVector<double>& y, DiffType t)
{
    XY out;
    const int n = nmin(x, y);
    if (n < 2) return out;
    switch (t) {
    case DiffType::Forward:
        for (int i = 0; i + 1 < n; ++i) {
            out.x.push_back(x[i]);
            out.y.push_back((y[i + 1] - y[i]) / (x[i + 1] - x[i]));
        }
        break;
    case DiffType::Backward:
        for (int i = 1; i < n; ++i) {
            out.x.push_back(x[i]);
            out.y.push_back((y[i] - y[i - 1]) / (x[i] - x[i - 1]));
        }
        break;
    case DiffType::Centered:
        for (int i = 1; i + 1 < n; ++i) {
            out.x.push_back(x[i]);
            out.y.push_back((y[i + 1] - y[i - 1]) / (x[i + 1] - x[i - 1]));
        }
        break;
    }
    return out;
}

XY integrate(const QVector<double>& x, const QVector<double>& y, double* total)
{
    XY out;
    const int n = nmin(x, y);
    double s = 0;
    for (int i = 0; i < n; ++i) {
        if (i > 0) s += 0.5 * (y[i] + y[i - 1]) * (x[i] - x[i - 1]);
        out.x.push_back(x[i]);
        out.y.push_back(s);
    }
    if (total) *total = s;
    return out;
}

XY seasonalDifference(const QVector<double>& x, const QVector<double>& y, int period)
{
    XY out;
    const int n = nmin(x, y);
    for (int i = 0; i + period < n; ++i) {
        out.x.push_back(x[i]);
        out.y.push_back(y[i + period] - y[i]);
    }
    return out;
}

XY cumulativeSum(const QVector<double>& x, const QVector<double>& y)
{
    XY out{x, y};
    double s = 0;
    for (double& v : out.y) { s += v; v = s; }
    return out;
}

QVector<double> linspace(double a, double b, int n)
{
    QVector<double> v(std::max(0, n));
    for (int i = 0; i < n; ++i) v[i] = n > 1 ? a + (b - a) * i / (n - 1) : a;
    return v;
}

namespace {

/// Sort (x, y) by x and merge duplicates by averaging.
void sortedUnique(const QVector<double>& x, const QVector<double>& y, QVector<double>* xs, QVector<double>* ys)
{
    const int n = nmin(x, y);
    QVector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return x[a] < x[b]; });
    xs->clear();
    ys->clear();
    for (int k = 0; k < n; ++k) {
        const int i = idx[k];
        if (!std::isfinite(x[i]) || !std::isfinite(y[i])) continue;
        if (!xs->isEmpty() && xs->last() == x[i]) {
            ys->last() = 0.5 * (ys->last() + y[i]);
            continue;
        }
        xs->push_back(x[i]);
        ys->push_back(y[i]);
    }
}

QVector<double> naturalSplineSecondDerivs(const QVector<double>& x, const QVector<double>& y)
{
    const int n = x.size();
    QVector<double> y2(n, 0.0), u(n, 0.0);
    for (int i = 1; i < n - 1; ++i) {
        const double sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
        const double p = sig * y2[i - 1] + 2.0;
        y2[i] = (sig - 1.0) / p;
        u[i] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
        u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }
    y2[n - 1] = 0.0;
    for (int k = n - 2; k >= 0; --k) y2[k] = y2[k] * y2[k + 1] + u[k];
    return y2;
}

QVector<double> akimaSlopes(const QVector<double>& x, const QVector<double>& y)
{
    const int n = x.size();
    QVector<double> m(n + 3);
    for (int i = 0; i < n - 1; ++i) m[i + 2] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]);
    m[1] = 2 * m[2] - m[3];
    m[0] = 2 * m[1] - m[2];
    m[n + 1] = 2 * m[n] - m[n - 1];
    m[n + 2] = 2 * m[n + 1] - m[n];
    QVector<double> t(n);
    for (int i = 0; i < n; ++i) {
        const double w1 = std::fabs(m[i + 3] - m[i + 2]), w2 = std::fabs(m[i + 1] - m[i]);
        t[i] = (w1 + w2 < 1e-12) ? 0.5 * (m[i + 1] + m[i + 2]) : (w1 * m[i + 1] + w2 * m[i + 2]) / (w1 + w2);
    }
    return t;
}

}  // namespace

XY interpolate(const QVector<double>& x0, const QVector<double>& y0, const QVector<double>& mesh, InterpMethod m, bool strict)
{
    XY out;
    QVector<double> x, y;
    sortedUnique(x0, y0, &x, &y);
    const int n = x.size();
    if (n < 2) return out;
    if (m != InterpMethod::Linear && n < 3) m = InterpMethod::Linear;
    QVector<double> y2, t;
    if (m == InterpMethod::Spline) y2 = naturalSplineSecondDerivs(x, y);
    if (m == InterpMethod::Akima) t = akimaSlopes(x, y);
    for (double xi : mesh) {
        if (strict && (xi < x.first() || xi > x.last())) continue;
        int k = int(std::upper_bound(x.begin(), x.end(), xi) - x.begin()) - 1;
        k = std::clamp(k, 0, n - 2);
        const double h = x[k + 1] - x[k];
        const double a = (x[k + 1] - xi) / h, b = (xi - x[k]) / h;
        double v;
        switch (m) {
        case InterpMethod::Linear: v = a * y[k] + b * y[k + 1]; break;
        case InterpMethod::Spline:
            v = a * y[k] + b * y[k + 1] + ((a * a * a - a) * y2[k] + (b * b * b - b) * y2[k + 1]) * h * h / 6.0;
            break;
        case InterpMethod::Akima: {
            const double s = (y[k + 1] - y[k]) / h;
            const double c2 = (3 * s - 2 * t[k] - t[k + 1]) / h;
            const double c3 = (t[k] + t[k + 1] - 2 * s) / (h * h);
            const double d = xi - x[k];
            v = y[k] + t[k] * d + c2 * d * d + c3 * d * d * d;
            break;
        }
        }
        out.x.push_back(xi);
        out.y.push_back(v);
    }
    return out;
}

XY histogram(const QVector<double>& data, const QVector<double>& bins, bool cumulative, bool normalize, bool centered)
{
    XY out;
    const int nb = int(bins.size()) - 1;
    if (nb < 1) return out;
    QVector<double> counts(nb, 0.0);
    int total = 0;
    for (double v : data) {
        if (!std::isfinite(v)) continue;
        if (v < bins.first() || v > bins.last()) continue;
        int k = int(std::upper_bound(bins.begin(), bins.end(), v) - bins.begin()) - 1;
        if (k >= nb) k = nb - 1;
        if (k < 0) continue;
        counts[k] += 1.0;
        ++total;
    }
    double acc = 0;
    for (int k = 0; k < nb; ++k) {
        double c = counts[k];
        if (cumulative) {
            acc += c;
            c = acc;
        }
        if (normalize && total > 0) c = cumulative ? c / total : c / (total * (bins[k + 1] - bins[k]));
        out.x.push_back(centered ? 0.5 * (bins[k] + bins[k + 1]) : bins[k + 1]);
        out.y.push_back(c);
    }
    return out;
}

void fft(QVector<double>& re, QVector<double>& im, bool inverse)
{
    const int n = re.size();
    if (n <= 1) return;
    const double sign = inverse ? 1.0 : -1.0;
    if ((n & (n - 1)) == 0) {
        // iterative radix-2
        for (int i = 1, j = 0; i < n; ++i) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        for (int len = 2; len <= n; len <<= 1) {
            const double ang = sign * 2 * M_PI / len;
            const double wr = std::cos(ang), wi = std::sin(ang);
            for (int i = 0; i < n; i += len) {
                double cr = 1, ci = 0;
                for (int k = 0; k < len / 2; ++k) {
                    const int a = i + k, b = i + k + len / 2;
                    const double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                    const double nr = cr * wr - ci * wi;
                    ci = cr * wi + ci * wr;
                    cr = nr;
                }
            }
        }
        return;
    }
    // Bluestein chirp-z for arbitrary n
    int m = 1;
    while (m < 2 * n - 1) m <<= 1;
    QVector<double> wr(n), wi(n);
    for (int k = 0; k < n; ++k) {
        const double a = sign * M_PI * double((static_cast<long long>(k) * k) % (2LL * n)) / n;
        wr[k] = std::cos(a);
        wi[k] = std::sin(a);
    }
    QVector<double> ar(m, 0.0), ai(m, 0.0), br(m, 0.0), bi(m, 0.0);
    for (int k = 0; k < n; ++k) {
        ar[k] = re[k] * wr[k] - im[k] * wi[k];
        ai[k] = re[k] * wi[k] + im[k] * wr[k];
    }
    br[0] = wr[0];
    bi[0] = -wi[0];
    for (int k = 1; k < n; ++k) {
        br[k] = br[m - k] = wr[k];
        bi[k] = bi[m - k] = -wi[k];
    }
    fft(ar, ai, false);
    fft(br, bi, false);
    for (int k = 0; k < m; ++k) {
        const double r = ar[k] * br[k] - ai[k] * bi[k];
        const double i = ar[k] * bi[k] + ai[k] * br[k];
        ar[k] = r;
        ai[k] = i;
    }
    fft(ar, ai, true);
    for (int k = 0; k < n; ++k) {
        const double r = ar[k] / m, i = ai[k] / m;
        re[k] = r * wr[k] - i * wi[k];
        im[k] = r * wi[k] + i * wr[k];
    }
}

XY fourier(const QVector<double>& x, const QVector<double>& y, const FftOptions& o)
{
    XY out;
    const int n = nmin(x, y);
    if (n < 2) return out;
    QVector<double> re(n), im(n, 0.0);
    for (int i = 0; i < n; ++i) {
        if (o.complexInput) {
            re[i] = x[i];
            im[i] = y[i];
        } else {
            re[i] = y[i];
        }
        double w = 1.0;
        const double t = double(i) / (n - 1);
        switch (o.window) {
        case FftWindow::None: break;
        case FftWindow::Triangular: w = 1.0 - std::fabs(2.0 * t - 1.0); break;
        case FftWindow::Hanning: w = 0.5 - 0.5 * std::cos(2 * M_PI * t); break;
        case FftWindow::Welch: w = 1.0 - std::pow(2.0 * t - 1.0, 2); break;
        case FftWindow::Hamming: w = 0.54 - 0.46 * std::cos(2 * M_PI * t); break;
        case FftWindow::Blackman: w = 0.42 - 0.5 * std::cos(2 * M_PI * t) + 0.08 * std::cos(4 * M_PI * t); break;
        case FftWindow::Parzen: w = 1.0 - std::fabs((i - 0.5 * (n - 1)) / (0.5 * (n + 1))); break;
        }
        re[i] *= w;
        im[i] *= w;
    }
    fft(re, im, o.inverse);
    const double dx = o.complexInput ? 1.0 : (x[n - 1] - x[0]) / (n - 1);
    const int count = (!o.complexInput && o.halfSpectrum && !o.inverse) ? n / 2 + 1 : n;
    const double norm = o.normalize ? (o.inverse ? 1.0 : 1.0 / n) : 1.0;
    for (int k = 0; k < count; ++k) {
        const double r = re[k] * norm, i = im[k] * norm;
        double v = 0;
        switch (o.output) {
        case FftOutput::Magnitude: v = std::hypot(r, i) * ((o.halfSpectrum && k > 0 && k < n - k) ? 2.0 : 1.0); break;
        case FftOutput::Phase: v = std::atan2(i, r); break;
        case FftOutput::Power: v = r * r + i * i; break;
        case FftOutput::Real: v = r; break;
        case FftOutput::Imaginary: v = i; break;
        case FftOutput::DB: v = 10.0 * std::log10(std::max(1e-300, r * r + i * i)); break;
        }
        double xv = k;
        if (o.xaxis == FftXAxis::Frequency) xv = (dx > 0) ? k / (n * dx) : k;
        else if (o.xaxis == FftXAxis::Period) {
            if (k == 0) continue;
            xv = (dx > 0) ? (n * dx) / k : double(n) / k;
        }
        out.x.push_back(xv);
        out.y.push_back(v);
    }
    return out;
}

XY crossCorrelation(const QVector<double>& a, const QVector<double>& b, int maxLag, bool covariance)
{
    XY out;
    const int n = nmin(a, b);
    if (n < 2) return out;
    maxLag = std::clamp(maxLag, 0, n - 1);
    const double ma = std::accumulate(a.begin(), a.begin() + n, 0.0) / n;
    const double mb = std::accumulate(b.begin(), b.begin() + n, 0.0) / n;
    double sa = 0, sb = 0;
    for (int i = 0; i < n; ++i) {
        sa += (a[i] - ma) * (a[i] - ma);
        sb += (b[i] - mb) * (b[i] - mb);
    }
    const double denom = covariance ? double(n) : std::sqrt(sa * sb);
    for (int lag = 0; lag <= maxLag; ++lag) {
        double s = 0;
        for (int i = 0; i + lag < n; ++i) s += (a[i] - ma) * (b[i + lag] - mb);
        out.x.push_back(lag);
        out.y.push_back(denom > 0 ? s / denom : 0.0);
    }
    return out;
}

XY linearConvolution(const QVector<double>& a, const QVector<double>& b)
{
    XY out;
    const int na = a.size(), nb = b.size();
    if (!na || !nb) return out;
    for (int k = 0; k < na + nb - 1; ++k) {
        double s = 0;
        for (int i = std::max(0, k - nb + 1); i <= std::min(k, na - 1); ++i) s += a[i] * b[k - i];
        out.x.push_back(k);
        out.y.push_back(s);
    }
    return out;
}

XY digitalFilter(const QVector<double>& x, const QVector<double>& y, const QVector<double>& w)
{
    XY out;
    const int n = nmin(x, y), m = w.size();
    if (m < 1 || n < m) return out;
    double ws = std::accumulate(w.begin(), w.end(), 0.0);
    if (ws == 0) ws = 1.0;
    for (int i = 0; i + m <= n; ++i) {
        double s = 0;
        for (int k = 0; k < m; ++k) s += w[k] * y[i + k];
        out.x.push_back(x[i + m / 2]);
        out.y.push_back(s / ws);
    }
    return out;
}

XY geometric(const QVector<double>& x, const QVector<double>& y, double rotDeg, double cx, double cy, double sx, double sy,
             double tx, double ty)
{
    XY out;
    const int n = nmin(x, y);
    const double a = rotDeg * M_PI / 180.0, c = std::cos(a), s = std::sin(a);
    for (int i = 0; i < n; ++i) {
        const double dx = x[i] - cx, dy = y[i] - cy;
        const double rx = c * dx - s * dy, ry = s * dx + c * dy;
        out.x.push_back(cx + rx * sx + tx);
        out.y.push_back(cy + ry * sy + ty);
    }
    return out;
}

XY sampleEvery(const QVector<double>& x, const QVector<double>& y, int start, int step)
{
    XY out;
    const int n = nmin(x, y);
    step = std::max(1, step);
    for (int i = std::max(0, start); i < n; i += step) {
        out.x.push_back(x[i]);
        out.y.push_back(y[i]);
    }
    return out;
}

XY prune(const QVector<double>& x, const QVector<double>& y, PruneType t, double dx, double dy, bool relative)
{
    XY out;
    const int n = nmin(x, y);
    if (n == 0) return out;
    out.x.push_back(x[0]);
    out.y.push_back(y[0]);
    auto rel = [&](double v, double d, double ref) { return relative ? d * std::fabs(ref) : d; };
    for (int i = 1; i < n; ++i) {
        const double px = out.x.last(), py = out.y.last();
        bool drop = false;
        const double ddx = std::fabs(x[i] - px), ddy = std::fabs(y[i] - py);
        const double ex = rel(x[i], dx, px), ey = rel(y[i], dy, py);
        switch (t) {
        case PruneType::Circle: drop = std::hypot(ddx, ddy) < ex; break;
        case PruneType::Ellipse: drop = (ex > 0 && ey > 0) && (ddx * ddx / (ex * ex) + ddy * ddy / (ey * ey) < 1.0); break;
        case PruneType::Rectangle: drop = ddx < ex && ddy < ey; break;
        case PruneType::Interpolation:
            if (i + 1 < n) {
                const double f = (x[i + 1] != px) ? (x[i] - px) / (x[i + 1] - px) : 0.0;
                const double yi = py + f * (y[i + 1] - py);
                drop = std::fabs(yi - y[i]) < rel(y[i], dy, y[i]);
            }
            break;
        }
        if (!drop || i == n - 1) {
            out.x.push_back(x[i]);
            out.y.push_back(y[i]);
        }
    }
    return out;
}

QVector<int> sortPermutation(const QVector<double>& x, const QVector<double>& y, SortKey key, bool descending)
{
    const int n = nmin(x, y);
    QVector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    const QVector<double>& k = key == SortKey::X ? x : y;
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return descending ? k[a] > k[b] : k[a] < k[b]; });
    return idx;
}

Stats statistics(const QVector<double>& v)
{
    Stats s;
    QVector<double> f;
    for (int i = 0; i < v.size(); ++i) {
        if (!std::isfinite(v[i])) continue;
        if (f.isEmpty() || v[i] < s.min) { s.min = v[i]; s.imin = i; }
        if (f.isEmpty() || v[i] > s.max) { s.max = v[i]; s.imax = i; }
        f.push_back(v[i]);
    }
    s.n = f.size();
    if (!s.n) return s;
    s.sum = std::accumulate(f.begin(), f.end(), 0.0);
    s.mean = s.sum / s.n;
    double ss = 0;
    for (double d : f) ss += (d - s.mean) * (d - s.mean);
    s.sd = s.n > 1 ? std::sqrt(ss / (s.n - 1)) : 0.0;
    std::sort(f.begin(), f.end());
    s.median = (s.n % 2) ? f[s.n / 2] : 0.5 * (f[s.n / 2 - 1] + f[s.n / 2]);
    return s;
}

QStringList featureNames()
{
    return {QStringLiteral("Y minimum"),       QStringLiteral("Y maximum"),     QStringLiteral("Y average"),
            QStringLiteral("Y std. dev."),     QStringLiteral("Y median"),      QStringLiteral("Number of points"),
            QStringLiteral("X of maximum"),    QStringLiteral("X of minimum"),  QStringLiteral("Integral"),
            QStringLiteral("Frequency"),       QStringLiteral("Period"),        QStringLiteral("Zero crossing"),
            QStringLiteral("Rise time"),       QStringLiteral("Fall time"),     QStringLiteral("Slope"),
            QStringLiteral("Y intercept"),     QStringLiteral("Set length"),    QStringLiteral("Half maximum width"),
            QStringLiteral("Barycenter X"),    QStringLiteral("Barycenter Y")};
}

double feature(const QVector<double>& x, const QVector<double>& y, const QString& name)
{
    const int n = nmin(x, y);
    if (n == 0) return NAN;
    const Stats sy = statistics(y);
    const QString f = name.toLower();
    if (f == QLatin1String("y minimum")) return sy.min;
    if (f == QLatin1String("y maximum")) return sy.max;
    if (f == QLatin1String("y average")) return sy.mean;
    if (f == QLatin1String("y std. dev.")) return sy.sd;
    if (f == QLatin1String("y median")) return sy.median;
    if (f == QLatin1String("number of points")) return n;
    if (f == QLatin1String("x of maximum")) return sy.imax >= 0 ? x[sy.imax] : NAN;
    if (f == QLatin1String("x of minimum")) return sy.imin >= 0 ? x[sy.imin] : NAN;
    if (f == QLatin1String("integral")) { double t = 0; integrate(x, y, &t); return t; }
    if (f == QLatin1String("set length")) {
        double L = 0;
        for (int i = 1; i < n; ++i) L += std::hypot(x[i] - x[i - 1], y[i] - y[i - 1]);
        return L;
    }
    if (f == QLatin1String("barycenter x") || f == QLatin1String("barycenter y")) {
        double sw = 0, sx = 0, syy = 0;
        for (int i = 0; i < n; ++i) { sw += y[i]; sx += x[i] * y[i]; syy += y[i] * y[i]; }
        if (sw == 0) return NAN;
        return f.endsWith(QLatin1Char('x')) ? sx / sw : syy / sw;
    }
    if (f == QLatin1String("zero crossing")) {
        for (int i = 1; i < n; ++i)
            if ((y[i - 1] <= 0 && y[i] > 0) || (y[i - 1] >= 0 && y[i] < 0))
                return x[i - 1] - y[i - 1] * (x[i] - x[i - 1]) / (y[i] - y[i - 1]);
        return NAN;
    }
    if (f == QLatin1String("frequency") || f == QLatin1String("period")) {
        // mean spacing of upward mean-crossings
        QVector<double> cross;
        for (int i = 1; i < n; ++i)
            if (y[i - 1] < sy.mean && y[i] >= sy.mean)
                cross.push_back(x[i - 1] + (sy.mean - y[i - 1]) * (x[i] - x[i - 1]) / (y[i] - y[i - 1]));
        if (cross.size() < 2) return NAN;
        const double period = (cross.last() - cross.first()) / (cross.size() - 1);
        return f == QLatin1String("period") ? period : 1.0 / period;
    }
    auto levelCross = [&](double level, bool rising, int from) {
        for (int i = std::max(1, from); i < n; ++i) {
            const bool c = rising ? (y[i - 1] < level && y[i] >= level) : (y[i - 1] > level && y[i] <= level);
            if (c) return x[i - 1] + (level - y[i - 1]) * (x[i] - x[i - 1]) / (y[i] - y[i - 1]);
        }
        return double(NAN);
    };
    if (f == QLatin1String("rise time") || f == QLatin1String("fall time")) {
        const bool rise = f.startsWith(QLatin1String("rise"));
        const double lo = sy.min + 0.1 * (sy.max - sy.min), hi = sy.min + 0.9 * (sy.max - sy.min);
        const double a = levelCross(rise ? lo : hi, rise, 1), b = levelCross(rise ? hi : lo, rise, 1);
        return b - a;
    }
    if (f == QLatin1String("half maximum width")) {
        const double half = sy.min + 0.5 * (sy.max - sy.min);
        const double a = levelCross(half, true, 1);
        const double b = levelCross(half, false, sy.imax + 1);
        return b - a;
    }
    if (f == QLatin1String("slope") || f == QLatin1String("y intercept")) {
        double sx = 0, syv = 0, sxx = 0, sxy = 0;
        for (int i = 0; i < n; ++i) { sx += x[i]; syv += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; }
        const double d = n * sxx - sx * sx;
        if (d == 0) return NAN;
        const double b = (n * sxy - sx * syv) / d;
        return f == QLatin1String("slope") ? b : (syv - b * sx) / n;
    }
    return NAN;
}

}  // namespace cactus::transforms
