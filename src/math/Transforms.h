// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - data transformations (the Grace "Data > Transformations" menu).
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace cactus::transforms {

struct XY {
    QVector<double> x, y;
};

// ----------------------------------------------------------------- running statistics
enum class RunStat { Average, Median, Min, Max, StdDev };
XY running(const QVector<double>& x, const QVector<double>& y, int window, RunStat stat);

// ----------------------------------------------------------------- differences / integrals
enum class DiffType { Forward, Backward, Centered };
XY differentiate(const QVector<double>& x, const QVector<double>& y, DiffType t);
/// Cumulative trapezoidal integral; *total receives the full integral.
XY integrate(const QVector<double>& x, const QVector<double>& y, double* total = nullptr);
XY seasonalDifference(const QVector<double>& x, const QVector<double>& y, int period);
XY cumulativeSum(const QVector<double>& x, const QVector<double>& y);

// ----------------------------------------------------------------- interpolation
enum class InterpMethod { Linear, Spline, Akima };
/// Interpolate (x, y) at the points of `mesh`. With strict, points outside the data range are dropped.
XY interpolate(const QVector<double>& x, const QVector<double>& y, const QVector<double>& mesh, InterpMethod m,
               bool strict = true);
QVector<double> linspace(double a, double b, int n);

// ----------------------------------------------------------------- histogram
/// Histogram on bin edges `bins` (n+1 edges -> n bins). Points at the upper bin edge (Grace) unless centered.
XY histogram(const QVector<double>& data, const QVector<double>& bins, bool cumulative, bool normalize,
             bool centered = false);

// ----------------------------------------------------------------- Fourier
enum class FftWindow { None, Triangular, Hanning, Welch, Hamming, Blackman, Parzen };
enum class FftOutput { Magnitude, Phase, Power, Real, Imaginary, DB };
enum class FftXAxis { Index, Frequency, Period };
struct FftOptions {
    bool inverse = false;
    bool complexInput = false;  ///< x = real part, y = imaginary part
    FftWindow window = FftWindow::None;
    FftOutput output = FftOutput::Magnitude;
    FftXAxis xaxis = FftXAxis::Frequency;
    bool halfSpectrum = true;   ///< only non-negative frequencies for real input
    bool normalize = true;
};
XY fourier(const QVector<double>& x, const QVector<double>& y, const FftOptions& o);
/// In-place complex FFT of arbitrary length (radix-2 or Bluestein).
void fft(QVector<double>& re, QVector<double>& im, bool inverse);

// ----------------------------------------------------------------- correlation / convolution
XY crossCorrelation(const QVector<double>& y1, const QVector<double>& y2, int maxLag, bool covariance);
XY linearConvolution(const QVector<double>& y1, const QVector<double>& y2);
XY digitalFilter(const QVector<double>& x, const QVector<double>& y, const QVector<double>& weights);

// ----------------------------------------------------------------- geometry and sampling
XY geometric(const QVector<double>& x, const QVector<double>& y, double rotDeg, double cx, double cy, double sx,
             double sy, double tx, double ty);
XY sampleEvery(const QVector<double>& x, const QVector<double>& y, int start, int step);
enum class PruneType { Interpolation, Circle, Ellipse, Rectangle };
XY prune(const QVector<double>& x, const QVector<double>& y, PruneType t, double dx, double dy, bool relative);
enum class SortKey { X, Y };
/// Sorting returns the permutation applied (use it to reorder other columns).
QVector<int> sortPermutation(const QVector<double>& x, const QVector<double>& y, SortKey key, bool descending);

// ----------------------------------------------------------------- feature extraction
QStringList featureNames();
/// Compute a named feature of one curve; returns NaN if undefined.
double feature(const QVector<double>& x, const QVector<double>& y, const QString& name);

// ----------------------------------------------------------------- statistics
struct Stats {
    int n = 0;
    double mean = 0, sd = 0, min = 0, max = 0, median = 0, sum = 0;
    int imin = -1, imax = -1;
};
Stats statistics(const QVector<double>& v);

}  // namespace cactus::transforms
