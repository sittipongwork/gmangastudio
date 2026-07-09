//
//  MetalStrokeRasterizer.hpp
//  IllusStudioFramework — Metal compute dab stamp (T1-3)
//

#pragma once

#include "../math/Rect.hpp"
#include "../strokes/Stroke.hpp"
#include "../strokes/StrokeSample.hpp"
#include "../tools/BrushPreset.hpp"

#include <cstdint>

namespace MTL {
class ComputePipelineState;
class CommandQueue;
class Texture;
}

namespace illus {

class MetalStrokeRasterizer {
public:
    /// Stamp one dab into `tex` (RGBA8, read_write). Returns false → caller uses CPU.
    static bool stampDab(
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
    );

    static void stampSegment(
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
    );
};

} // namespace illus
