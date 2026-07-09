//
//  TileGrid.hpp
//  IllusStudioFramework — dirty tile tracking (T1-2-3)
//

#pragma once

#include "Rect.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace illus::math {

/// Fixed 32×32 tiles over the page. Union of dirty tiles → upload/compute rect.
struct TileGrid {
    static constexpr int32_t kTile = 32;

    std::unordered_set<uint32_t> dirty;

    static uint32_t pack(int32_t tx, int32_t ty) {
        return (static_cast<uint32_t>(ty) << 16) | static_cast<uint32_t>(tx & 0xffff);
    }

    void clear() { dirty.clear(); }
    bool empty() const { return dirty.empty(); }

    void markRect(Rect r, int32_t pageW, int32_t pageH) {
        r.clipTo(pageW, pageH);
        if (r.empty()) return;
        const int32_t x0 = r.x / kTile;
        const int32_t y0 = r.y / kTile;
        const int32_t x1 = (r.x + r.w - 1) / kTile;
        const int32_t y1 = (r.y + r.h - 1) / kTile;
        for (int32_t ty = y0; ty <= y1; ++ty) {
            for (int32_t tx = x0; tx <= x1; ++tx) {
                dirty.insert(pack(tx, ty));
            }
        }
    }

    /// Bounding rect of all dirty tiles (clipped to page).
    Rect bounds(int32_t pageW, int32_t pageH) const {
        if (dirty.empty()) return {};
        int32_t minX = pageW, minY = pageH, maxX = 0, maxY = 0;
        for (uint32_t key : dirty) {
            const int32_t tx = static_cast<int32_t>(key & 0xffff);
            const int32_t ty = static_cast<int32_t>(key >> 16);
            minX = std::min(minX, tx * kTile);
            minY = std::min(minY, ty * kTile);
            maxX = std::max(maxX, std::min(pageW, (tx + 1) * kTile) - 1);
            maxY = std::max(maxY, std::min(pageH, (ty + 1) * kTile) - 1);
        }
        if (minX > maxX || minY > maxY) return {};
        Rect out{};
        out.unionWith(minX, minY, maxX, maxY);
        out.clipTo(pageW, pageH);
        return out;
    }
};

} // namespace illus::math
