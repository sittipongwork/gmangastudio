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
    const float t = dist / radius;
    const float softStart = std::clamp(1.f - hardness, 0.05f, 1.f);
    if (t <= softStart) return 1.f;
    const float u = (t - softStart) / (1.f - softStart);
    return 1.f - (u * u * (3.f - 2.f * u));
}

uint8_t* pixelAt(uint8_t* pixels, int32_t width, int32_t x, int32_t y) {
    return pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
}

bool stampTipDab(
    uint8_t* pixels,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    const BrushTexture& tip,
    math::Rect& dirty
) {
    const float radius = StrokeRasterizer::effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;

    const float p = std::clamp(pressure, 0.f, 1.f);
    const float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    const float alphaF = std::clamp(preset.opacity * preset.flow * opacScale, 0.f, 1.f);
    if (alphaF <= 0.f) return false;

    const float diameter = radius * 2.f;
    const float angle = preset.angleDeg * (3.14159265f / 180.f);
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);

    const int minX = std::max(0, static_cast<int>(std::floor(x - radius - 1.f)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + radius + 1.f)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - radius - 1.f)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + radius + 1.f)));
    if (minX > maxX || minY > maxY) return false;

    const bool erase = preset.mode == BrushMode::Erase;
    const float invDiam = 1.f / std::max(1.f, diameter);

    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            // Rotate into tip space, map to [0,1] UV over diameter.
            const float rx = dx * ca + dy * sa;
            const float ry = -dx * sa + dy * ca;
            const float u = rx * invDiam + 0.5f;
            const float v = ry * invDiam + 0.5f;
            if (u < 0.f || v < 0.f || u >= 1.f || v >= 1.f) continue;

            const int tx = std::min(tip.width - 1, static_cast<int>(u * tip.width));
            const int ty = std::min(tip.height - 1, static_cast<int>(v * tip.height));
            const uint8_t* src = tip.rgba.data()
                + (static_cast<size_t>(ty) * static_cast<size_t>(tip.width) + static_cast<size_t>(tx)) * 4u;
            // Tip coverage from luminance or alpha (Shape.png often grayscale in alpha/RGB).
            const float tipA = std::max({src[0], src[1], src[2], src[3]}) / 255.f;
            if (tipA <= 0.001f) continue;
            const uint8_t a = static_cast<uint8_t>(std::clamp(tipA * alphaF * 255.f, 0.f, 255.f));
            uint8_t* dst = pixelAt(pixels, width, px, py);
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
    const float step = std::max(0.5f, preset.spacing * 2.f * StrokeRasterizer::effectiveRadius(midP, preset));

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
    const BrushAssetStore* assets
) {
    if (!pixels) return false;

    if (assets && preset.tipTextureId >= 0) {
        if (const BrushTexture* tip = assets->find(preset.tipTextureId)) {
            if (tip->width > 0 && tip->height > 0 && !tip->rgba.empty()) {
                return stampTipDab(pixels, width, height, x, y, pressure, preset, *tip, dirty);
            }
        }
    }

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
            uint8_t* dst = pixelAt(pixels, width, px, py);
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
