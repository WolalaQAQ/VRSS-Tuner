#pragma once
#include <algorithm>
#include <cmath>

namespace region {
constexpr double Pi = 3.14159265358979323846;
struct Estimate {
    bool valid{}, supported{}, active{}, adaptive{};
    double radiusX{}, radiusY{}, nominalFraction{}, visibleFraction{}, shadingRatio{1};
};
inline Estimate Calculate(double area, double aspect, int samples, int mode, int layout) {
    Estimate e;
    if (!std::isfinite(area) || !std::isfinite(aspect) || area < 1 || area > 100 ||
        aspect < .5 || aspect > 2 || (samples != 2 && samples != 4 && samples != 8) ||
        mode < 0 || mode > 3 || layout < 0 || layout > 1) return e;
    e.valid = true; e.supported = layout == 0 && mode != 3;
    e.active = mode != 0; e.adaptive = mode == 2;
    // Offline driver model, multiplier=1, no guard margins or 16-pixel rounding.
    // Coordinates are normalized per-eye/subview, with gaze fixed at its center.
    e.nominalFraction = area / 100;
    e.radiusX = std::sqrt(e.nominalFraction * aspect / Pi);
    e.radiusY = std::sqrt(e.nominalFraction / (aspect * Pi));
    if (e.radiusX <= .5 && e.radiusY <= .5) e.visibleFraction = e.nominalFraction;
    else {
        constexpr int n = 2048;
        auto height = [&](double x) {
            if (std::abs(x) >= e.radiusX) return 0.0;
            return 2 * std::min(.5, e.radiusY * std::sqrt(std::max(0.0, 1 - x*x/(e.radiusX*e.radiusX))));
        };
        double sum = height(-.5) + height(.5);
        for (int i = 1; i < n; ++i) sum += (i % 2 ? 4 : 2) * height(-.5 + double(i)/n);
        e.visibleFraction = std::clamp(sum / (3*n), 0.0, std::min(1.0, e.nominalFraction));
    }
    e.shadingRatio = e.active ? 1 + e.visibleFraction * (samples - 1) : 1;
    return e;
}
}
