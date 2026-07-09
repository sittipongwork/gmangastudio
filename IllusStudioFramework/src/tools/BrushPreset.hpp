//
//  BrushPreset.hpp
//  IllusStudioFramework — library entry (defaults)
//

#pragma once

#include "../math/Blend.hpp"

#include <cstdint>
#include <string>

namespace illus {

enum class BrushMode : int32_t {
    Paint = 0,
    Erase = 1,
};

enum class BrushSource : int32_t {
    BuiltIn = 0,
    User = 1,
};

struct BrushPreset {
    int32_t id = 0;
    std::string name;
    BrushMode mode = BrushMode::Paint;
    BrushSource source = BrushSource::BuiltIn;

    float lineWidthPx = 16.f;
    float lineSmooth = 0.f;   // 0..1 input EMA
    float hardness = 0.8f;    // 0..1
    float opacity = 1.f;      // 0..1
    float flow = 1.f;         // 0..1
    float spacing = 0.25f;    // fraction of effective width
    float sizePressure = 1.f; // 0..1
    float opacityPressure = 0.f;
    math::RGBA color{20, 20, 20, 255};
};

} // namespace illus
