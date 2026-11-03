// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>
#include <cmath>

#include "math/Expr.h"
#include "math/Fit.h"
#include "math/SpecialFunctions.h"
#include "math/Transforms.h"
#include "plot/Ticks.h"

using namespace cactus;

class TestMath : public QObject {
    Q_OBJECT

    static double ev(const QString& s)
    {
        expr::Context ctx;
        expr::Value v;
        QString err;
        if (!expr::evaluate(s, ctx, &v, &err)) {
            qWarning() << s << err;
            return NAN;
        }
        return v.scalar();
    }

private slots:
    void expressions()
    {
        QCOMPARE(ev("1 + 2 * 3"), 7.0);
        QCOMPARE(ev("-2^2"), -4.0);
        QCOMPARE(ev("2^3^2"), 512.0);
        QCOMPARE(ev("2**3"), 8.0);
        QCOMPARE(ev("(1 + 2) * 3"), 9.0);
        QCOMPARE(ev("7 % 4"), 3.0);
        QCOMPARE(ev("1 < 2 && 3 > 2"), 1.0);
        QCOMPARE(ev("1 LT 2 AND NOT 0"), 1.0);
        QCOMPARE(ev("1 == 1 ? 10 : 20"), 10.0);
        QCOMPARE(ev("abs(-3) + sqrt(16) + sqr(2)"), 11.0);
        QCOMPARE(ev("fac(5)"), 120.0);
        QCOMPARE(ev("maxof(3, 7) + minof(3, 7)"), 10.0);
        QVERIFY(std::fabs(ev("pi") - M_PI) < 1e-15);
        QVERIFY(std::fabs(ev("sin(pi/2)") - 1.0) < 1e-15);
        QCOMPARE(ev("sum(mesh(1, 10, 10))"), 55.0);
        QCOMPARE(ev("avg(mesh(5))"), 2.0);
        QCOMPARE(ev("max(mesh(0, 1, 11))"), 1.0);
        QCOMPARE(ev("imax(mesh(0, 1, 11))"), 10.0);
        QCOMPARE(ev("mesh(0, 10, 11)[3]"), 3.0);
        QCOMPARE(ev("int(mesh(0, 1, 101), mesh(0, 1, 101))"), 0.5);
        QVERIFY(std::isnan(ev("sqrt(-1)")));
    }

    void vectors()
    {
        expr::Context ctx;
        ctx.setVector(QStringLiteral("x"), {1, 2, 3});
        expr::Value v;
        QVERIFY(expr::evaluate(QStringLiteral("x^2 + 1"), ctx, &v));
        QVERIFY(v.isVector);
        QCOMPARE(v.v, (QVector<double>{2, 5, 10}));
        QVERIFY(expr::evaluate(QStringLiteral("x > 1 ? x : -x"), ctx, &v));
        QCOMPARE(v.v, (QVector<double>{-1, 2, 3}));
        QVERIFY(expr::evaluate(QStringLiteral("rsum(x)"), ctx, &v));
        QCOMPARE(v.v, (QVector<double>{1, 3, 6}));
    }

    void errors()
    {
        expr::Context ctx;
        expr::Value v;
        QString err;
        QVERIFY(!expr::evaluate(QStringLiteral("1 +"), ctx, &v, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!expr::evaluate(QStringLiteral("nosuchfunc(1)"), ctx, &v, &err));
        QVERIFY(!expr::evaluate(QStringLiteral("undefinedvar + 1"), ctx, &v, &err));
    }

    void specialFunctions_data()
    {
        QTest::addColumn<QString>("expr");
        QTest::addColumn<double>("expected");
        QTest::addColumn<double>("tol");
        QTest::newRow("gamma") << "gamma(5)" << 24.0 << 1e-12;
        QTest::newRow("lgamma") << "lgamma(10)" << 12.801827480081469 << 1e-12;
        QTest::newRow("psi") << "psi(1)" << -0.5772156649015329 << 1e-12;
        QTest::newRow("beta") << "beta(2, 3)" << 1.0 / 12.0 << 1e-12;
        QTest::newRow("erf") << "erf(0.5)" << 0.5204998778130465 << 1e-14;
        QTest::newRow("ndtr") << "ndtr(1.96)" << 0.9750021048517795 << 1e-12;
        QTest::newRow("ndtri") << "ndtri(0.975)" << 1.959963984540054 << 1e-9;
        QTest::newRow("igam") << "igam(1, 1)" << 1.0 - std::exp(-1.0) << 1e-12;
        QTest::newRow("igamc") << "igamc(2.5, 3)" << 0.30621891841327875 << 1e-10;
        QTest::newRow("incbet") << "incbet(2, 3, 0.4)" << 0.5248 << 1e-10;
        QTest::newRow("chdtr") << "chdtr(3, 7.814727903251178)" << 0.95 << 1e-8;
        QTest::newRow("stdtr") << "stdtr(10, 2.228138851986273)" << 0.975 << 1e-8;
        QTest::newRow("stdtri") << "stdtri(10, 0.975)" << 2.228138851986273 << 1e-7;
        QTest::newRow("fdtr") << "fdtr(3, 10, 3.708264819)" << 0.95 << 1e-7;
        QTest::newRow("j0") << "jv(0, 1)" << 0.7651976865579666 << 1e-11;
        QTest::newRow("j1") << "jv(1, 2.5)" << 0.4970941024642741 << 1e-11;
        QTest::newRow("j2_big") << "jv(2, 30)" << 0.07845124607326538 << 1e-9;
        QTest::newRow("jhalf") << "jv(0.5, 1)" << std::sqrt(2.0 / M_PI) * std::sin(1.0) << 1e-10;
        QTest::newRow("y0") << "yv(0, 1)" << 0.08825696421567697 << 1e-9;
        QTest::newRow("y1") << "yv(1, 3)" << 0.3246744247918000 << 1e-9;
        QTest::newRow("i0") << "iv(0, 1)" << 1.2660658777520082 << 1e-11;
        QTest::newRow("k0") << "kv(0, 1)" << 0.42102443824070834 << 1e-9;
        QTest::newRow("k1") << "kn(1, 2)" << 0.13986588181652243 << 1e-9;
        QTest::newRow("i0e") << "i0e(1)" << 1.2660658777520082 * std::exp(-1.0) << 1e-11;
        QTest::newRow("ai0") << "ai(0)" << 0.3550280538878172 << 1e-12;
        QTest::newRow("ai1") << "ai(1)" << 0.1352924163128814 << 1e-10;
        QTest::newRow("ai3") << "ai(3)" << 0.006591139357460719 << 1e-9;
        QTest::newRow("bi1") << "bi(1)" << 1.2074235949528713 << 1e-10;
        QTest::newRow("ai_neg") << "ai(-8)" << -0.05270505168740 << 1e-6;
        QTest::newRow("zeta") << "zeta(2, 1)" << M_PI * M_PI / 6.0 << 1e-12;
        QTest::newRow("zetac") << "zetac(3)" << 0.2020569031595942 << 1e-12;
        QTest::newRow("fresnls") << "fresnls(1)" << 0.4382591473903548 << 1e-10;
        QTest::newRow("fresnlc") << "fresnlc(1)" << 0.7798934003768228 << 1e-10;
        QTest::newRow("si") << "si(1)" << 0.946083070367183 << 1e-12;
        QTest::newRow("ci") << "ci(1)" << 0.3374039229009681 << 1e-12;
        QTest::newRow("shi") << "shi(1)" << 1.0572508753757285 << 1e-12;
        QTest::newRow("chi") << "chi(1)" << 0.8378669409802082 << 1e-12;
        QTest::newRow("expn") << "expn(1, 1)" << 0.21938393439552029 << 1e-10;
        QTest::newRow("ellpk") << "ellpk(0.5)" << 1.8540746773013719 << 1e-12;
        QTest::newRow("ellpe") << "ellpe(0.5)" << 1.3506438810476755 << 1e-12;
        QTest::newRow("dawsn") << "dawsn(1)" << 0.5380795069127684 << 1e-12;
        QTest::newRow("spence") << "spence(0.5)" << 0.5822405264650125 << 1e-12;
        QTest::newRow("hyp2f1") << "hyp2f1(1, 1, 2, 0.5)" << 1.3862943611198906 << 1e-12;
        QTest::newRow("hyperg") << "hyperg(1, 2, 1)" << 1.718281828459045 << 1e-12;
        QTest::newRow("struve") << "struve(0, 1)" << 0.5686566270482879 << 1e-10;
        QTest::newRow("voigt_lorentz") << "voigt(1, 0, 0)" << 1.0 / M_PI << 1e-12;
        QTest::newRow("voigt") << "voigt(0.5, 1, 0)" << 0.27895547038929436 << 1e-8;
    }

    void specialFunctions()
    {
        QFETCH(QString, expr);
        QFETCH(double, expected);
        QFETCH(double, tol);
        const double got = ev(expr);
        if (std::fabs(got - expected) > tol * std::max(1.0, std::fabs(expected)))
            QFAIL(qPrintable(QStringLiteral("%1 = %2, expected %3").arg(expr).arg(got, 0, 'g', 16).arg(expected, 0, 'g', 16)));
    }

    void fftMatchesDft()
    {
        for (int n : {8, 12, 17, 64, 100}) {
            QVector<double> re(n), im(n), r0(n), i0(n);
            for (int i = 0; i < n; ++i) {
                re[i] = r0[i] = std::sin(0.3 * i) + 0.1 * i;
                im[i] = i0[i] = std::cos(0.7 * i);
            }
            transforms::fft(re, im, false);
            for (int k = 0; k < n; ++k) {
                double sr = 0, si = 0;
                for (int j = 0; j < n; ++j) {
                    const double a = -2 * M_PI * k * j / n;
                    sr += r0[j] * std::cos(a) - i0[j] * std::sin(a);
                    si += r0[j] * std::sin(a) + i0[j] * std::cos(a);
                }
                QVERIFY2(std::fabs(sr - re[k]) < 1e-9 && std::fabs(si - im[k]) < 1e-9, qPrintable(QString::number(n)));
            }
            transforms::fft(re, im, true);
            for (int i = 0; i < n; ++i) QVERIFY(std::fabs(re[i] / n - r0[i]) < 1e-10);
        }
    }

    void fourierPeak()
    {
        QVector<double> x, y;
        for (int i = 0; i < 200; ++i) {
            x << i * 0.01;
            y << std::sin(2 * M_PI * 10.0 * i * 0.01);
        }
        transforms::FftOptions o;
        const auto r = transforms::fourier(x, y, o);
        int imax = 0;
        for (int i = 1; i < r.y.size(); ++i) if (r.y[i] > r.y[imax]) imax = i;
        QVERIFY(std::fabs(r.x[imax] - 10.0) < 0.5);
        QVERIFY(std::fabs(r.y[imax] - 1.0) < 0.2);
    }

    void interpolation()
    {
        QVector<double> x, y;
        for (int i = 0; i <= 20; ++i) {
            x << i * 0.25;
            y << std::sin(i * 0.25);
        }
        const auto mesh = transforms::linspace(0, 5, 51);
        for (auto m : {transforms::InterpMethod::Linear, transforms::InterpMethod::Spline, transforms::InterpMethod::Akima}) {
            const auto r = transforms::interpolate(x, y, mesh, m);
            QCOMPARE(r.x.size(), 51);
            double err = 0;
            for (int i = 0; i < r.x.size(); ++i) err = std::max(err, std::fabs(r.y[i] - std::sin(r.x[i])));
            QVERIFY2(err < (m == transforms::InterpMethod::Linear ? 1e-2 : 5e-3), qPrintable(QString::number(err)));
        }
    }

    void histogramAndIntegral()
    {
        const auto h = transforms::histogram({0.1, 0.2, 0.25, 0.7, 0.9, 1.0}, transforms::linspace(0, 1, 5), false, false);
        QCOMPARE(h.y, (QVector<double>{2, 1, 1, 2}));
        QCOMPARE(h.x.last(), 1.0);
        double total = 0;
        transforms::integrate(transforms::linspace(0, 1, 1001), transforms::linspace(0, 2, 1001), &total);
        QVERIFY(std::fabs(total - 1.0) < 1e-12);
        const auto run = transforms::running({1, 2, 3, 4, 5}, {1, 2, 3, 4, 5}, 3, transforms::RunStat::Average);
        QCOMPARE(run.y, (QVector<double>{2, 3, 4}));
        const auto conv = transforms::linearConvolution({1, 2, 3}, {0, 1, 0.5});
        QCOMPARE(conv.y, (QVector<double>{0, 1, 2.5, 4, 1.5}));
    }

    void nistMisra1a()
    {
        // NIST StRD Misra1a, start 1; certified values b1 = 2.3894212918E+02, b2 = 5.5015643181E-04
        const QVector<double> x{77.6, 114.9, 141.1, 190.8, 239.9, 289.0, 332.8, 378.4, 434.8, 477.3, 536.8, 593.1, 689.1, 760.0};
        const QVector<double> y{10.07, 14.73, 17.94, 23.93, 29.61, 35.18, 40.02, 44.82, 50.76, 55.05, 61.01, 66.40, 75.47, 81.78};
        fit::Options o;
        o.formula = QStringLiteral("y = a0 * (1 - exp(-a1 * x))");
        o.nparams = 2;
        o.params[0].value = 500;
        o.params[1].value = 1e-4;
        o.maxIterations = 1000;
        o.tolerance = 1e-14;
        const auto r = fit::nonlinear(x, y, {}, o);
        QVERIFY(r.ok);
        QVERIFY2(std::fabs(r.values[0] / 2.3894212918E+02 - 1) < 1e-6, qPrintable(QString::number(r.values[0], 'g', 12)));
        QVERIFY2(std::fabs(r.values[1] / 5.5015643181E-04 - 1) < 1e-6, qPrintable(QString::number(r.values[1], 'g', 12)));
        // certified standard deviations: 2.7070075241E+00, 7.2668688436E-06
        QVERIFY(std::fabs(r.errors[0] / 2.7070075241 - 1) < 1e-3);
        QVERIFY(std::fabs(r.errors[1] / 7.2668688436E-06 - 1) < 1e-3);
    }

    void nistThurberStyleBounds()
    {
        // a bounded fit must respect constraints
        QVector<double> x, y;
        for (int i = 0; i < 50; ++i) {
            x << i;
            y << 3.0 * std::exp(-0.1 * i) + 1.0;
        }
        fit::Options o;
        o.formula = QStringLiteral("a0*exp(-a1*x) + a2");
        o.nparams = 3;
        o.params[0].value = 1;
        o.params[1].value = 0.5;
        o.params[2].value = 0;
        o.params[2].constrained = true;
        o.params[2].lo = 0;
        o.params[2].hi = 0.5;
        const auto r = fit::nonlinear(x, y, {}, o);
        QVERIFY(r.ok);
        QVERIFY(r.values[2] <= 0.5 + 1e-12 && r.values[2] >= -1e-12);
    }

    void regressions()
    {
        QVector<double> x, y, ye;
        for (int i = 1; i <= 20; ++i) {
            x << i;
            y << 2 - 3 * i + 0.5 * i * i;
            ye << 4.0 * std::exp(0.2 * i);
        }
        const auto p = fit::regression(x, y, fit::Regression::Polynomial, 2);
        QVERIFY(p.ok);
        QVERIFY(std::fabs(p.coefficients[0] - 2) < 1e-8);
        QVERIFY(std::fabs(p.coefficients[1] + 3) < 1e-9);
        QVERIFY(std::fabs(p.coefficients[2] - 0.5) < 1e-10);
        QVERIFY(std::fabs(p.r2 - 1.0) < 1e-12);
        const auto e = fit::regression(x, ye, fit::Regression::Exponential);
        QVERIFY(std::fabs(e.coefficients[0] - 4) < 1e-9 && std::fabs(e.coefficients[1] - 0.2) < 1e-12);
    }

    void niceTicks()
    {
        QCOMPARE(ticks::niceNumber(0.23, true), 0.25);
        QCOMPARE(ticks::niceNumber(0.17, true), 0.2);
        QCOMPARE(ticks::niceNumber(7.9, true), 10.0);
        double a = 0.13, b = 9.7;
        ticks::niceRange(&a, &b, ScaleType::Linear, 6);
        QCOMPARE(a, 0.0);
        QCOMPARE(b, 10.0);
        a = 3; b = 4.2e5;
        ticks::niceRange(&a, &b, ScaleType::Log, 6);
        QCOMPARE(a, 1.0);
        QCOMPARE(b, 1e6);
        Axis ax;
        ax.major = 2;
        ax.minorTicks = 1;
        const auto t = ticks::compute(ax, Scale(ScaleType::Linear, 0, 10));
        int majors = 0;
        for (const Tick& k : t) if (k.major) ++majors;
        QCOMPARE(majors, 6);
        QCOMPARE(int(t.size()), 11);
    }

    void labelFormats()
    {
        ticks::FormatOptions o;
        o.pretty = false;
        o.format = LabelFormat::Decimal;
        o.precision = 2;
        QCOMPARE(ticks::formatValue(3.14159, o), QStringLiteral("3.14"));
        o.format = LabelFormat::General;
        o.precision = 5;
        QCOMPARE(ticks::formatValue(1e6, o), QStringLiteral("1e+06"));
        o.pretty = true;
        QCOMPARE(ticks::formatValue(1e6, o), QStringLiteral("10\\S6\\N"));
        QCOMPARE(ticks::formatValue(-2.5, o), QString(QChar(0x2212)) + QStringLiteral("2.5"));
        o.format = LabelFormat::Engineering;
        o.precision = 1;
        QCOMPARE(ticks::formatValue(0.0012, o), QStringLiteral("1.2 m"));
        o.format = LabelFormat::Computer;
        o.precision = 0;
        QCOMPARE(ticks::formatValue(4096, o), QStringLiteral("4K"));
        o.format = LabelFormat::YYMMDD;
        o.referenceDate = 0;
        QCOMPARE(ticks::formatValue(ticks::calendarToJulian(2024, 2, 29), o), QStringLiteral("2024-02-29"));
        o.format = LabelFormat::DegreesLat;
        o.precision = 0;
        QCOMPARE(ticks::formatValue(-30, o), QStringLiteral("30") + QChar(0x00B0) + QLatin1Char('S'));
        int y, m, d, h, mi;
        double s;
        ticks::julianToCalendar(ticks::calendarToJulian(1999, 12, 31, 23, 59, 30), &y, &m, &d, &h, &mi, &s);
        QCOMPARE(y, 1999);
        QCOMPARE(d, 31);
        QCOMPARE(mi, 59);
    }
};

QTEST_GUILESS_MAIN(TestMath)
#include "test_math.moc"
