//
//  StrokeSample.hpp
//  IllusStudioFramework
//

#pragma once

namespace illus {

struct StrokeSample {
    float x = 0;
    float y = 0;
    float pressure = 1;
    float tiltX = 0;
    float tiltY = 0;
    float t = 0; // relative time / order
};

} // namespace illus
