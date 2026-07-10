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
    ImportedProcreate = 2,
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
    float sizePressure = 1.f; // 0..1 how much pressure affects size
    float opacityPressure = 0.f;
    /// Size at pressure 0 as fraction of lineWidth (Procreate minSize).
    float minSize = 0.f; // 0..1
    /// Stroke-start taper amount (Procreate taperSize / pencilTaperSize).
    float taperSize = 0.f; // 0..1
    float taperOpacity = 0.f; // 0..1
    float taperPressure = 0.f; // 0..1 blend taper with pressure
    float angleDeg = 0.f;
    float roundness = 1.f; // 0..1 (1 = circle)
    /// Rotate tip with stroke tangent (Procreate "orient to stroke").
    bool orientTip = false;
    /// Force tip luminance invert (Procreate shapeInverted).
    bool shapeInverted = false;
    /// Grain tiling scale (Procreate textureScale).
    float grainScale = 1.f;
    /// How hard grain punches holes (Procreate grainDepth), 0..1.
    float grainDepth = 1.f;
    math::RGBA color{20, 20, 20, 255};

    /// BrushAssetStore ids; 0 / negative = none (procedural round dab).
    int32_t tipTextureId = -1;
    int32_t grainTextureId = -1;
    int32_t previewTextureId = -1;
    /// True when import mapped few keys / used defaults (UI badge).
    bool approximated = false;
};

} // namespace illus
