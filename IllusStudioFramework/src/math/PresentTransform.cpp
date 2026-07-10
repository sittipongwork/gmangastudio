//
//  PresentTransform.cpp
//  IllusStudioFramework — present transforms (TX-7-2)
//

#include "PresentTransform.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <cmath>
#include <cstring>

namespace illus {

void presentModelMatrix(
    const Viewport& vp,
    float canvasW,
    float canvasH,
    float viewW,
    float viewH,
    float out16[16]
) {
    // GLM kept for rotate / MVP consumers — not on the 120Hz present path.
    const float s = vp.effectiveScale(viewW, viewH, canvasW, canvasH);
    float ox = 0.f, oy = 0.f;
    vp.viewOrigin(viewW, viewH, canvasW, canvasH, ox, oy);

    glm::mat4 m(1.f);
    m = glm::translate(m, glm::vec3(ox, oy, 0.f));
    m = glm::scale(m, glm::vec3(s, s, 1.f));
    std::memcpy(out16, glm::value_ptr(m), sizeof(float) * 16);
}

void presentNdcRect(
    const Viewport& vp,
    float canvasW,
    float canvasH,
    float viewW,
    float viewH,
    float out[4]
) {
    // ponytail: scalar only — GLM mat4 here cost ~+CPU at 120Hz for axis-aligned fit.
    // Upgrade: use presentModelMatrix when canvas rotate lands.
    const float s = vp.effectiveScale(viewW, viewH, canvasW, canvasH);
    float ox = 0.f, oy = 0.f;
    vp.viewOrigin(viewW, viewH, canvasW, canvasH, ox, oy);
    const float drawnW = canvasW * s;
    const float drawnH = canvasH * s;

    out[0] = -1.f + 2.f * ox / viewW;
    out[1] = 1.f - 2.f * (oy + drawnH) / viewH;
    out[2] = -1.f + 2.f * (ox + drawnW) / viewW;
    out[3] = 1.f - 2.f * oy / viewH;
}

bool presentTransformSelfCheck() {
    Viewport vp;
    vp.scale = 1.f;
    vp.offsetX = 0.f;
    vp.offsetY = 0.f;
    float out[4];
    presentNdcRect(vp, 100.f, 50.f, 200.f, 100.f, out);
    // Fit scale = min(2,2)=2; drawn 200×100 fills view → NDC full [-1,1].
    if (std::abs(out[0] - (-1.f)) > 1e-3f) return false;
    if (std::abs(out[2] - 1.f) > 1e-3f) return false;
    if (std::abs(out[3] - 1.f) > 1e-3f) return false;
    if (std::abs(out[1] - (-1.f)) > 1e-3f) return false;

    float m16[16];
    presentModelMatrix(vp, 100.f, 50.f, 200.f, 100.f, m16);
    if (std::abs(m16[0] - 2.f) > 1e-3f) return false;
    return true;
}

} // namespace illus
