// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - axis scale transformations.
#pragma once

#include "core/Types.h"

namespace cactus {

struct Scale {
    ScaleType type = ScaleType::Linear;
    double wmin = 0.0, wmax = 1.0;
    bool invert = false;

    Scale() = default;
    Scale(ScaleType t, double a, double b, bool inv = false) : type(t), wmin(a), wmax(b), invert(inv) {}

    static double forward(ScaleType t, double w);
    static double inverse(ScaleType t, double v);
    static bool validValue(ScaleType t, double w);

    double fwd(double w) const { return forward(type, w); }
    double inv(double v) const { return inverse(type, v); }
    bool valid(double w) const { return validValue(type, w); }
    /// Is the [wmin, wmax] range usable for this scale?
    bool validRange() const;
    /// World value -> fraction along the axis (0 at the start, 1 at the end).
    double toFrac(double w) const;
    double fromFrac(double f) const;
};

}  // namespace cactus
