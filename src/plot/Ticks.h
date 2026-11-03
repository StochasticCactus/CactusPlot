// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - tick placement and tick label formatting.
#pragma once

#include <QString>
#include <QVector>

#include "core/Model.h"
#include "plot/Scale.h"

namespace cactus {

struct Tick {
    double value = 0.0;
    bool major = true;
    QString label;  ///< formatted label (Grace rich-text markup); empty for minor ticks
};

namespace ticks {

/// "Nice" number close to x (1, 2, 2.5, 5 x 10^n). round=true rounds, otherwise ceils.
double niceNumber(double x, bool round);

/// Choose a nice major spacing for a range (for log scales: a decade factor).
double autoSpacing(double wmin, double wmax, ScaleType scale, int targetCount);
/// Suggested number of minor ticks between majors for a spacing.
int autoMinorCount(double spacing, ScaleType scale, bool modern);

/// Expand [wmin, wmax] to nice values (Grace autoscale rounding).
void niceRange(double* wmin, double* wmax, ScaleType scale, int targetCount);

/// Compute the ticks of an axis between wmin and wmax.
QVector<Tick> compute(const Axis& axis, const Scale& scale, const Project* project = nullptr);

struct FormatOptions {
    LabelFormat format = LabelFormat::General;
    int precision = 5;
    QString prepend, append;
    TickSign sign = TickSign::Normal;
    double referenceDate = 0.0;
    bool pretty = true;  ///< Unicode minus, x10^n superscripts instead of e+NN
    bool twoDigitYears = false;  ///< Grace style "yy" years
};

/// Format a number as a tick/annotation label (returns Grace rich-text markup).
QString formatValue(double v, const FormatOptions& opt);

/// Calendar helpers (proleptic Gregorian, Julian day numbers).
void julianToCalendar(double jd, int* year, int* month, int* day, int* hour, int* minute, double* second);
double calendarToJulian(int year, int month, int day, int hour = 0, int minute = 0, double second = 0.0);

}  // namespace ticks
}  // namespace cactus
