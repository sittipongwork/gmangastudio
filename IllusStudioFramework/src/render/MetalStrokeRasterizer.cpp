//
//  MetalStrokeRasterizer.cpp
//  IllusStudioFramework — Metal compute dab stamp (T1-3)
//

#include "MetalStrokeRasterizer.hpp"

#include "StrokeRasterizer.hpp"

#include <Metal/Metal.hpp>

#include <algorithm>
#include <cmath>

namespace illus {
namespace {

// Must match MetalShaders DabUniforms (16-byte aligned float4 + ints).
struct DabUniforms {
    float centerX, centerY;
    float radius;
    float hardness;
    float colorR, colorG, colorB, colorA;
    int32_t erase;
    int32_t width;
    int32_t height;
    int32_t originX;
    int32_t originY;
    int32_t pad0;
    int32_t pad1;
    int32_t pad2;
};

DabUniforms makeUniforms(
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    int32_t width,
    int32_t height,
    int minX,
    int minY
) {
    const float p = std::clamp(pressure, 0.f, 1.f);
    const float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    const float alphaF = std::clamp(preset.opacity * preset.flow * opacScale, 0.f, 1.f);
    const float radius = StrokeRasterizer::effectiveRadius(pressure, preset);
    DabUniforms u{};
    u.centerX = x;
    u.centerY = y;
    u.radius = radius;
    u.hardness = preset.hardness;
    u.colorR = preset.color.r / 255.f;
    u.colorG = preset.color.g / 255.f;
    u.colorB = preset.color.b / 255.f;
    u.colorA = alphaF;
    u.erase = preset.mode == BrushMode::Erase ? 1 : 0;
    u.width = width;
    u.height = height;
    u.originX = minX;
    u.originY = minY;
    return u;
}

bool encodeDab(
    MTL::ComputeCommandEncoder* enc,
    MTL::ComputePipelineState* pipeline,
    MTL::Texture* tex,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty
) {
    const float radius = StrokeRasterizer::effectiveRadius(pressure, preset);
    if (radius < 0.25f) return false;

    const float p = std::clamp(pressure, 0.f, 1.f);
    const float opacScale = 1.f - preset.opacityPressure + preset.opacityPressure * p;
    const float alphaF = std::clamp(preset.opacity * preset.flow * opacScale, 0.f, 1.f);
    if (alphaF <= 0.f) return false;

    const int minX = std::max(0, static_cast<int>(std::floor(x - radius)));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(x + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - radius)));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(y + radius)));
    if (minX > maxX || minY > maxY) return false;

    DabUniforms u = makeUniforms(x, y, pressure, preset, width, height, minX, minY);
    enc->setComputePipelineState(pipeline);
    enc->setTexture(tex, 0);
    enc->setBytes(&u, sizeof(u), 0);

    const NS::UInteger tw = static_cast<NS::UInteger>(maxX - minX + 1);
    const NS::UInteger th = static_cast<NS::UInteger>(maxY - minY + 1);
    const NS::UInteger wtg = pipeline->threadExecutionWidth();
    const NS::UInteger htg = std::max(NS::UInteger(1), pipeline->maxTotalThreadsPerThreadgroup() / wtg);
    enc->dispatchThreads(MTL::Size(tw, th, 1), MTL::Size(wtg, htg, 1));

    dirty.unionWith(minX, minY, maxX, maxY);
    return true;
}

} // namespace

bool MetalStrokeRasterizer::stampDab(
    MTL::CommandQueue* queue,
    MTL::ComputePipelineState* pipeline,
    MTL::Texture* tex,
    int32_t width,
    int32_t height,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty
) {
    if (!queue || !pipeline || !tex || width < 1 || height < 1) return false;

    // commandBuffer() is autoreleased — do not release.
    MTL::CommandBuffer* cmd = queue->commandBuffer();
    if (!cmd) return false;
    MTL::ComputeCommandEncoder* enc = cmd->computeCommandEncoder();
    if (!enc) return false;

    const bool ok = encodeDab(enc, pipeline, tex, width, height, x, y, pressure, preset, dirty);
    enc->endEncoding();
    if (ok) {
        cmd->commit();
        cmd->waitUntilCompleted();
    }
    return ok;
}

void MetalStrokeRasterizer::stampSegment(
    MTL::CommandQueue* queue,
    MTL::ComputePipelineState* pipeline,
    MTL::Texture* tex,
    int32_t width,
    int32_t height,
    const StrokeSample& a,
    const StrokeSample& b,
    const BrushPreset& preset,
    float& carryDist,
    math::Rect& dirty,
    StrokeBounds* bounds
) {
    if (!queue || !pipeline || !tex) return;

    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float midP = 0.5f * (a.pressure + b.pressure);
    const float step = std::max(0.5f, preset.spacing * 2.f * StrokeRasterizer::effectiveRadius(midP, preset));

    // commandBuffer() is autoreleased — do not release.
    MTL::CommandBuffer* cmd = queue->commandBuffer();
    if (!cmd) return;
    MTL::ComputeCommandEncoder* enc = cmd->computeCommandEncoder();
    if (!enc) return;

    bool any = false;
    if (dist < 1e-4f) {
        if (carryDist <= 0.f) {
            any = encodeDab(enc, pipeline, tex, width, height, b.x, b.y, b.pressure, preset, dirty);
            if (bounds) bounds->expand(b.x, b.y, StrokeRasterizer::effectiveRadius(b.pressure, preset) + 1.f);
            carryDist = step;
        }
    } else {
        float traveled = carryDist;
        while (traveled <= dist) {
            const float t = traveled / dist;
            const float x = a.x + dx * t;
            const float y = a.y + dy * t;
            const float pr = a.pressure + (b.pressure - a.pressure) * t;
            any = encodeDab(enc, pipeline, tex, width, height, x, y, pr, preset, dirty) || any;
            if (bounds) bounds->expand(x, y, StrokeRasterizer::effectiveRadius(pr, preset) + 1.f);
            traveled += step;
        }
        carryDist = traveled - dist;
    }

    enc->endEncoding();
    if (any) {
        cmd->commit();
        cmd->waitUntilCompleted();
    }
}

} // namespace illus
