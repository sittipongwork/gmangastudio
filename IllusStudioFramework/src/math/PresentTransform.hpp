//
//  PresentTransform.hpp
//  IllusStudioFramework — present transforms (TX-7): scalar NDC + GLM model matrix
//

#pragma once

#include "../viewport/Viewport.hpp"

#include <cstdint>

namespace illus {

/// Canvas→NDC axis-aligned quad for Metal present (y-up NDC, view y-down).
/// Writes xmin, ymin, xmax, ymax into out[4]. Scalar (no GLM) — safe for 120Hz.
void presentNdcRect(
    const Viewport& vp,
    float canvasW,
    float canvasH,
    float viewW,
    float viewH,
    float out[4]
);

/// Column-major float[16] model matrix: canvas pixel → view pixel (pre-NDC).
/// Uses GLM — for rotate / MVP consumers; not the 120Hz axis-aligned path.
void presentModelMatrix(
    const Viewport& vp,
    float canvasW,
    float canvasH,
    float viewW,
    float viewH,
    float out16[16]
);

bool presentTransformSelfCheck();

} // namespace illus
