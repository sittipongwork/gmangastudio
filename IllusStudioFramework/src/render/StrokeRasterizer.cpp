//
//  StrokeRasterizer.cpp
//  IllusStudioFramework
//

#include "StrokeRasterizer.hpp"

#include "../math/Blend.hpp"

#include <algorithm>
#include <cmath>

namespace illus {
namespace {

float coverageAt(float dist, float radius, float hardness) {
    if (radius <= 0.f || dist >= radius) return 0.f;
    const float t = dist / radius; // 0 center → 1 edge
    // hardness 1 = hard edge; 0 = soft falloff from center.
    const float softStart = std::clamp(1.f - hardness, 0.05f, 1.f);
    if (t <= softStart) return 1.f;
    const float u = (t - softStart) / (1.f - softStart);
    // smoothstep
    return 1.f - (u * u * (3.f - 2.f * u));
}

} // namespace

float StrokeRasterizer::effectiveRadius(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    const float scale = 1.f - preset.sizePressure + preset.sizePressure * std::max(0.05f, p);
    return 0.5f * preset.lineWidthPx * scale;
}

bool StrokeRasterizer::stampDab(
    Layer& layer,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty
) {
    layer.ensurePixels(width, height);
    const float radius = effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;

    const float p = std::clamp(pressure, 0.f, 1.f);
    const float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    const float alphaF = std::clamp(preset.opacity * preset.flow * opacScale, 0.f, 1.f);
    if (alphaF <= 0.f) return false;

    const float radius2 = radius * radius;
    const int minX = std::max(0, static_cast<int>(std::floor(x - radius)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - radius)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + radius)));
    if (minX > maxX || minY > maxY) return false;

    const bool erase = preset.mode == BrushMode::Erase;
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > radius2) continue;
            const float cov = coverageAt(std::sqrt(d2), radius, preset.hardness);
            if (cov <= 0.f) continue;
            const uint8_t a = static_cast<uint8_t>(std::clamp(cov * alphaF * 255.f, 0.f, 255.f));
            uint8_t* dst = layer.pixelAt(px, py, width);
            if (erase) {
                math::blendDestOut(dst, a);
            } else {
                math::blendSrcOver(dst, preset.color.r, preset.color.g, preset.color.b, a);
            }
        }
    }
    dirty.unionWith(minX, minY, maxX, maxY);
    dirty.clipTo(width, height);
    return true;
}

void StrokeRasterizer::stampSegment(
    Layer& layer,
    int32_t width,
    int32_t height,
    const StrokeSample& a,
    const StrokeSample& b,
    const BrushPreset& preset,
    float& carryDist,
    math::Rect& dirty,
    StrokeBounds* bounds
) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float midP = 0.5f * (a.pressure + b.pressure);
    const float step = std::max(0.5f, preset.spacing * 2.f * effectiveRadius(midP, preset));

    if (dist < 1e-4f) {
        if (carryDist <= 0.f) {
            stampDab(layer, width, height, b.x, b.y, b.pressure, preset, dirty);
            if (bounds) bounds->expand(b.x, b.y, effectiveRadius(b.pressure, preset) + 1.f);
            carryDist = step;
        }
        return;
    }

    float traveled = carryDist;
    while (traveled <= dist) {
        const float t = traveled / dist;
        const float x = a.x + dx * t;
        const float y = a.y + dy * t;
        const float pr = a.pressure + (b.pressure - a.pressure) * t;
        stampDab(layer, width, height, x, y, pr, preset, dirty);
        if (bounds) bounds->expand(x, y, effectiveRadius(pr, preset) + 1.f);
        traveled += step;
    }
    carryDist = traveled - dist;
}

} // namespace illus
