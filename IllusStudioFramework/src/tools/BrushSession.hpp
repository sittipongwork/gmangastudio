//
//  BrushSession.hpp
//  IllusStudioFramework — pre-draw overrides (snapshot at beginStroke)
//

#pragma once

#include "BrushPreset.hpp"

#include <optional>

namespace illus {

struct BrushSession {
    int32_t presetId = 0;
    std::optional<float> lineWidthPx;
    std::optional<float> lineSmooth;
    std::optional<float> hardness;
    std::optional<float> opacity;
    std::optional<float> flow;
    std::optional<float> spacing;
    std::optional<float> sizePressure;
    std::optional<float> opacityPressure;
    std::optional<math::RGBA> color;

    void clearOverrides() {
        lineWidthPx.reset();
        lineSmooth.reset();
        hardness.reset();
        opacity.reset();
        flow.reset();
        spacing.reset();
        sizePressure.reset();
        opacityPressure.reset();
        color.reset();
    }

    BrushPreset resolve(const BrushPreset& base) const {
        BrushPreset out = base;
        if (lineWidthPx) out.lineWidthPx = *lineWidthPx;
        if (lineSmooth) out.lineSmooth = *lineSmooth;
        if (hardness) out.hardness = *hardness;
        if (opacity) out.opacity = *opacity;
        if (flow) out.flow = *flow;
        if (spacing) out.spacing = *spacing;
        if (sizePressure) out.sizePressure = *sizePressure;
        if (opacityPressure) out.opacityPressure = *opacityPressure;
        if (color) out.color = *color;
        return out;
    }
};

} // namespace illus
