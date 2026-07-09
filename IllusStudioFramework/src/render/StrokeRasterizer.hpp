//
//  StrokeRasterizer.hpp
//  IllusStudioFramework — CPU vector → layer pixels
//

#pragma once

#include "../layers/Layer.hpp"
#include "../math/Rect.hpp"
#include "../strokes/Stroke.hpp"
#include "../strokes/StrokeSample.hpp"

#include <cstdint>

namespace illus {

class StrokeRasterizer {
public:
    /// Stamp one dab into a full-frame RGBA buffer (size width*height*4).
    static bool stampDab(
        uint8_t* pixels,
        int32_t width,
        int32_t height,
        float x,
        float y,
        float pressure,
        const BrushPreset& preset,
        math::Rect& dirty
    );

    /// Stamp one dab onto a layer (ensures pixels).
    static bool stampDab(
        Layer& layer,
        int32_t width,
        int32_t height,
        float x,
        float y,
        float pressure,
        const BrushPreset& preset,
        math::Rect& dirty
    );

    static void stampSegment(
        uint8_t* pixels,
        int32_t width,
        int32_t height,
        const StrokeSample& a,
        const StrokeSample& b,
        const BrushPreset& preset,
        float& carryDist,
        math::Rect& dirty,
        StrokeBounds* bounds
    );

    static void stampSegment(
        Layer& layer,
        int32_t width,
        int32_t height,
        const StrokeSample& a,
        const StrokeSample& b,
        const BrushPreset& preset,
        float& carryDist,
        math::Rect& dirty,
        StrokeBounds* bounds
    );

    static float effectiveRadius(float pressure, const BrushPreset& preset);
};

} // namespace illus
