//
//  SoftwareRenderer.hpp
//  IllusStudioFramework — CPU composite
//

#pragma once

#include "../document/PageSettings.hpp"
#include "../layers/LayerStack.hpp"
#include "../math/Rect.hpp"

#include <cstdint>
#include <vector>

namespace illus {

class SoftwareRenderer {
public:
    /// Full composite: background + all visible layers (bottom→top).
    static void composite(const PageSettings& page, const LayerStack& stack, std::vector<uint8_t>& out);

    /// Composite only `rect` into existing `out` (same size as page).
    static void compositeRect(const PageSettings& page, const LayerStack& stack, std::vector<uint8_t>& out, math::Rect rect);

    /// Background + layers below `activeIndex` (index 0 = front). Writes full buffer.
    static void compositeBelow(const PageSettings& page, const LayerStack& stack, int32_t activeIndex, std::vector<uint8_t>& out);

    /// Copy below into out for `rect`, then blend active layer + layers above it in that rect.
    static void compositeFromBelow(
        const PageSettings& page,
        const LayerStack& stack,
        int32_t activeIndex,
        const std::vector<uint8_t>& below,
        std::vector<uint8_t>& out,
        math::Rect rect
    );

    static bool selfCheck();
};

} // namespace illus
