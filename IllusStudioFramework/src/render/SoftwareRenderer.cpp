//
//  SoftwareRenderer.cpp
//  IllusStudioFramework
//

#include "SoftwareRenderer.hpp"

#include "../math/Blend.hpp"

#include <algorithm>

namespace illus {
namespace {

void blendStackRect(
    const PageSettings& page,
    const LayerStack& stack,
    std::vector<uint8_t>& out,
    math::Rect rect,
    int32_t fromBackIndexInclusive,
    int32_t toFrontIndexInclusive
) {
    // layers_: index 0 = front, count-1 = back.
    // Iterate back → front: i from count-1 down to 0.
    const int32_t count = stack.count();
    for (int32_t i = count - 1; i >= 0; --i) {
        if (i < toFrontIndexInclusive || i > fromBackIndexInclusive) continue;
        const Layer* layer = stack.at(i);
        if (!layer || !layer->visible || layer->opacity <= 0.f) continue;
        if (!layer->hasPixels()) continue; // lazy / cleared — fully transparent
        if (layer->pixels.size() != out.size()) continue;
        math::blendLayerRect(out.data(), layer->pixels.data(), page.width, rect, layer->opacity);
    }
}

} // namespace

namespace {

void ensureBuffer(std::vector<uint8_t>& out, size_t bytes) {
    if (out.size() != bytes) {
        out.assign(bytes, 0);
    }
}

} // namespace

void SoftwareRenderer::composite(const PageSettings& page, const LayerStack& stack, std::vector<uint8_t>& out) {
    const size_t n = static_cast<size_t>(page.width) * static_cast<size_t>(page.height);
    ensureBuffer(out, n * 4u);
    math::Rect full{0, 0, page.width, page.height};
    math::fillRGBA(out.data(), n, page.background);
    blendStackRect(page, stack, out, full, stack.count() - 1, 0);
}

void SoftwareRenderer::compositeRect(const PageSettings& page, const LayerStack& stack, std::vector<uint8_t>& out, math::Rect rect) {
    const size_t n = static_cast<size_t>(page.width) * static_cast<size_t>(page.height);
    ensureBuffer(out, n * 4u);
    rect.clipTo(page.width, page.height);
    if (rect.empty()) return;
    math::fillRect(out.data(), page.width, rect, page.background);
    blendStackRect(page, stack, out, rect, stack.count() - 1, 0);
}

void SoftwareRenderer::compositeBelow(
    const PageSettings& page,
    const LayerStack& stack,
    int32_t activeIndex,
    std::vector<uint8_t>& out
) {
    const size_t n = static_cast<size_t>(page.width) * static_cast<size_t>(page.height);
    ensureBuffer(out, n * 4u);
    math::fillRGBA(out.data(), n, page.background);
    // Layers strictly behind active: indices activeIndex+1 .. count-1
    if (activeIndex < 0) activeIndex = 0;
    blendStackRect(page, stack, out, {0, 0, page.width, page.height}, stack.count() - 1, activeIndex + 1);
}

void SoftwareRenderer::compositeFromBelow(
    const PageSettings& page,
    const LayerStack& stack,
    int32_t activeIndex,
    const std::vector<uint8_t>& below,
    std::vector<uint8_t>& out,
    math::Rect rect
) {
    const size_t n = static_cast<size_t>(page.width) * static_cast<size_t>(page.height);
    ensureBuffer(out, n * 4u);
    if (below.size() != out.size()) {
        compositeRect(page, stack, out, rect);
        return;
    }
    rect.clipTo(page.width, page.height);
    if (rect.empty()) return;

    math::copyRect(out.data(), below.data(), page.width, rect);

    // Active + layers above (front): indices activeIndex .. 0
    if (activeIndex < 0) activeIndex = 0;
    blendStackRect(page, stack, out, rect, activeIndex, 0);
}

bool SoftwareRenderer::selfCheck() {
    PageSettings page{4, 4, {255, 255, 255, 255}};
    LayerStack stack(4, 4);
    const int32_t topId = stack.activeId();
    stack.add("Top");
    stack.setActive(topId);
    Layer* back = stack.at(1);
    if (!back) return false;
    back->ensurePixels(4, 4);
    for (size_t i = 0; i < back->pixels.size(); i += 4) {
        back->pixels[i] = 255;
        back->pixels[i + 1] = 0;
        back->pixels[i + 2] = 0;
        back->pixels[i + 3] = 255;
    }
    Layer* front = stack.at(0);
    if (!front) return false;
    front->ensurePixels(4, 4);
    uint8_t* p = front->pixelAt(2, 2, 4);
    p[0] = 0;
    p[1] = 0;
    p[2] = 255;
    p[3] = 128;

    std::vector<uint8_t> out;
    composite(page, stack, out);
    const uint8_t* c = &out[(2u * 4u + 2u) * 4u];
    if ((c[0] == 255 && c[1] == 0 && c[2] == 0) || (c[0] == 255 && c[1] == 255 && c[2] == 255)) {
        return false;
    }

    // Dirty-rect path must match full composite at that pixel.
    std::vector<uint8_t> out2(out.size(), 0);
    compositeRect(page, stack, out2, {2, 2, 1, 1});
    const uint8_t* c2 = &out2[(2u * 4u + 2u) * 4u];
    return c[0] == c2[0] && c[1] == c2[1] && c[2] == c2[2] && c[3] == c2[3];
}

} // namespace illus
