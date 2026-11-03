// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - special mathematical functions (Cephes-compatible semantics as
// used by Grace). Implemented from scratch so that every platform (including
// Apple's libc++, which lacks the C++17 special math functions) behaves alike.
#pragma once

#include <functional>

namespace cactus::special {

/// Adaptive Gauss-Kronrod (7-15) quadrature on [a, b].
double integrate(const std::function<double(double)>& f, double a, double b, double tol = 1e-12, int maxDepth = 40);
/// Integral on [a, +inf).
double integrateToInf(const std::function<double(double)>& f, double a, double tol = 1e-12);

// gamma family
double rgamma(double x);
double psi(double x);
double beta(double a, double b);
double lbeta(double a, double b);
double igam(double a, double x);   ///< regularised lower incomplete gamma P(a, x)
double igamc(double a, double x);  ///< regularised upper incomplete gamma Q(a, x)
double igami(double a, double y);  ///< x such that igamc(a, x) = y
double incbet(double a, double b, double x);
double incbi(double a, double b, double y);

// distributions
double ndtr(double x);
double ndtri(double y);
double chdtr(double df, double x);
double chdtrc(double df, double x);
double chdtri(double df, double y);
double fdtr(double df1, double df2, double x);
double fdtrc(double df1, double df2, double x);
double fdtri(double df1, double df2, double y);
double gdtr(double a, double b, double x);
double gdtrc(double a, double b, double x);
double pdtr(double k, double m);
double pdtrc(double k, double m);
double pdtri(double k, double y);
double stdtr(double k, double t);
double stdtri(double k, double p);

// Bessel and Airy
double jv(double v, double x);
double yv(double v, double x);
double iv(double v, double x);
double kv(double v, double x);
double i0e(double x);
double i1e(double x);
double k0e(double x);
double k1e(double x);
double airyAi(double x);
double airyBi(double x);
double struve(double v, double x);

// integrals
double dawsn(double x);
double ellik(double phi, double m);
double ellie(double phi, double m);
double ellpk(double m1);  ///< Cephes convention: K(m) with m = 1 - m1
double ellpe(double m1);  ///< Cephes convention: E(m) with m = 1 - m1
double expn(double n, double x);
double fresnls(double x);
double fresnlc(double x);
double si(double x);
double ci(double x);
double shi(double x);
double chi(double x);
double spence(double x);

// hypergeometric, zeta, misc
double hyp2f1(double a, double b, double c, double x);
double hyperg(double a, double b, double x);
double zeta(double x, double q);
double zetac(double x);
double voigt(double gamma, double sigma, double x);

}  // namespace cactus::special
