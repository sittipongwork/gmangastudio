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

/// Round dab coverage: hard core + smooth falloff + ~1px AA fringe.
float coverageAt(float dist, float radius, float hardness) {
    if (radius <= 0.f) return 0.f;
    constexpr float kAA = 1.f;
    if (dist >= radius + kAA) return 0.f;

    const float soft = std::clamp(1.f - hardness, 0.f, 1.f);
    // soft=0 → core almost to edge (ink); soft=1 → falloff from near center (airbrush).
    const float coreR = radius * (1.f - soft * 0.92f);

    float cov = 1.f;
    if (dist > coreR) {
        const float span = std::max(radius - coreR, 1e-4f);
        const float u = std::clamp((dist - coreR) / span, 0.f, 1.f);
        cov = 1.f - (u * u * (3.f - 2.f * u));
    }
    if (dist > radius) {
        cov *= 1.f - (dist - radius) / kAA;
    }
    return std::clamp(cov, 0.f, 1.f);
}

uint8_t* pixelAt(uint8_t* pixels, int32_t width, int32_t x, int32_t y) {
    return pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
}

float stampSpacing(const BrushPreset& preset) {
    // Continuous stroke: keep steps ≤ ~15% of diameter. Tip presets were often too sparse.
    float spacing = std::clamp(preset.spacing, 0.04f, 0.15f);
    if (preset.tipTextureId >= 0) spacing = std::min(spacing, 0.10f);
    return spacing;
}

float stampHardness(const BrushPreset& preset) {
    // Until T1-7-3b tip/grain: imported soft tips → muddy rings; floor toward ink.
    if (preset.tipTextureId >= 0) return std::clamp(std::max(preset.hardness, 0.88f), 0.f, 1.f);
    return std::clamp(preset.hardness, 0.f, 1.f);
}

float stampAlpha(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    const float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    float flow = preset.flow;
    // Tip-bearing Procreate maps often ship tiny flow → translucent speckled buildup.
    if (preset.tipTextureId >= 0) flow = std::max(flow, 0.95f);
    return std::clamp(preset.opacity * flow * opacScale, 0.f, 1.f);
}

void stampSegmentInto(
    uint8_t* pixels,
    int32_t width,
    int32_t height,
    const StrokeSample& a,
    const StrokeSample& b,
    const BrushPreset& preset,
    float& carryDist,
    math::Rect& dirty,
    StrokeBounds* bounds,
    const BrushAssetStore* assets
) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float midP = 0.5f * (a.pressure + b.pressure);
    const float radius = StrokeRasterizer::effectiveRadius(midP, preset);
    const float step = std::max(0.35f, stampSpacing(preset) * 2.f * radius);

    if (dist < 1e-4f) {
        if (carryDist <= 0.f) {
            StrokeRasterizer::stampDab(pixels, width, height, b.x, b.y, b.pressure, preset, dirty, assets);
            if (bounds) bounds->expand(b.x, b.y, StrokeRasterizer::effectiveRadius(b.pressure, preset) + 1.f);
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
        StrokeRasterizer::stampDab(pixels, width, height, x, y, pr, preset, dirty, assets);
        if (bounds) bounds->expand(x, y, StrokeRasterizer::effectiveRadius(pr, preset) + 1.f);
        traveled += step;
    }
    carryDist = traveled - dist;
}

} // namespace

float StrokeRasterizer::effectiveRadius(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    const float scale = 1.f - preset.sizePressure + preset.sizePressure * std::max(0.05f, p);
    return 0.5f * preset.lineWidthPx * scale;
}

bool StrokeRasterizer::stampDab(
    uint8_t* pixels,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty,
    const BrushAssetStore* /*assets*/
) {
    if (!pixels) return false;

    // ponytail: skip Shape.png tip stamp — imported tips bake grain into coverage (speckles /
    // dark cores). Procedural round only; tip+grain returns as T1-7-3b.

    const float radius = effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;

    const float alphaF = stampAlpha(pressure, preset);
    if (alphaF <= 0.f) return false;

    const float hard = stampHardness(preset);
    constexpr float kAA = 1.f;
    const int minX = std::max(0, static_cast<int>(std::floor(x - radius - kAA)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + radius + kAA)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - radius - kAA)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + radius + kAA)));
    if (minX > maxX || minY > maxY) return false;

    const bool erase = preset.mode == BrushMode::Erase;
    const float outer2 = (radius + kAA) * (radius + kAA);
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > outer2) continue;
            const float cov = coverageAt(std::sqrt(d2), radius, hard);
            if (cov <= 0.004f) continue;
            const uint8_t a = static_cast<uint8_t>(std::clamp(cov * alphaF * 255.f + 0.5f, 0.f, 255.f));
            uint8_t* dst = pixelAt(pixels, width, px, py);
            if (erase) {
                math::blendDestOut(dst, a);
            } else {
                math::blendPaintOver(dst, preset.color.r, preset.color.g, preset.color.b, a);
            }
        }
    }
    dirty.unionWith(minX, minY, maxX, maxY);
    dirty.clipTo(width, height);
    return true;
}

bool StrokeRasterizer::stampDab(
    Layer& layer,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty,
    const BrushAssetStore* assets
) {
    layer.ensurePixels(width, height);
    return stampDab(layer.pixels.data(), width, height, x, y, pressure, preset, dirty, assets);
}

void StrokeRasterizer::stampSegment(
    uint8_t* pixels,
    int32_t width,
    int32_t height,
    const StrokeSample& a,
    const StrokeSample& b,
    const BrushPreset& preset,
    float& carryDist,
    math::Rect& dirty,
    StrokeBounds* bounds,
    const BrushAssetStore* assets
) {
    stampSegmentInto(pixels, width, height, a, b, preset, carryDist, dirty, bounds, assets);
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
    StrokeBounds* bounds,
    const BrushAssetStore* assets
) {
    layer.ensurePixels(width, height);
    stampSegmentInto(layer.pixels.data(), width, height, a, b, preset, carryDist, dirty, bounds, assets);
}

} // namespace illus
