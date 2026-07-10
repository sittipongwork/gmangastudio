//
//  StrokeRasterizer.cpp
//  IllusStudioFramework — StampEngine (Shape + Grain + Stroke Path)
//

#include "StrokeRasterizer.hpp"

#include "../math/Blend.hpp"
#include "../tools/BrushAssetStore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace illus {
namespace {

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
    return std::max({src[0], src[1], src[2], src[3]}) / 255.f;
}

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
    return std::clamp(preset.spacing, 0.02f, 0.35f);
}

float stampHardness(const BrushPreset& preset) {
    return std::clamp(preset.hardness, 0.f, 1.f);
}

float stampAlpha(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    if (preset.opacityCurveValid) {
        const float t = p;
        const float cy = preset.opacityCurveY0 + (preset.opacityCurveY1 - preset.opacityCurveY0) * t;
        opacScale = std::clamp(cy, 0.f, 1.f);
    }
    return std::clamp(preset.opacity * preset.flow * opacScale, 0.f, 1.f);
}

float wrap01(float v) {
    v = std::fmod(v, 1.f);
    if (v < 0.f) v += 1.f;
    return v;
}

float grainAt(
    const BrushTexture& grain,
    float canvasX,
    float canvasY,
    float tipU,
    float tipV,
    float strokeDistPx,
    const BrushPreset& preset
) {
    if (grain.width < 1 || grain.height < 1) return 1.f;
    const float scale = std::max(0.05f, preset.grainScale);
    float u = 0.f;
    float v = 0.f;
    if (preset.grainMoving) {
        // Moving: tip-local UV rolls along stroke distance (paint roller).
        u = wrap01(tipU + strokeDistPx * scale / static_cast<float>(grain.width));
        v = wrap01(tipV);
    } else {
        // Texturized: canvas-locked tiling.
        u = wrap01(canvasX * scale / static_cast<float>(grain.width));
        v = wrap01(canvasY * scale / static_cast<float>(grain.height));
    }
    return grainSampleBilinear(grain, u, v);
}

// Deterministic hash → 0..1 (scatter without RNG state).
float hash01(float x, float y, int salt) {
    const float n = std::sin(x * 12.9898f + y * 78.233f + static_cast<float>(salt) * 45.164f) * 43758.5453f;
    return n - std::floor(n);
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
    math::Rect& dirty,
    float strokeDistPx
) {
    const float radius = StrokeRasterizer::effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;
    const float alphaF = stampAlpha(pressure, preset);
    if (alphaF <= 0.f) return false;

    const bool invert = preset.shapeInverted || tipNeedsInvert(tip);
    const float diameter = radius * 2.f;
    const float roundness = std::clamp(preset.roundness, 0.15f, 1.f);
    const float angle = preset.angleDeg * (3.14159265f / 180.f);
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);
    const float invDiamX = 1.f / std::max(1.f, diameter);
    const float invDiamY = 1.f / std::max(1.f, diameter * roundness);

    // Ellipse AABB.
    const float rxBound = radius;
    const float ryBound = radius * roundness;
    const float ext = std::max(rxBound, ryBound) + 1.f;
    constexpr float kAA = 1.f;
    const int minX = std::max(0, static_cast<int>(std::floor(x - ext - kAA)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + ext + kAA)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - ext - kAA)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + ext + kAA)));
    if (minX > maxX || minY > maxY) return false;

    const bool erase = preset.mode == BrushMode::Erase;
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            const float rx = dx * ca + dy * sa;
            const float ry = -dx * sa + dy * ca;
            // Ellipse normalized distance.
            const float nx = rx * invDiamX;
            const float ny = ry * invDiamY;
            const float nd = std::sqrt(nx * nx + ny * ny) * 2.f; // 0 at center, 1 at edge
            if (nd > 1.f + kAA / std::max(radius, 1.f)) continue;

            const float u = rx * invDiamX + 0.5f;
            const float v = ry * invDiamY + 0.5f;
            if (u < 0.f || v < 0.f || u > 1.f || v > 1.f) continue;

            float tipA = tipSampleBilinear(tip, u, v);
            if (invert) tipA = 1.f - tipA;
            if (tipA < 0.08f) continue;
            tipA = (tipA - 0.08f) / 0.92f;
            if (grain) {
                float g = grainAt(
                    *grain, px + 0.5f, py + 0.5f, u, v, strokeDistPx, preset
                );
                const float depth = std::clamp(preset.grainDepth, 0.f, 1.f);
                g = 1.f - depth + depth * g;
                g = std::clamp(g * g, 0.f, 1.f);
                tipA *= g;
            }
            float window = 1.f;
            if (nd > 1.f) {
                window = 1.f - (nd - 1.f) * radius / kAA;
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

void stampOnePlacement(
    uint8_t* pixels,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty,
    const BrushAssetStore* assets,
    float strokeDistPx
) {
    StrokeRasterizer::stampDab(
        pixels, width, height, x, y, pressure, preset, dirty, assets, strokeDistPx
    );
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
    const BrushAssetStore* assets,
    float totalStrokeLen
) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    float speed = 0.f;
    const float dt = b.t - a.t;
    if (dt > 1e-5f) speed = dist / dt;
    else if (dist > 1e-4f) speed = dist * 60.f; // assume ~60Hz samples

    const float midP = 0.5f * (a.pressure + b.pressure);
    const float midTiltX = 0.5f * (a.tiltX + b.tiltX);
    const float midTiltY = 0.5f * (a.tiltY + b.tiltY);
    BrushPreset midPreset = StrokeRasterizer::withStrokeDynamics(
        preset, midP, strokeDistPx, totalStrokeLen, speed, midTiltX, midTiltY
    );
    float spacing = stampSpacing(midPreset);
    if (preset.speedSpacing > 0.f && speed > 0.f) {
        const float s = std::clamp(speed / 800.f, 0.f, 1.f);
        spacing = std::clamp(spacing * (1.f + preset.speedSpacing * s), 0.02f, 0.4f);
    }
    const float radius = StrokeRasterizer::effectiveRadius(midP, midPreset);
    const float step = std::max(0.35f, spacing * 2.f * radius);
    const int count = std::clamp(preset.stampCount, 1, 4);
    const float scatter = std::clamp(preset.scatter, 0.f, 1.f);
    const float rotJit = std::clamp(preset.rotationJitter, 0.f, 1.f);

    auto placeAt = [&](float x, float y, float pr, float distAlong, float tiltX, float tiltY) {
        BrushPreset dabPreset = StrokeRasterizer::withStrokeDynamics(
            preset, pr, distAlong, totalStrokeLen, speed, tiltX, tiltY
        );
        if (preset.orientTip) {
            dabPreset.angleDeg = preset.angleDeg + std::atan2(dy, dx) * (180.f / 3.14159265f);
        }
        for (int c = 0; c < count; ++c) {
            float sx = x;
            float sy = y;
            BrushPreset p = dabPreset;
            if (scatter > 0.001f || rotJit > 0.001f) {
                const float h0 = hash01(x, y, c * 3 + 1);
                const float h1 = hash01(x, y, c * 3 + 2);
                const float h2 = hash01(x, y, c * 3 + 3);
                const float rad = StrokeRasterizer::effectiveRadius(pr, dabPreset);
                sx += (h0 - 0.5f) * 2.f * scatter * rad;
                sy += (h1 - 0.5f) * 2.f * scatter * rad;
                p.angleDeg += (h2 - 0.5f) * 2.f * rotJit * 180.f;
            }
            stampOnePlacement(pixels, width, height, sx, sy, pr, p, dirty, assets, distAlong);
            if (bounds) {
                bounds->expand(sx, sy, StrokeRasterizer::effectiveRadius(pr, p) + 1.f);
            }
        }
    };

    if (dist < 1e-4f) {
        if (carryDist <= 0.f) {
            placeAt(b.x, b.y, b.pressure, strokeDistPx, b.tiltX, b.tiltY);
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
        const float tx = a.tiltX + (b.tiltX - a.tiltX) * t;
        const float ty = a.tiltY + (b.tiltY - a.tiltY) * t;
        placeAt(x, y, pr, strokeDistPx + traveled, tx, ty);
        traveled += step;
    }
    carryDist = traveled - dist;
    strokeDistPx += dist;
}

} // namespace

float StrokeRasterizer::effectiveRadius(float pressure, const BrushPreset& preset) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    float sized = 0.f;
    if (preset.sizeCurveValid) {
        sized = preset.sizeCurveY0 + (preset.sizeCurveY1 - preset.sizeCurveY0) * p;
        sized = std::clamp(sized, 0.05f, 1.f);
    } else {
        const float minS = std::clamp(preset.minSize, 0.f, 1.f);
        sized = minS + (1.f - minS) * p;
    }
    const float scale = 1.f - preset.sizePressure + preset.sizePressure * sized;
    return 0.5f * preset.lineWidthPx * std::max(0.05f, scale);
}

float StrokeRasterizer::strokeTaperFactor(
    float strokeDistPx,
    float pressure,
    const BrushPreset& preset,
    float totalStrokeLen
) {
    float factor = 1.f;
    const float minS = std::clamp(std::max(preset.minSize, 0.05f), 0.f, 1.f);

    auto taperFrom = [&](float distFromEnd, float amt) {
        if (amt <= 0.001f) return 1.f;
        const float tipLen = std::max(24.f, preset.lineWidthPx * 8.f) * std::clamp(amt, 0.f, 1.f);
        float u = tipLen > 1e-4f ? std::clamp(distFromEnd / tipLen, 0.f, 1.f) : 1.f;
        u = u * u * (3.f - 2.f * u);
        return minS + (1.f - minS) * u;
    };

    factor = std::min(factor, taperFrom(strokeDistPx, preset.taperSize));
    if (totalStrokeLen >= 0.f && preset.taperEndSize > 0.001f) {
        const float fromEnd = std::max(0.f, totalStrokeLen - strokeDistPx);
        factor = std::min(factor, taperFrom(fromEnd, preset.taperEndSize));
    }

    if (preset.taperPressure > 0.f) {
        const float p = std::clamp(pressure, 0.f, 1.f);
        factor *= 1.f - preset.taperPressure + preset.taperPressure * p;
    }
    return std::clamp(factor, 0.05f, 1.f);
}

BrushPreset StrokeRasterizer::withStrokeDynamics(
    const BrushPreset& preset,
    float pressure,
    float strokeDistPx,
    float totalStrokeLen,
    float speed,
    float tiltX,
    float tiltY
) {
    BrushPreset out = preset;
    const float taper = strokeTaperFactor(strokeDistPx, pressure, preset, totalStrokeLen);
    out.lineWidthPx = std::max(0.5f, preset.lineWidthPx * taper);

    if (preset.speedSize > 0.f && speed > 0.f) {
        const float s = std::clamp(speed / 800.f, 0.f, 1.f);
        out.lineWidthPx *= 1.f - preset.speedSize * s * 0.65f;
        out.lineWidthPx = std::max(0.5f, out.lineWidthPx);
    }
    const float tiltMag = std::clamp(std::sqrt(tiltX * tiltX + tiltY * tiltY), 0.f, 1.f);
    if (preset.tiltSize > 0.f) {
        out.lineWidthPx *= 1.f - preset.tiltSize * tiltMag * 0.5f;
        out.lineWidthPx = std::max(0.5f, out.lineWidthPx);
    }

    float opac = preset.opacity;
    if (preset.taperOpacity > 0.f) {
        opac *= 1.f - preset.taperOpacity + preset.taperOpacity * taper;
    }
    if (preset.speedOpacity > 0.f && speed > 0.f) {
        const float s = std::clamp(speed / 800.f, 0.f, 1.f);
        opac *= 1.f - preset.speedOpacity * s * 0.7f;
    }
    if (preset.tiltOpacity > 0.f) {
        opac *= 1.f - preset.tiltOpacity * tiltMag * 0.6f;
    }
    out.opacity = std::clamp(opac, 0.f, 1.f);
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
    const BrushAssetStore* assets,
    float strokeDistPx
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
                return stampTipDab(
                    pixels, width, height, x, y, pressure, preset, *tip, grain, dirty, strokeDistPx
                );
            }
        }
    }

    const float radius = effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;
    const float alphaF = stampAlpha(pressure, preset);
    if (alphaF <= 0.f) return false;

    const float hard = stampHardness(preset);
    const float roundness = std::clamp(preset.roundness, 0.15f, 1.f);
    const float angle = preset.angleDeg * (3.14159265f / 180.f);
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);
    constexpr float kAA = 1.f;
    const float ext = std::max(radius, radius * roundness) + kAA;
    const int minX = std::max(0, static_cast<int>(std::floor(x - ext)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + ext)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - ext)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + ext)));
    if (minX > maxX || minY > maxY) return false;

    const bool erase = preset.mode == BrushMode::Erase;
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            const float rx = (dx * ca + dy * sa) / radius;
            const float ry = (-dx * sa + dy * ca) / (radius * roundness);
            const float nd = std::sqrt(rx * rx + ry * ry);
            const float cov = coverageAt(nd * radius, radius, hard);
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

bool StrokeRasterizer::stampDab(
    Layer& layer,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty,
    const BrushAssetStore* assets,
    float strokeDistPx
) {
    layer.ensurePixels(width, height);
    return stampDab(
        layer.pixels.data(), width, height, x, y, pressure, preset, dirty, assets, strokeDistPx
    );
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
    const BrushAssetStore* assets,
    float totalStrokeLen
) {
    stampSegmentInto(
        pixels, width, height, a, b, preset, carryDist, strokeDistPx, dirty, bounds, assets, totalStrokeLen
    );
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
    const BrushAssetStore* assets,
    float totalStrokeLen
) {
    layer.ensurePixels(width, height);
    stampSegmentInto(
        layer.pixels.data(),
        width,
        height,
        a,
        b,
        preset,
        carryDist,
        strokeDistPx,
        dirty,
        bounds,
        assets,
        totalStrokeLen
    );
}

} // namespace illus
