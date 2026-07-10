//
//  Bezier.cpp
//  IllusStudioFramework — Eigen least-squares cubic fit (TX-7-4)
//

#include "Bezier.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <vector>

namespace illus {
namespace {

void evalCubic(const BezierCubic& c, float t, float& x, float& y) {
    const float u = 1.f - t;
    const float uu = u * u;
    const float tt = t * t;
    const float uuu = uu * u;
    const float ttt = tt * t;
    x = uuu * c.x0 + 3.f * uu * t * c.x1 + 3.f * u * tt * c.x2 + ttt * c.x3;
    y = uuu * c.y0 + 3.f * uu * t * c.y1 + 3.f * u * tt * c.y2 + ttt * c.y3;
}

float maxFitError(const StrokeSample* samples, int32_t n, const BezierCubic& c) {
    if (n < 2) return 0.f;
    float maxE = 0.f;
    for (int32_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n - 1);
        float x = 0.f, y = 0.f;
        evalCubic(c, t, x, y);
        const float dx = x - samples[i].x;
        const float dy = y - samples[i].y;
        maxE = std::max(maxE, std::sqrt(dx * dx + dy * dy));
    }
    return maxE;
}

} // namespace

bool fitCubicLeastSquares(const StrokeSample* samples, int32_t n, BezierCubic& out) {
    if (!samples || n < 2) return false;
    if (n == 2) {
        out = {
            samples[0].x,
            samples[0].y,
            samples[0].x,
            samples[0].y,
            samples[1].x,
            samples[1].y,
            samples[1].x,
            samples[1].y
        };
        return true;
    }

    std::vector<float> t(static_cast<size_t>(n), 0.f);
    float total = 0.f;
    for (int32_t i = 1; i < n; ++i) {
        const float dx = samples[i].x - samples[i - 1].x;
        const float dy = samples[i].y - samples[i - 1].y;
        total += std::sqrt(dx * dx + dy * dy);
        t[static_cast<size_t>(i)] = total;
    }
    if (total < 1e-4f) {
        out = {
            samples[0].x,
            samples[0].y,
            samples[0].x,
            samples[0].y,
            samples[0].x,
            samples[0].y,
            samples[0].x,
            samples[0].y
        };
        return true;
    }
    for (int32_t i = 0; i < n; ++i) t[static_cast<size_t>(i)] /= total;

    const float x0 = samples[0].x, y0 = samples[0].y;
    const float x3 = samples[n - 1].x, y3 = samples[n - 1].y;

    Eigen::MatrixXf Ax(n, 2), Ay(n, 2);
    Eigen::VectorXf bx(n), by(n);
    for (int32_t i = 0; i < n; ++i) {
        const float ti = t[static_cast<size_t>(i)];
        const float u = 1.f - ti;
        const float b0 = u * u * u;
        const float b1 = 3.f * u * u * ti;
        const float b2 = 3.f * u * ti * ti;
        const float b3 = ti * ti * ti;
        Ax(i, 0) = Ay(i, 0) = b1;
        Ax(i, 1) = Ay(i, 1) = b2;
        bx(i) = samples[i].x - b0 * x0 - b3 * x3;
        by(i) = samples[i].y - b0 * y0 - b3 * y3;
    }

    const Eigen::Vector2f sx = Ax.colPivHouseholderQr().solve(bx);
    const Eigen::Vector2f sy = Ay.colPivHouseholderQr().solve(by);
    out = {x0, y0, sx(0), sy(0), sx(1), sy(1), x3, y3};
    return std::isfinite(out.x1) && std::isfinite(out.y1) && std::isfinite(out.x2) && std::isfinite(out.y2);
}

void fitStrokeCubics(const std::vector<StrokeSample>& samples, std::vector<BezierCubic>& out, float maxErrPx) {
    out.clear();
    const int32_t n = static_cast<int32_t>(samples.size());
    if (n < 2) return;

    // ponytail: recursive split on max error; ceiling ~64 cubics then stop.
    const int32_t kMaxCubics = 64;
    std::vector<std::pair<int32_t, int32_t>> stack;
    stack.push_back({0, n - 1});
    while (!stack.empty() && static_cast<int32_t>(out.size()) < kMaxCubics) {
        const auto [a, b] = stack.back();
        stack.pop_back();
        const int32_t count = b - a + 1;
        BezierCubic c{};
        if (!fitCubicLeastSquares(samples.data() + a, count, c)) continue;
        if (count <= 4 || maxFitError(samples.data() + a, count, c) <= maxErrPx) {
            out.push_back(c);
            continue;
        }
        const int32_t mid = a + count / 2;
        stack.push_back({mid, b});
        stack.push_back({a, mid});
    }
}

bool ensureStrokeCubics(const std::vector<StrokeSample>& samples, std::vector<BezierCubic>& cubics, float maxErrPx) {
    if (!cubics.empty()) return true;
    fitStrokeCubics(samples, cubics, maxErrPx);
    return !cubics.empty();
}

bool bezierSelfCheck() {
    // Straight line (0,0)→(10,0) should fit with tiny error and control points near the line.
    std::vector<StrokeSample> line;
    for (int i = 0; i <= 10; ++i) {
        line.push_back({static_cast<float>(i), 0.f, 1.f, 0, 0, 0});
    }
    BezierCubic c{};
    if (!fitCubicLeastSquares(line.data(), static_cast<int32_t>(line.size()), c)) return false;
    if (std::abs(c.y0) > 1e-2f || std::abs(c.y3) > 1e-2f) return false;
    if (std::abs(c.x0) > 1e-2f || std::abs(c.x3 - 10.f) > 1e-2f) return false;
    if (maxFitError(line.data(), static_cast<int32_t>(line.size()), c) > 0.5f) return false;

    std::vector<BezierCubic> many;
    fitStrokeCubics(line, many, 1.f);
    if (many.empty()) return false;
    return true;
}

} // namespace illus
