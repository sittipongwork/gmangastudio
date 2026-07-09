//
//  PageSettings.hpp
//  IllusStudioFramework
//

#pragma once

#include "../math/Blend.hpp"

#include <cstdint>

namespace illus {

struct PageSettings {
    int32_t width = 1;
    int32_t height = 1;
    /// Under-layer clear (not the white page). White lives on Background Layer.
    math::RGBA background{0, 0, 0, 0};
};

} // namespace illus
