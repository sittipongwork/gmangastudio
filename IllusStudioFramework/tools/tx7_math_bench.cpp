//
//  tx7_math_bench.cpp — scalar/POD vs GLM / Eigen microbench (TX-7)
//
//  Build & run (Release, not Debug):
//    clang++ -O2 -std=c++20 -DNDEBUG \
//      -I IllusStudioFramework/src \
//      -isystem IllusStudioFramework/third_party/eigen \
//      -isystem IllusStudioFramework/third_party/glm \
//      IllusStudioFramework/src/math/Bezier.cpp \
//      IllusStudioFramework/src/math/PresentTransform.cpp \
//      IllusStudioFramework/tools/tx7_math_bench.cpp \
//      -o /tmp/tx7_math_bench && /tmp/tx7_math_bench
//

#include "math/Bezier.hpp"
#include "math/PresentTransform.hpp"
#include "viewport/Viewport.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

static volatile float gSink = 0.f;

static double nsPer(Clock::time_point a, Clock::time_point b, int64_t iters) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    return static_cast<double>(ns) / static_cast<double>(iters);
}

/// Pre-TX-7 / current hot path: axis-aligned NDC from scale+origin (no GLM).
static void presentNdcRectScalar(
    const illus::Viewport& vp,
    float canvasW,
    float canvasH,
    float viewW,
    float viewH,
    float out[4]
) {
    const float s = vp.effectiveScale(viewW, viewH, canvasW, canvasH);
    float ox = 0.f, oy = 0.f;
    vp.viewOrigin(viewW, viewH, canvasW, canvasH, ox, oy);
    const float drawnW = canvasW * s;
    const float drawnH = canvasH * s;
    out[0] = -1.f + 2.f * ox / viewW;
    out[1] = 1.f - 2.f * (oy + drawnH) / viewH;
    out[2] = -1.f + 2.f * (ox + drawnW) / viewW;
    out[3] = 1.f - 2.f * oy / viewH;
}

/// Early TX-7 hot path: GLM mat4 + corner multiply every present.
static void presentNdcRectGlm(
    const illus::Viewport& vp,
    float canvasW,
    float canvasH,
    float viewW,
    float viewH,
    float out[4]
) {
    const float s = vp.effectiveScale(viewW, viewH, canvasW, canvasH);
    float ox = 0.f, oy = 0.f;
    vp.viewOrigin(viewW, viewH, canvasW, canvasH, ox, oy);
    glm::mat4 m(1.f);
    m = glm::translate(m, glm::vec3(ox, oy, 0.f));
    m = glm::scale(m, glm::vec3(s, s, 1.f));
    const glm::vec4 tl = m * glm::vec4(0.f, 0.f, 0.f, 1.f);
    const glm::vec4 br = m * glm::vec4(canvasW, canvasH, 0.f, 1.f);
    out[0] = -1.f + 2.f * tl.x / viewW;
    out[1] = 1.f - 2.f * br.y / viewH;
    out[2] = -1.f + 2.f * br.x / viewW;
    out[3] = 1.f - 2.f * tl.y / viewH;
}

static std::vector<illus::StrokeSample> makeStroke(int n) {
    std::vector<illus::StrokeSample> s;
    s.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(std::max(1, n - 1));
        // Mild curve so fit may split (not a perfect line).
        s.push_back({
            t * 400.f,
            40.f * std::sin(t * 6.2831853f * 1.5f) + t * 20.f,
            1.f,
            0,
            0,
            t
        });
    }
    return s;
}

int main() {
    illus::Viewport vp;
    vp.scale = 1.25f;
    vp.offsetX = 12.f;
    vp.offsetY = -8.f;
    const float cw = 1920.f, ch = 1080.f, vw = 1512.f, vh = 982.f;

    // Warmup + correctness: scalar vs GLM NDC should match within epsilon.
    float a[4], b[4], c[4];
    presentNdcRectScalar(vp, cw, ch, vw, vh, a);
    presentNdcRectGlm(vp, cw, ch, vw, vh, b);
    illus::presentNdcRect(vp, cw, ch, vw, vh, c);
    for (int i = 0; i < 4; ++i) {
        if (std::abs(a[i] - b[i]) > 1e-4f || std::abs(a[i] - c[i]) > 1e-4f) {
            std::printf("FAIL: NDC mismatch scalar vs GLM vs presentNdcRect\n");
            return 1;
        }
    }

    constexpr int64_t kPresentIters = 2'000'000; // ~4.6h of 120Hz wall if sequential
    float out[4];

    auto t0 = Clock::now();
    for (int64_t i = 0; i < kPresentIters; ++i) {
        presentNdcRectScalar(vp, cw, ch, vw, vh, out);
        gSink += out[0] + out[3];
    }
    auto t1 = Clock::now();
    for (int64_t i = 0; i < kPresentIters; ++i) {
        presentNdcRectGlm(vp, cw, ch, vw, vh, out);
        gSink += out[0] + out[3];
    }
    auto t2 = Clock::now();
    for (int64_t i = 0; i < kPresentIters; ++i) {
        float m16[16];
        illus::presentModelMatrix(vp, cw, ch, vw, vh, m16);
        gSink += m16[0] + m16[5];
    }
    auto t3 = Clock::now();

    const double scalarNs = nsPer(t0, t1, kPresentIters);
    const double glmNs = nsPer(t1, t2, kPresentIters);
    const double modelNs = nsPer(t2, t3, kPresentIters);

    std::printf("=== Present NDC (axis-aligned) — %lld iters ===\n", (long long)kPresentIters);
    std::printf("  scalar (no GLM)     %7.2f ns/op   baseline\n", scalarNs);
    std::printf("  GLM mat4 path       %7.2f ns/op   %.2fx vs scalar\n", glmNs, glmNs / scalarNs);
    std::printf("  presentModelMatrix  %7.2f ns/op   %.2fx vs scalar\n", modelNs, modelNs / scalarNs);
    std::printf("  @120Hz budget 8333 ns/frame — scalar uses %.3f%%, GLM %.3f%%\n",
                100.0 * scalarNs / 8333.0, 100.0 * glmNs / 8333.0);

    std::printf("\n=== Bézier fit on endStroke — Eigen vs skip ===\n");
    const int sizes[] = {50, 200, 1000};
    for (int n : sizes) {
        auto samples = makeStroke(n);
        std::vector<illus::BezierCubic> cubics;
        constexpr int kFitIters = 400;

        // "not use": store samples only (what we do now on endStroke).
        auto u0 = Clock::now();
        for (int i = 0; i < kFitIters; ++i) {
            cubics.clear();
            gSink += static_cast<float>(samples.size());
        }
        auto u1 = Clock::now();

        auto u2 = Clock::now();
        for (int i = 0; i < kFitIters; ++i) {
            illus::fitStrokeCubics(samples, cubics, 2.f);
            gSink += cubics.empty() ? 0.f : cubics[0].x0;
        }
        auto u3 = Clock::now();

        const double skipNs = nsPer(u0, u1, kFitIters);
        const double eigenNs = nsPer(u2, u3, kFitIters);
        illus::fitStrokeCubics(samples, cubics, 2.f);
        std::printf("  samples=%4d  skip-fit %8.1f ns   Eigen-fit %10.1f ns  (%.0fx)  cubics=%zu\n",
                    n, skipNs, eigenNs, eigenNs / std::max(1.0, skipNs), cubics.size());
    }

    std::printf("\nVerdict:\n");
    std::printf("  - Present: keep scalar; GLM only when rotate/MVP needs mat4.\n");
    std::printf("  - Fit: call Eigen lazily (export/SVG), never under endStroke mutex.\n");
    std::printf("sink=%g\n", (double)gSink);
    return 0;
}
