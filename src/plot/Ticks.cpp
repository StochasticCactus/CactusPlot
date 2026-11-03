// SPDX-License-Identifier: GPL-2.0-or-later
#include "plot/Ticks.h"

#include <QChar>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include "math/Expr.h"

namespace cactus::ticks {

namespace {

const QString kMinus = QString(QChar(0x2212));
const QString kTimes = QString(QChar(0x00D7));

QString cfmt(const char* fmt, int prec, double v)
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), fmt, prec, v);
    return QString::fromLatin1(buf);
}

/// Normalise a printf exponent "1.5e+06" into mantissa/exponent strings.
bool splitExp(const QString& s, QString* mant, int* exp)
{
    const int e = s.indexOf(QLatin1Char('e'), 0, Qt::CaseInsensitive);
    if (e < 0) return false;
    *mant = s.left(e);
    *exp = s.mid(e + 1).toInt();
    return true;
}

QString prettyMinus(QString s, bool pretty)
{
    if (pretty) s.replace(QLatin1Char('-'), kMinus);
    return s;
}

QString powerOfTen(int exp, bool pretty)
{
    return QStringLiteral("10\\S%1\\N").arg(prettyMinus(QString::number(exp), pretty));
}

QString mantissaTimesPower(const QString& mant, int exp, bool pretty)
{
    if (exp == 0) return prettyMinus(mant, pretty);
    if (mant == QLatin1String("1")) return powerOfTen(exp, pretty);
    if (mant == QLatin1String("-1")) return prettyMinus(QStringLiteral("-"), pretty) + powerOfTen(exp, pretty);
    return prettyMinus(mant, pretty) + (pretty ? kTimes : QStringLiteral("x")) + powerOfTen(exp, pretty);
}

const char* kMonthShort[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* kMonthLong[] = {"January", "February", "March",     "April",   "May",      "June",
                            "July",    "August",   "September", "October", "November", "December"};
const char* kDayShort[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char* kDayLong[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

QString two(int v) { return QStringLiteral("%1").arg(v, 2, 10, QLatin1Char('0')); }

QString degrees(double v, int prec, int parts, bool lon, bool minutesOnly)
{
    QChar hemi;
    if (lon) {
        v = std::fmod(v, 360.0);
        if (v > 180.0) v -= 360.0;
        if (v < -180.0) v += 360.0;
        hemi = v >= 0 ? QLatin1Char('E') : QLatin1Char('W');
    } else {
        hemi = v >= 0 ? QLatin1Char('N') : QLatin1Char('S');
    }
    v = std::fabs(v);
    const QChar deg(0x00B0);
    if (parts == 1) return QString::number(v, 'f', prec) + deg + hemi;
    int d = int(std::floor(v));
    double rem = (v - d) * 60.0;
    if (parts == 2) {
        if (rem >= 59.9999999) { d += 1; rem = 0; }
        return QString::number(d) + deg + QString::number(rem, 'f', prec) + QLatin1Char('\'') + hemi;
    }
    int m = int(std::floor(rem));
    double s = (rem - m) * 60.0;
    if (s >= 59.9999999) { s = 0; m += 1; }
    if (m >= 60) { m = 0; d += 1; }
    if (minutesOnly) return two(m) + QLatin1Char('\'') + QString::number(s, 'f', prec) + QLatin1Char('"') + hemi;
    return QString::number(d) + deg + two(m) + QLatin1Char('\'') + QString::number(s, 'f', prec) + QLatin1Char('"') + hemi;
}

}  // namespace

double niceNumber(double x, bool round)
{
    if (!(x > 0.0) || !std::isfinite(x)) return 1.0;
    const double expv = std::floor(std::log10(x));
    const double f = x / std::pow(10.0, expv);
    double nf;
    if (round) {
        if (f < 1.5) nf = 1.0;
        else if (f < 2.25) nf = 2.0;
        else if (f < 3.5) nf = 2.5;
        else if (f < 7.5) nf = 5.0;
        else nf = 10.0;
    } else {
        if (f <= 1.0) nf = 1.0;
        else if (f <= 2.0) nf = 2.0;
        else if (f <= 2.5) nf = 2.5;
        else if (f <= 5.0) nf = 5.0;
        else nf = 10.0;
    }
    return nf * std::pow(10.0, expv);
}

double autoSpacing(double wmin, double wmax, ScaleType scale, int targetCount)
{
    targetCount = std::max(2, targetCount);
    if (scale == ScaleType::Log && wmin > 0 && wmax > wmin) {
        const double decades = std::log10(wmax / wmin);
        const double per = std::max(1.0, std::ceil(decades / (targetCount + 1)));
        return std::pow(10.0, per);
    }
    if (scale == ScaleType::Reciprocal && wmin > 0 && wmax > wmin) {
        // place ticks on a nice linear grid in world space
        return niceNumber((wmax - wmin) / (targetCount - 1), true);
    }
    const double range = wmax - wmin;
    if (!(range > 0) || !std::isfinite(range)) return 1.0;
    return niceNumber(range / (targetCount - 1), true);
}

int autoMinorCount(double spacing, ScaleType scale, bool modern)
{
    if (scale == ScaleType::Log) return std::fabs(spacing - 10.0) < 1e-9 ? 8 : 0;
    if (!modern) return 1;
    const double m = spacing / std::pow(10.0, std::floor(std::log10(spacing) + 1e-12));
    if (std::fabs(m - 2.0) < 1e-6) return 3;
    if (std::fabs(m - 2.5) < 1e-6) return 4;
    return 4;
}

void niceRange(double* wmin, double* wmax, ScaleType scale, int targetCount)
{
    double a = *wmin, b = *wmax;
    if (!std::isfinite(a) || !std::isfinite(b)) return;
    if (scale == ScaleType::Log) {
        if (b <= 0) { a = 1; b = 10; }
        if (a <= 0) a = b / 1000.0;
        if (a == b) { a /= 10.0; b *= 10.0; }
        *wmin = std::pow(10.0, std::floor(std::log10(a) + 1e-12));
        *wmax = std::pow(10.0, std::ceil(std::log10(b) - 1e-12));
        if (*wmin == *wmax) *wmax *= 10.0;
        return;
    }
    if (scale == ScaleType::Logit) {
        *wmin = std::max(a, 1e-6);
        *wmax = std::min(b, 1.0 - 1e-6);
        if (*wmin >= *wmax) { *wmin = 0.01; *wmax = 0.99; }
        return;
    }
    if (a == b) {
        const double d = a == 0.0 ? 1.0 : std::fabs(a) * 0.1;
        a -= d;
        b += d;
    }
    const double sp = autoSpacing(a, b, ScaleType::Linear, targetCount);
    double na = std::floor(a / sp + 1e-9) * sp;
    double nb = std::ceil(b / sp - 1e-9) * sp;
    if (scale == ScaleType::Reciprocal && na <= 0.0 && a > 0.0) na = a;
    *wmin = na;
    *wmax = nb;
}

QString formatValue(double v, const FormatOptions& opt)
{
    if (opt.sign == TickSign::Absolute) v = std::fabs(v);
    else if (opt.sign == TickSign::Negate) v = -v;
    const int prec = std::clamp(opt.precision, 0, 17);
    const bool pretty = opt.pretty;
    QString s;
    switch (opt.format) {
    case LabelFormat::Decimal:
        s = prettyMinus(cfmt("%.*f", prec, v), pretty);
        if (s == kMinus + QString::number(0.0, 'f', prec) || s == QStringLiteral("-") + QString::number(0.0, 'f', prec))
            s = QString::number(0.0, 'f', prec);
        break;
    case LabelFormat::Exponential: {
        const QString raw = cfmt("%.*e", prec, v);
        QString mant;
        int e = 0;
        if (pretty && splitExp(raw, &mant, &e)) s = mantissaTimesPower(mant, e, true);
        else s = raw;
        break;
    }
    case LabelFormat::General: {
        if (v == 0.0) { s = QStringLiteral("0"); break; }
        const QString raw = cfmt("%.*g", std::max(1, prec), v);
        QString mant;
        int e = 0;
        if (pretty && splitExp(raw, &mant, &e)) s = mantissaTimesPower(mant, e, true);
        else s = prettyMinus(raw, pretty);
        break;
    }
    case LabelFormat::Power: {
        if (v > 0) {
            const double e = std::log10(v);
            const QString es = cfmt("%.*f", prec, e);
            s = QStringLiteral("10\\S%1\\N").arg(prettyMinus(es, pretty));
        } else {
            s = prettyMinus(cfmt("%.*g", std::max(1, prec), v), pretty);
        }
        break;
    }
    case LabelFormat::Scientific: {
        if (v == 0.0) { s = QString::number(0.0, 'f', prec); break; }
        int e = int(std::floor(std::log10(std::fabs(v))));
        double m = v / std::pow(10.0, e);
        QString ms = cfmt("%.*f", prec, m);
        if (std::fabs(ms.toDouble()) >= 10.0) { e += 1; ms = cfmt("%.*f", prec, v / std::pow(10.0, e)); }
        s = prettyMinus(ms, pretty) + (pretty ? kTimes : QStringLiteral("x")) + powerOfTen(e, pretty);
        break;
    }
    case LabelFormat::Engineering:
    case LabelFormat::Computer: {
        static const char* prefixes[] = {"y", "z", "a", "f", "p", "n", "\xC2\xB5", "m", "", "k", "M", "G", "T", "P", "E", "Z", "Y"};
        static const char* cprefixes[] = {"y", "z", "a", "f", "p", "n", "u", "m", "", "K", "M", "G", "T", "P", "E", "Z", "Y"};
        if (v == 0.0) { s = QString::number(0.0, 'f', prec); break; }
        const bool binary = opt.format == LabelFormat::Computer;
        int e3 = binary ? int(std::floor(std::log2(std::fabs(v)) / 10.0)) : int(std::floor(std::log10(std::fabs(v)) / 3.0));
        e3 = std::clamp(e3, -8, 8);
        const double m = binary ? v / std::pow(1024.0, e3) : v / std::pow(10.0, 3 * e3);
        const char* pre = opt.format == LabelFormat::Engineering ? prefixes[e3 + 8] : cprefixes[e3 + 8];
        QString ms = cfmt("%.*f", prec, m);
        if (ms.contains(QLatin1Char('.'))) {
            while (ms.endsWith(QLatin1Char('0'))) ms.chop(1);
            if (ms.endsWith(QLatin1Char('.'))) ms.chop(1);
        }
        s = prettyMinus(ms, pretty) + (opt.format == LabelFormat::Engineering && *pre ? QStringLiteral(" ") : QString()) +
            QString::fromUtf8(pre);
        break;
    }
    case LabelFormat::DegreesLon: s = degrees(v, prec, 1, true, false); break;
    case LabelFormat::DegreesMMLon: s = degrees(v, prec, 2, true, false); break;
    case LabelFormat::DegreesMMSSLon: s = degrees(v, prec, 3, true, false); break;
    case LabelFormat::MMSSLon: s = degrees(v, prec, 3, true, true); break;
    case LabelFormat::DegreesLat: s = degrees(v, prec, 1, false, false); break;
    case LabelFormat::DegreesMMLat: s = degrees(v, prec, 2, false, false); break;
    case LabelFormat::DegreesMMSSLat: s = degrees(v, prec, 3, false, false); break;
    case LabelFormat::MMSSLat: s = degrees(v, prec, 3, false, true); break;
    default: {
        // calendar formats
        int y, mo, d, h, mi;
        double sec;
        double jd = v + opt.referenceDate;
        const bool hasTime = opt.format == LabelFormat::HMS || opt.format == LabelFormat::MMDDHMS ||
                             opt.format == LabelFormat::MMDDYYHMS || opt.format == LabelFormat::YYMMDDHMS;
        if (!hasTime) jd += 0.5;  // date-only formats round to the nearest day (Grace)
        julianToCalendar(jd, &y, &mo, &d, &h, &mi, &sec);
        int isec = int(std::floor(sec + 0.5));
        if (isec >= 60) isec = 59;
        const QString ys = opt.twoDigitYears ? two(((y % 100) + 100) % 100) : QString::number(y);
        const QString hms = two(h) + QLatin1Char(':') + two(mi) + QLatin1Char(':') + two(isec);
        switch (opt.format) {
        case LabelFormat::DDMMYY: s = two(d) + QLatin1Char('-') + two(mo) + QLatin1Char('-') + ys; break;
        case LabelFormat::MMDDYY: s = two(mo) + QLatin1Char('-') + two(d) + QLatin1Char('-') + ys; break;
        case LabelFormat::YYMMDD: s = ys + QLatin1Char('-') + two(mo) + QLatin1Char('-') + two(d); break;
        case LabelFormat::MMYY: s = two(mo) + QLatin1Char('-') + ys; break;
        case LabelFormat::MMDD: s = two(mo) + QLatin1Char('-') + two(d); break;
        case LabelFormat::MonthDay: s = QString::fromLatin1(kMonthShort[mo - 1]) + QLatin1Char('-') + two(d); break;
        case LabelFormat::DayMonth: s = two(d) + QLatin1Char('-') + QString::fromLatin1(kMonthShort[mo - 1]); break;
        case LabelFormat::Months: s = QString::fromLatin1(kMonthShort[mo - 1]); break;
        case LabelFormat::MonthsY: s = QString::fromLatin1(kMonthShort[mo - 1]) + QLatin1Char('-') + ys; break;
        case LabelFormat::MonthL: s = QString::fromLatin1(kMonthLong[mo - 1]); break;
        case LabelFormat::DayOfWeek:
        case LabelFormat::DayOfWeekL: {
            const long jdn = long(std::floor(jd + 0.5));
            const int dow = int(((jdn % 7) + 7) % 7);  // 0 = Monday
            s = QString::fromLatin1(opt.format == LabelFormat::DayOfWeek ? kDayShort[dow] : kDayLong[dow]);
            break;
        }
        case LabelFormat::DayOfYear: {
            const double j0 = calendarToJulian(y, 1, 1);
            const int doy = int(std::floor(jd - j0 + 1e-6)) + 1;
            s = opt.twoDigitYears ? QString::number(doy) : QStringLiteral("%1").arg(doy, 3, 10, QLatin1Char('0'));
            break;
        }
        case LabelFormat::HMS: s = hms; break;
        case LabelFormat::MMDDHMS: s = two(mo) + QLatin1Char('-') + two(d) + QLatin1Char(' ') + hms; break;
        case LabelFormat::MMDDYYHMS: s = two(mo) + QLatin1Char('-') + two(d) + QLatin1Char('-') + ys + QLatin1Char(' ') + hms; break;
        case LabelFormat::YYMMDDHMS: s = ys + QLatin1Char('-') + two(mo) + QLatin1Char('-') + two(d) + QLatin1Char(' ') + hms; break;
        default: s = QString::number(v); break;
        }
    }
    }
    return opt.prepend + s + opt.append;
}

void julianToCalendar(double jd, int* year, int* month, int* day, int* hour, int* minute, double* second)
{
    const double j = jd + 0.5;
    double Z = std::floor(j);
    double F = j - Z;
    // round to the millisecond first so that 23:59:59.9999 rolls over to the next day
    if (std::round(F * 86400000.0) >= 86400000.0) {
        Z += 1.0;
        F = 0.0;
    }
    const double alpha = std::floor((Z - 1867216.25) / 36524.25);
    const double A = Z + 1 + alpha - std::floor(alpha / 4.0);
    const double B = A + 1524;
    const double C = std::floor((B - 122.1) / 365.25);
    const double D = std::floor(365.25 * C);
    const double E = std::floor((B - D) / 30.6001);
    *day = int(B - D - std::floor(30.6001 * E));
    *month = int(E < 14 ? E - 1 : E - 13);
    *year = int(*month > 2 ? C - 4716 : C - 4715);
    double secs = F * 86400.0;
    // round to the millisecond to avoid 59.9999 artefacts
    secs = std::round(secs * 1000.0) / 1000.0;
    if (secs >= 86400.0) secs = 86399.999;
    *hour = int(secs / 3600.0);
    secs -= *hour * 3600.0;
    *minute = int(secs / 60.0);
    *second = secs - *minute * 60.0;
}

double calendarToJulian(int y, int m, int d, int hour, int minute, double second)
{
    const int a = (14 - m) / 12;
    const long yy = long(y) + 4800 - a;
    const int mm = m + 12 * a - 3;
    const long jdn = d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
    return double(jdn) + (hour - 12) / 24.0 + minute / 1440.0 + second / 86400.0;
}

QVector<Tick> compute(const Axis& axis, const Scale& scale, const Project* project)
{
    QVector<Tick> out;
    if (!scale.validRange()) return out;
    const double lo = scale.wmin, hi = scale.wmax;

    FormatOptions fo;
    fo.format = axis.format;
    fo.precision = axis.precision;
    fo.prepend = axis.prepend;
    fo.append = axis.append;
    fo.sign = axis.ticklabelSign;
    fo.referenceDate = project ? project->referenceDate : 0.0;
    fo.pretty = project ? project->prettyNumbers : true;
    fo.twoDigitYears = project ? (project->dateWrap || !project->prettyNumbers) : false;

    std::unique_ptr<expr::Expression> formula;
    if (!axis.ticklabelFormula.trimmed().isEmpty()) {
        formula = std::make_unique<expr::Expression>();
        if (!formula->parse(axis.ticklabelFormula)) formula.reset();
    }
    auto labelFor = [&](double v) {
        double shown = v;
        if (formula) {
            expr::Context ctx;
            ctx.setScalar(QStringLiteral("$t"), v);
            ctx.setScalar(QStringLiteral("t"), v);
            shown = formula->evalScalar(ctx);
        }
        return formatValue(shown, fo);
    };

    const double eps = (scale.fwd(hi) - scale.fwd(lo)) * 1e-9;
    auto inRange = [&](double v) {
        if (!scale.valid(v)) return false;
        const double t = scale.fwd(v);
        const double a = std::min(scale.fwd(lo), scale.fwd(hi)), b = std::max(scale.fwd(lo), scale.fwd(hi));
        return t >= a - std::fabs(eps) && t <= b + std::fabs(eps);
    };

    if (!axis.tickAuto) {
        for (const TickMark& tm : axis.specTicks) {
            if (!inRange(tm.value)) continue;
            Tick t{tm.value, tm.major, QString()};
            if (tm.major) t.label = axis.ticklabelAuto ? labelFor(tm.value) : tm.label;
            out.push_back(t);
        }
        return out;
    }

    const bool prettyLog = fo.pretty && scale.type == ScaleType::Log && axis.format == LabelFormat::General &&
                           hi / lo >= 999.0;

    QVector<double> majors;
    double sp = axis.major;
    int nminor = std::max(0, axis.minorTicks);
    if (scale.type == ScaleType::Log) {
        if (!(sp > 1.0) || std::log10(hi / lo) / std::log10(sp) > 500) {
            sp = autoSpacing(lo, hi, scale.type, axis.autoTickCount);
        }
        const double lsp = std::log10(sp);
        double k0 = std::floor(std::log10(lo) / lsp + 1e-9);
        if (!axis.placeRounded) k0 = std::log10(lo) / lsp;
        for (int idx = 0; idx < 2000; ++idx) {
            const double v = std::pow(10.0, (k0 - 1 + idx) * lsp);
            majors.push_back(v);
            if (v > hi * (1 + 1e-9)) break;
        }
    } else {
        if (!(sp > 0.0) || (hi - lo) / sp > 1000.0) sp = autoSpacing(lo, hi, ScaleType::Linear, axis.autoTickCount);
        double start = axis.placeRounded ? std::floor(lo / sp - 1e-9) * sp : lo;
        if (axis.placeRounded) start -= sp;
        for (int k = 0; k < 2100; ++k) {
            double v = start + k * sp;
            if (std::fabs(v) < sp * 1e-9) v = 0.0;
            if (v > hi + sp) break;
            majors.push_back(v);
        }
    }

    int labelIndex = 0;
    for (int i = 0; i < majors.size(); ++i) {
        const double v = majors[i];
        if (inRange(v)) {
            Tick t{v, true, QString()};
            bool show = axis.ticklabelOn;
            if (!axis.ticklabelStartAuto && v < axis.ticklabelStart) show = false;
            if (!axis.ticklabelStopAuto && v > axis.ticklabelStop) show = false;
            if (axis.ticklabelSkip > 0 && (labelIndex % (axis.ticklabelSkip + 1)) != 0) show = false;
            ++labelIndex;
            if (show) {
                if (prettyLog) {
                    const double e = std::log10(v);
                    if (std::fabs(e - std::round(e)) < 1e-9) t.label = fo.prepend + powerOfTen(int(std::round(e)), true) + fo.append;
                    else t.label = labelFor(v);
                } else {
                    t.label = labelFor(v);
                }
            }
            out.push_back(t);
        }
        if (i + 1 < majors.size() && nminor > 0) {
            const double next = majors[i + 1];
            for (int j = 1; j <= nminor; ++j) {
                const double mv = v + (next - v) * j / double(nminor + 1);
                if (inRange(mv)) out.push_back(Tick{mv, false, QString()});
            }
        }
    }
    return out;
}

}  // namespace cactus::ticks
