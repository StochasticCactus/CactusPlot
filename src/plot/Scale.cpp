// SPDX-License-Identifier: GPL-2.0-or-later
#include "plot/Scale.h"

#include <cmath>

namespace cactus {

double Scale::forward(ScaleType t, double w)
{
    switch (t) {
    case ScaleType::Linear: return w;
    case ScaleType::Log: return w > 0.0 ? std::log10(w) : -HUGE_VAL;
    case ScaleType::Reciprocal: return w != 0.0 ? 1.0 / w : HUGE_VAL;
    case ScaleType::Logit:
        if (w <= 0.0) return -HUGE_VAL;
        if (w >= 1.0) return HUGE_VAL;
        return std::log(w / (1.0 - w));
    }
    return w;
}

double Scale::inverse(ScaleType t, double v)
{
    switch (t) {
    case ScaleType::Linear: return v;
    case ScaleType::Log: return std::pow(10.0, v);
    case ScaleType::Reciprocal: return v != 0.0 ? 1.0 / v : HUGE_VAL;
    case ScaleType::Logit: return 1.0 / (1.0 + std::exp(-v));
    }
    return v;
}

bool Scale::validValue(ScaleType t, double w)
{
    if (!std::isfinite(w)) return false;
    switch (t) {
    case ScaleType::Linear: return true;
    case ScaleType::Log: return w > 0.0;
    case ScaleType::Reciprocal: return w != 0.0;
    case ScaleType::Logit: return w > 0.0 && w < 1.0;
    }
    return true;
}

bool Scale::validRange() const
{
    if (!valid(wmin) || !valid(wmax)) return false;
    if (wmin >= wmax) return false;
    if (type == ScaleType::Reciprocal && wmin < 0.0 && wmax > 0.0) return false;
    const double a = fwd(wmin), b = fwd(wmax);
    return std::isfinite(a) && std::isfinite(b) && a != b;
}

double Scale::toFrac(double w) const
{
    const double a = fwd(wmin), b = fwd(wmax);
    double f = (fwd(w) - a) / (b - a);
    return invert ? 1.0 - f : f;
}

double Scale::fromFrac(double f) const
{
    if (invert) f = 1.0 - f;
    const double a = fwd(wmin), b = fwd(wmax);
    return inv(a + f * (b - a));
}

}  // namespace cactus
