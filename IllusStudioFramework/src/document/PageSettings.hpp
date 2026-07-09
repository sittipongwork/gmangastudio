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
    math::RGBA background{255, 255, 255, 255};
};

} // namespace illus
