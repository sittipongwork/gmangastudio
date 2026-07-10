//
//  StrokeRasterizer.cpp
//  IllusStudioFramework
//

#include "StrokeRasterizer.hpp"

#include "../math/Blend.hpp"
#include "../tools/BrushAssetStore.hpp"

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

float tipRawAt(const BrushTexture& tip, int x, int y) {
    x = std::clamp(x, 0, tip.width - 1);
    y = std::clamp(y, 0, tip.height - 1);
    const uint8_t* src = tip.rgba.data()
        + (static_cast<size_t>(y) * static_cast<size_t>(tip.width) + static_cast<size_t>(x)) * 4u;
    // Shape may live in RGB or alpha (fixture tips are near-black RGB + opaque A).
    return std::max({src[0], src[1], src[2], src[3]}) / 255.f;
}

/// Grain/texture: Procreate Grain.png is grayscale RGB with A=255 after CG decode.
/// Must NOT max with A or opaque dark grain reads as 1 and canvas stamps go solid.
float grainRawAt(const BrushTexture& grain, int x, int y) {
    x = std::clamp(x, 0, grain.width - 1);
    y = std::clamp(y, 0, grain.height - 1);
    const uint8_t* src = grain.rgba.data()
        + (static_cast<size_t>(y) * static_cast<size_t>(grain.width) + static_cast<size_t>(x)) * 4u;
    return std::max({src[0], src[1], src[2]}) / 255.f;
}

float tipSampleBilinear(const BrushTexture& tip, float u, float v) {
    if (tip.width < 1 || tip.height < 1 || tip.rgba.empty()) return 0.f;
    const float fx = std::clamp(u, 0.f, 1.f) * static_cast<float>(tip.width - 1);
    const float fy = std::clamp(v, 0.f, 1.f) * static_cast<float>(tip.height - 1);
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(tip.width - 1, x0 + 1);
    const int y1 = std::min(tip.height - 1, y0 + 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float c00 = tipRawAt(tip, x0, y0);
    const float c10 = tipRawAt(tip, x1, y0);
    const float c01 = tipRawAt(tip, x0, y1);
    const float c11 = tipRawAt(tip, x1, y1);
    const float a = c00 * (1.f - tx) + c10 * tx;
    const float b = c01 * (1.f - tx) + c11 * tx;
    return a * (1.f - ty) + b * ty;
}

float grainSampleBilinear(const BrushTexture& grain, float u, float v) {
    if (grain.width < 1 || grain.height < 1 || grain.rgba.empty()) return 1.f;
    const float fx = std::clamp(u, 0.f, 1.f) * static_cast<float>(grain.width - 1);
    const float fy = std::clamp(v, 0.f, 1.f) * static_cast<float>(grain.height - 1);
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(grain.width - 1, x0 + 1);
    const int y1 = std::min(grain.height - 1, y0 + 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float c00 = grainRawAt(grain, x0, y0);
    const float c10 = grainRawAt(grain, x1, y0);
    const float c01 = grainRawAt(grain, x0, y1);
    const float c11 = grainRawAt(grain, x1, y1);
    const float a = c00 * (1.f - tx) + c10 * tx;
    const float b = c01 * (1.f - tx) + c11 * tx;
    return a * (1.f - ty) + b * ty;
}

bool tipNeedsInvert(const BrushTexture& tip) {
    // Invert only for dark stamps on a light field (center dark, border bright).
    // Uniform bright tips (fixture Shape.png) must NOT invert — that zeroed coverage.
    if (tip.width < 2 || tip.height < 2) return false;
    const float center = tipRawAt(tip, tip.width / 2, tip.height / 2);
    double borderSum = 0;
    int borderN = 0;
    const int w = tip.width;
    const int h = tip.height;
    for (int x = 0; x < w; ++x) {
        borderSum += tipRawAt(tip, x, 0) + tipRawAt(tip, x, h - 1);
        borderN += 2;
    }
    for (int y = 1; y < h - 1; ++y) {
        borderSum += tipRawAt(tip, 0, y) + tipRawAt(tip, w - 1, y);
        borderN += 2;
    }
    if (borderN < 1) return false;
    const float border = static_cast<float>(borderSum / borderN);
    return border > 0.55f && center < 0.45f;
}

float stampSpacing(const BrushPreset& preset) {
    float spacing = std::clamp(preset.spacing, 0.02f, 0.20f);
    if (preset.tipTextureId >= 0) spacing = std::min(spacing, 0.12f);
    return spacing;
}

float stampHardness(const BrushPreset& preset) {
    if (preset.tipTextureId >= 0) return std::clamp(std::max(preset.hardness, 0.75f), 0.f, 1.f);
    return std::clamp(preset.hardness, 0.f, 1.f);
}

float stampAlpha(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    const float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    float flow = preset.flow;
    // Solid tip ink only — grain brushes must keep low flow (set in resolvedPreset).
    if (preset.tipTextureId >= 0 && preset.grainTextureId < 0) flow = std::max(flow, 0.85f);
    return std::clamp(preset.opacity * flow * opacScale, 0.f, 1.f);
}

float grainAt(const BrushTexture& grain, float canvasX, float canvasY, float grainScale) {
    if (grain.width < 1 || grain.height < 1) return 1.f;
    const float scale = std::max(0.05f, grainScale);
    float u = std::fmod(canvasX * scale / static_cast<float>(grain.width), 1.f);
    float v = std::fmod(canvasY * scale / static_cast<float>(grain.height), 1.f);
    if (u < 0.f) u += 1.f;
    if (v < 0.f) v += 1.f;
    return grainSampleBilinear(grain, u, v);
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
    const BrushTexture* grain,
    math::Rect& dirty
) {
    const float radius = StrokeRasterizer::effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;
    const float alphaF = stampAlpha(pressure, preset);
    if (alphaF <= 0.f) return false;

    const bool invert = preset.shapeInverted || tipNeedsInvert(tip);
    const float diameter = radius * 2.f;
    const float angle = preset.angleDeg * (3.14159265f / 180.f);
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);
    const float invDiam = 1.f / std::max(1.f, diameter);

    constexpr float kAA = 1.f;
    const int minX = std::max(0, static_cast<int>(std::floor(x - radius - kAA)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + radius + kAA)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - radius - kAA)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + radius + kAA)));
    if (minX > maxX || minY > maxY) return false;

    const bool erase = preset.mode == BrushMode::Erase;
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius + kAA) continue;

            const float rx = dx * ca + dy * sa;
            const float ry = -dx * sa + dy * ca;
            const float u = rx * invDiam + 0.5f;
            const float v = ry * invDiam + 0.5f;
            if (u < 0.f || v < 0.f || u > 1.f || v > 1.f) continue;

            float tipA = tipSampleBilinear(tip, u, v);
            if (invert) tipA = 1.f - tipA;
            // Soft threshold: kill near-zero grain speckles in Shape.png.
            if (tipA < 0.08f) continue;
            tipA = (tipA - 0.08f) / 0.92f;
            if (grain) {
                // Procreate: mix(1, grain, grainDepth) then punch — keep dark grain as holes.
                float g = grainAt(*grain, px + 0.5f, py + 0.5f, preset.grainScale);
                const float depth = std::clamp(preset.grainDepth, 0.f, 1.f);
                g = 1.f - depth + depth * g;
                // Contrast so mid-gray hatch reads on canvas (not muddy solid).
                g = std::clamp(g * g, 0.f, 1.f);
                tipA *= g;
            }
            // ponytail: tip PNG is the silhouette — soft AA window only (no round mask).
            float window = 1.f;
            if (dist > radius) {
                window = 1.f - (dist - radius) / kAA;
                if (window <= 0.004f) continue;
            }
            const float cov = tipA * window;
            if (cov <= 0.004f) continue;
            const uint8_t a = static_cast<uint8_t>(std::clamp(cov * alphaF * 255.f + 0.5f, 0.f, 255.f));
            uint8_t* dst = pixelAt(pixels, width, px, py);
            if (erase) math::blendDestOut(dst, a);
            else math::blendPaintOver(dst, preset.color.r, preset.color.g, preset.color.b, a);
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
    float& strokeDistPx,
    math::Rect& dirty,
    StrokeBounds* bounds,
    const BrushAssetStore* assets
) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float midP = 0.5f * (a.pressure + b.pressure);
    const BrushPreset midPreset = StrokeRasterizer::withStrokeDynamics(preset, midP, strokeDistPx);
    const float radius = StrokeRasterizer::effectiveRadius(midP, midPreset);
    const float step = std::max(0.35f, stampSpacing(midPreset) * 2.f * radius);

    if (dist < 1e-4f) {
        if (carryDist <= 0.f) {
            const BrushPreset dabPreset = StrokeRasterizer::withStrokeDynamics(preset, b.pressure, strokeDistPx);
            StrokeRasterizer::stampDab(pixels, width, height, b.x, b.y, b.pressure, dabPreset, dirty, assets);
            if (bounds) {
                bounds->expand(b.x, b.y, StrokeRasterizer::effectiveRadius(b.pressure, dabPreset) + 1.f);
            }
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
        BrushPreset dabPreset = StrokeRasterizer::withStrokeDynamics(preset, pr, strokeDistPx + traveled);
        if (preset.orientTip) {
            dabPreset.angleDeg = preset.angleDeg + std::atan2(dy, dx) * (180.f / 3.14159265f);
        }
        StrokeRasterizer::stampDab(pixels, width, height, x, y, pr, dabPreset, dirty, assets);
        if (bounds) {
            bounds->expand(x, y, StrokeRasterizer::effectiveRadius(pr, dabPreset) + 1.f);
        }
        traveled += step;
    }
    carryDist = traveled - dist;
    strokeDistPx += dist;
}

} // namespace

float StrokeRasterizer::effectiveRadius(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    const float minS = std::clamp(preset.minSize, 0.f, 1.f);
    // Procreate-like: pressure maps minSize→1 when sizePressure>0; else full width.
    const float sized = minS + (1.f - minS) * p;
    const float scale = 1.f - preset.sizePressure + preset.sizePressure * sized;
    return 0.5f * preset.lineWidthPx * std::max(0.05f, scale);
}

float StrokeRasterizer::strokeTaperFactor(float strokeDistPx, float pressure, const BrushPreset& preset) {
    float taperAmt = std::max(preset.taperSize, 0.f);
    if (taperAmt <= 0.001f) return 1.f;

    const float tipLen = std::max(24.f, preset.lineWidthPx * 8.f) * std::clamp(taperAmt, 0.f, 1.f);
    float u = tipLen > 1e-4f ? std::clamp(strokeDistPx / tipLen, 0.f, 1.f) : 1.f;
    u = u * u * (3.f - 2.f * u); // smoothstep
    const float minS = std::clamp(std::max(preset.minSize, 0.05f), 0.f, 1.f);
    float factor = minS + (1.f - minS) * u;

    if (preset.taperPressure > 0.f) {
        const float p = std::clamp(pressure, 0.f, 1.f);
        factor *= 1.f - preset.taperPressure + preset.taperPressure * p;
    }
    return std::clamp(factor, 0.05f, 1.f);
}

BrushPreset StrokeRasterizer::withStrokeDynamics(
    const BrushPreset& preset,
    float pressure,
    float strokeDistPx
) {
    BrushPreset out = preset;
    const float taper = strokeTaperFactor(strokeDistPx, pressure, preset);
    out.lineWidthPx = std::max(0.5f, preset.lineWidthPx * taper);
    if (preset.taperOpacity > 0.f) {
        out.opacity = std::clamp(
            preset.opacity * (1.f - preset.taperOpacity + preset.taperOpacity * taper),
            0.f,
            1.f
        );
    }
    return out;
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
                const BrushTexture* grain = nullptr;
                if (preset.grainTextureId >= 0) {
                    grain = assets->find(preset.grainTextureId);
                    if (grain && (grain->width < 1 || grain->rgba.empty())) grain = nullptr;
                }
                return stampTipDab(pixels, width, height, x, y, pressure, preset, *tip, grain, dirty);
            }
        }
    }

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
    float& strokeDistPx,
    math::Rect& dirty,
    StrokeBounds* bounds,
    const BrushAssetStore* assets
) {
    stampSegmentInto(pixels, width, height, a, b, preset, carryDist, strokeDistPx, dirty, bounds, assets);
}

void StrokeRasterizer::stampSegment(
    Layer& layer,
    int32_t width,
    int32_t height,
    const StrokeSample& a,
    const StrokeSample& b,
    const BrushPreset& preset,
    float& carryDist,
    float& strokeDistPx,
    math::Rect& dirty,
    StrokeBounds* bounds,
    const BrushAssetStore* assets
) {
    layer.ensurePixels(width, height);
    stampSegmentInto(
        layer.pixels.data(), width, height, a, b, preset, carryDist, strokeDistPx, dirty, bounds, assets
    );
}

} // namespace illus
