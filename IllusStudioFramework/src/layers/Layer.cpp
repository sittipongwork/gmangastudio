//
//  Layer.cpp
//  IllusStudioFramework
//

#include "Layer.hpp"

#include <algorithm>

namespace illus {

void Layer::ensurePixels(int32_t width, int32_t height) {
    const size_t n = static_cast<size_t>(std::max(1, width)) * static_cast<size_t>(std::max(1, height)) * 4u;
    if (pixels.size() == n) return;
    pixels.assign(n, 0);
}

void Layer::releasePixels() {
    pixels.clear();
    pixels.shrink_to_fit();
}

void Layer::clearTransparent() {
    // ponytail: free the buffer so Clear / empty layers don't keep ~8MB each.
    releasePixels();
}

void Layer::fillSolid(int32_t width, int32_t height, math::RGBA c) {
    ensurePixels(width, height);
    math::fillRGBA(pixels.data(), pixels.size() / 4u, c);
}

uint8_t* Layer::pixelAt(int32_t x, int32_t y, int32_t width) {
    return &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u];
}

const uint8_t* Layer::pixelAt(int32_t x, int32_t y, int32_t width) const {
    return &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u];
}

} // namespace illus
