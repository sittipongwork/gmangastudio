//
//  Bezier.hpp
//  IllusStudioFramework — cubic fit for stroke storage / SVG (T2-7-1 / TX-7-4)
//

#pragma once

#include "../strokes/StrokeSample.hpp"

#include <cstdint>
#include <vector>

namespace illus {

struct BezierCubic {
    float x0 = 0, y0 = 0;
    float x1 = 0, y1 = 0;
    float x2 = 0, y2 = 0;
    float x3 = 0, y3 = 0;
};

/// Fit one cubic to `n` samples (n >= 2). Uses Eigen least-squares (TX-7).
/// Returns false if fit is unusable; caller keeps dense samples.
bool fitCubicLeastSquares(const StrokeSample* samples, int32_t n, BezierCubic& out);

/// Fit whole polyline into cubics (splits when needed). Empty out → keep samples only.
/// Call from export / demand — not from `endStroke` (TX-7).
void fitStrokeCubics(const std::vector<StrokeSample>& samples, std::vector<BezierCubic>& out, float maxErrPx = 2.f);

/// If `cubics` already filled, no-op. Otherwise fit once. Returns true if cubics non-empty.
bool ensureStrokeCubics(const std::vector<StrokeSample>& samples, std::vector<BezierCubic>& cubics, float maxErrPx = 2.f);

bool bezierSelfCheck();

} // namespace illus
