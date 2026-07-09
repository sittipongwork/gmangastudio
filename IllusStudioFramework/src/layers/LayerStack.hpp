//
//  LayerStack.hpp
//  IllusStudioFramework — index 0 = front (top of panel)
//

#pragma once

#include "Layer.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace illus {

class LayerStack {
public:
    explicit LayerStack(int32_t width, int32_t height);

    void resizeAll(int32_t width, int32_t height);

    /// Inserts at front (top of panel). Null/empty/"Layer" → auto-name `Layer N`.
    int32_t add(const char* name = nullptr);
    bool remove(int32_t layerId);
    std::optional<int32_t> duplicate(int32_t layerId);
    bool move(int32_t layerId, int32_t toIndex); // 0 = front
    /// Merge `srcId` into `dstId` (src-over), then remove src. Returns false if invalid.
    bool mergeDown(int32_t srcId, int32_t dstId);

    bool setActive(int32_t layerId);
    bool setOpacity(int32_t layerId, float opacity);
    bool setVisible(int32_t layerId, bool visible);
    float opacity(int32_t layerId) const;
    bool visible(int32_t layerId) const;
    int32_t activeId() const { return activeId_; }
    Layer* active();
    const Layer* active() const;

    Layer* find(int32_t layerId);
    const Layer* find(int32_t layerId) const;

    int32_t count() const { return static_cast<int32_t>(layers_.size()); }
    Layer* at(int32_t index); // 0 = front
    const Layer* at(int32_t index) const;
    int32_t indexOf(int32_t layerId) const;

    const std::vector<Layer>& layers() const { return layers_; }

private:
    int32_t nextId_ = 1;
    int32_t nextPaintLayerNumber_ = 1;
    int32_t activeId_ = 0;
    int32_t width_ = 1;
    int32_t height_ = 1;
    std::vector<Layer> layers_;
};

} // namespace illus
