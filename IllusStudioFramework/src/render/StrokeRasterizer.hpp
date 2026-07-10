//
//  StrokeRasterizer.hpp
//  IllusStudioFramework — CPU vector → layer pixels (StampEngine)
//

#pragma once

#include "../layers/Layer.hpp"
#include "../math/Rect.hpp"
#include "../strokes/Stroke.hpp"
#include "../strokes/StrokeSample.hpp"
#include "../tools/BrushAssetStore.hpp"

#include <cstdint>

namespace illus {

class StrokeRasterizer {
public:
    /// Stamp one dab. `strokeDistPx` drives Moving grain + dynamics.
    static bool stampDab(
        uint8_t* pixels,
        int32_t width,
        int32_t height,
        float x,
        float y,
        float pressure,
        const BrushPreset& preset,
        math::Rect& dirty,
        const BrushAssetStore* assets = nullptr,
        float strokeDistPx = 0.f
    );

    static bool stampDab(
        Layer& layer,
        int32_t width,
        int32_t height,
        float x,
        float y,
        float pressure,
        const BrushPreset& preset,
        math::Rect& dirty,
        const BrushAssetStore* assets = nullptr,
        float strokeDistPx = 0.f
    );

    static void stampSegment(
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
        const BrushAssetStore* assets = nullptr,
        float totalStrokeLen = -1.f
    );

    static void stampSegment(
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
        const BrushAssetStore* assets = nullptr,
        float totalStrokeLen = -1.f
    );

    static float effectiveRadius(float pressure, const BrushPreset& preset);

    /// Start (+ optional end when totalLen >= 0) taper factor 0..1.
    static float strokeTaperFactor(
        float strokeDistPx,
        float pressure,
        const BrushPreset& preset,
        float totalStrokeLen = -1.f
    );

    /// Apply taper / speed / tilt / pressure curves for a dab.
    static BrushPreset withStrokeDynamics(
        const BrushPreset& preset,
        float pressure,
        float strokeDistPx,
        float totalStrokeLen = -1.f,
        float speed = 0.f,
        float tiltX = 0.f,
        float tiltY = 0.f
    );
};

} // namespace illus
