//
//  Stroke.hpp
//  IllusStudioFramework — vector stroke (source of truth)
//

#pragma once

#include "StrokeSample.hpp"
#include "../tools/BrushPreset.hpp"

#include <cstdint>
#include <vector>

namespace illus {

struct StrokeBounds {
    float minX = 0;
    float minY = 0;
    float maxX = 0;
    float maxY = 0;
    bool empty = true;

    void expand(float x, float y, float pad) {
        if (empty) {
            minX = x - pad;
            minY = y - pad;
            maxX = x + pad;
            maxY = y + pad;
            empty = false;
            return;
        }
        if (x - pad < minX) minX = x - pad;
        if (y - pad < minY) minY = y - pad;
        if (x + pad > maxX) maxX = x + pad;
        if (y + pad > maxY) maxY = y + pad;
    }

    void reset() {
        empty = true;
        minX = minY = maxX = maxY = 0;
    }
};

struct Stroke {
    int32_t id = 0;
    int32_t layerId = 0;
    BrushPreset presetSnapshot{};
    std::vector<StrokeSample> samples;
    StrokeBounds bounds;
};

struct LayerStrokeList {
    int32_t layerId = 0;
    std::vector<Stroke> strokes;

    void clear() { strokes.clear(); }

    Stroke* find(int32_t strokeId) {
        for (auto& s : strokes) {
            if (s.id == strokeId) return &s;
        }
        return nullptr;
    }

    const Stroke* find(int32_t strokeId) const {
        for (const auto& s : strokes) {
            if (s.id == strokeId) return &s;
        }
        return nullptr;
    }
};

} // namespace illus
