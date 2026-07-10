//
//  BrushPreset.hpp
//  IllusStudioFramework — library entry (BrushModel v2 fields on one struct)
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
    float spacing = 0.25f;    // fraction of effective diameter
    float sizePressure = 1.f; // 0..1 how much pressure affects size
    float opacityPressure = 0.f;
    /// Size at pressure 0 as fraction of lineWidth (Procreate minSize).
    float minSize = 0.f; // 0..1
    /// Stroke-start taper amount (Procreate taperSize / pencilTaperSize).
    float taperSize = 0.f; // 0..1
    /// Stroke-end taper (Procreate taper end); applied when totalLen known.
    float taperEndSize = 0.f; // 0..1
    float taperOpacity = 0.f; // 0..1
    float taperPressure = 0.f; // 0..1 blend taper with pressure
    float angleDeg = 0.f;
    float roundness = 1.f; // 0..1 (1 = circle; <1 = flatter ellipse)
    /// Rotate tip with stroke tangent (Procreate "orient to stroke").
    bool orientTip = false;
    /// Force tip luminance invert (Procreate shapeInverted).
    bool shapeInverted = false;

    /// Shape scatter 0..1 (position/angle jitter per dab). Cap applied in stamp.
    float scatter = 0.f;
    /// Stamps per spacing step (Procreate shape count); clamped 1..4.
    int32_t stampCount = 1;
    /// Extra rotation jitter degrees scale 0..1.
    float rotationJitter = 0.f;

    /// Grain tiling scale (Procreate textureScale).
    float grainScale = 1.f;
    /// How hard grain punches holes (Procreate grainDepth), 0..1.
    float grainDepth = 1.f;
    /// true = Moving (rolls with stroke); false = Texturized (canvas-locked).
    bool grainMoving = false;

    /// Speed dynamics 0..1 (faster → thinner / more spacing).
    float speedSize = 0.f;
    float speedOpacity = 0.f;
    float speedSpacing = 0.f;
    /// Tilt → size/opacity (Apple Pencil); 0 = off.
    float tiltSize = 0.f;
    float tiltOpacity = 0.f;

    /// Pressure curve endpoints (Procreate); used when sizeCurveValid.
    float sizeCurveY0 = 0.f;
    float sizeCurveY1 = 1.f;
    bool sizeCurveValid = false;
    float opacityCurveY0 = 0.f;
    float opacityCurveY1 = 1.f;
    bool opacityCurveValid = false;

    math::RGBA color{20, 20, 20, 255};

    /// BrushAssetStore ids; negative = none (procedural round dab).
    int32_t tipTextureId = -1;
    int32_t grainTextureId = -1;
    int32_t previewTextureId = -1;
    /// True when import mapped few keys / used defaults (UI badge).
    bool approximated = false;
};

} // namespace illus
