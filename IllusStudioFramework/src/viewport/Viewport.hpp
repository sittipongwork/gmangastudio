//
//  Viewport.hpp
//  IllusStudioFramework — present-time pan/zoom (no re-raster)
//

#pragma once

#include <algorithm>
#include <cmath>

namespace illus {

/// Present transform. `scale` is relative to fit-to-view (1 = fit).
/// `offsetX` / `offsetY` are pan in canvas pixels (resolution-independent).
struct Viewport {
    float scale = 1.f;
    float offsetX = 0.f;
    float offsetY = 0.f;

    static constexpr float kMinScale = 0.1f;
    static constexpr float kMaxScale = 16.f;

    void clampScale() {
        scale = std::clamp(scale, kMinScale, kMaxScale);
        if (!std::isfinite(scale)) scale = 1.f;
        if (!std::isfinite(offsetX)) offsetX = 0.f;
        if (!std::isfinite(offsetY)) offsetY = 0.f;
    }

    float fitScale(float viewW, float viewH, float canvasW, float canvasH) const {
        if (viewW <= 0.f || viewH <= 0.f || canvasW <= 0.f || canvasH <= 0.f) return 1.f;
        return std::min(viewW / canvasW, viewH / canvasH);
    }

    float effectiveScale(float viewW, float viewH, float canvasW, float canvasH) const {
        return fitScale(viewW, viewH, canvasW, canvasH) * scale;
    }

    void viewOrigin(float viewW, float viewH, float canvasW, float canvasH, float& outX, float& outY) const {
        const float s = effectiveScale(viewW, viewH, canvasW, canvasH);
        outX = (viewW - canvasW * s) * 0.5f - offsetX * s;
        outY = (viewH - canvasH * s) * 0.5f - offsetY * s;
    }

    void viewToCanvas(float viewX, float viewY, float viewW, float viewH, float canvasW, float canvasH,
                      float& outX, float& outY) const {
        const float s = effectiveScale(viewW, viewH, canvasW, canvasH);
        if (s <= 0.f) {
            outX = 0.f;
            outY = 0.f;
            return;
        }
        float ox = 0.f, oy = 0.f;
        viewOrigin(viewW, viewH, canvasW, canvasH, ox, oy);
        outX = (viewX - ox) / s;
        outY = (viewY - oy) / s;
    }

    void canvasToView(float canvasX, float canvasY, float viewW, float viewH, float canvasW, float canvasH,
                      float& outX, float& outY) const {
        const float s = effectiveScale(viewW, viewH, canvasW, canvasH);
        float ox = 0.f, oy = 0.f;
        viewOrigin(viewW, viewH, canvasW, canvasH, ox, oy);
        outX = ox + canvasX * s;
        outY = oy + canvasY * s;
    }
};

} // namespace illus
