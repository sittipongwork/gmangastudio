//
//  Layer.hpp
//  IllusStudioFramework
//

#pragma once

#include "../math/Blend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace illus {

enum class BlendMode : int32_t {
    Normal = 0,
};

struct Layer {
    int32_t id = 0;
    std::string name;
    bool visible = true;
    float opacity = 1.f; // 0..1
    BlendMode blend = BlendMode::Normal;
    /// Bottom page layer created with the document; not removable.
    bool isBackground = false;
    /// Empty until first paint — saves ~W*H*4 per unused layer (cpp_optimise).
    std::vector<uint8_t> pixels;

    bool hasPixels() const { return !pixels.empty(); }
    void ensurePixels(int32_t width, int32_t height);
    void releasePixels(); // free heap (clear + shrink)
    void clearTransparent();
    void fillSolid(int32_t width, int32_t height, math::RGBA c);
    uint8_t* pixelAt(int32_t x, int32_t y, int32_t width);
    const uint8_t* pixelAt(int32_t x, int32_t y, int32_t width) const;
};

} // namespace illus
