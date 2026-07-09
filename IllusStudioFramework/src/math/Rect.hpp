//
//  Rect.hpp
//  IllusStudioFramework
//

#pragma once

#include <algorithm>
#include <cstdint>

namespace illus::math {

struct Rect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;

    bool empty() const { return w <= 0 || h <= 0; }

    void setEmpty() { x = y = w = h = 0; }

    void unionWith(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
        if (x1 < x0) std::swap(x0, x1);
        if (y1 < y0) std::swap(y0, y1);
        const int32_t rw = x1 - x0 + 1;
        const int32_t rh = y1 - y0 + 1;
        if (rw <= 0 || rh <= 0) return;
        if (empty()) {
            x = x0;
            y = y0;
            w = rw;
            h = rh;
            return;
        }
        const int32_t x2 = std::min(x, x0);
        const int32_t y2 = std::min(y, y0);
        const int32_t r = std::max(x + w, x0 + rw);
        const int32_t b = std::max(y + h, y0 + rh);
        x = x2;
        y = y2;
        w = r - x2;
        h = b - y2;
    }

    void clipTo(int32_t width, int32_t height) {
        if (empty()) return;
        const int32_t x1 = std::min(x + w, width);
        const int32_t y1 = std::min(y + h, height);
        x = std::max(0, x);
        y = std::max(0, y);
        w = std::max(0, x1 - x);
        h = std::max(0, y1 - y);
    }
};

} // namespace illus::math
