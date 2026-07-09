//
//  LayerStack.cpp
//  IllusStudioFramework
//

#include "LayerStack.hpp"

#include "../math/Blend.hpp"
#include "../math/Rect.hpp"

#include <algorithm>
#include <utility>

namespace illus {

LayerStack::LayerStack(int32_t width, int32_t height)
    : width_(std::max(1, width))
    , height_(std::max(1, height))
{
    activeId_ = add("Layer 1");
}

void LayerStack::resizeAll(int32_t width, int32_t height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    for (auto& layer : layers_) {
        if (layer.hasPixels()) {
            layer.ensurePixels(width_, height_);
        }
    }
}

int32_t LayerStack::add(const char* name) {
    Layer layer;
    layer.id = nextId_++;
    layer.name = name ? name : "Layer";
    // Lazy pixels: allocate on first stroke (cpp_optimise).
    layers_.insert(layers_.begin(), std::move(layer)); // front
    activeId_ = layers_.front().id;
    return activeId_;
}

bool LayerStack::remove(int32_t layerId) {
    if (layers_.size() <= 1) return false;
    const auto it = std::find_if(layers_.begin(), layers_.end(), [&](const Layer& l) {
        return l.id == layerId;
    });
    if (it == layers_.end()) return false;
    const bool wasActive = it->id == activeId_;
    layers_.erase(it);
    if (wasActive) {
        activeId_ = layers_.front().id;
    }
    return true;
}

std::optional<int32_t> LayerStack::duplicate(int32_t layerId) {
    const Layer* src = find(layerId);
    if (!src) return std::nullopt;
    Layer copy = *src;
    copy.id = nextId_++;
    copy.name = src->name + " copy";
    const int32_t idx = indexOf(layerId);
    layers_.insert(layers_.begin() + idx, std::move(copy));
    activeId_ = layers_[static_cast<size_t>(idx)].id;
    return activeId_;
}

bool LayerStack::move(int32_t layerId, int32_t toIndex) {
    const int32_t from = indexOf(layerId);
    if (from < 0) return false;
    toIndex = std::clamp(toIndex, 0, count() - 1);
    if (from == toIndex) return true;
    Layer layer = std::move(layers_[static_cast<size_t>(from)]);
    layers_.erase(layers_.begin() + from);
    layers_.insert(layers_.begin() + toIndex, std::move(layer));
    return true;
}

bool LayerStack::mergeDown(int32_t srcId, int32_t dstId) {
    if (srcId == dstId || layers_.size() <= 1) return false;
    Layer* src = find(srcId);
    Layer* dst = find(dstId);
    if (!src || !dst) return false;

    if (src->hasPixels() && src->visible && src->opacity > 0.f) {
        dst->ensurePixels(width_, height_);
        if (src->pixels.size() == dst->pixels.size()) {
            math::Rect full{0, 0, width_, height_};
            const float op = std::clamp(src->opacity, 0.f, 1.f);
            math::blendLayerRect(dst->pixels.data(), src->pixels.data(), width_, full, op);
        }
    }
    // Keep dst active if we remove src.
    const bool ok = remove(srcId);
    if (ok) activeId_ = dstId;
    return ok;
}

bool LayerStack::setActive(int32_t layerId) {
    if (!find(layerId)) return false;
    activeId_ = layerId;
    return true;
}

bool LayerStack::setOpacity(int32_t layerId, float opacity) {
    Layer* layer = find(layerId);
    if (!layer) return false;
    layer->opacity = std::clamp(opacity, 0.f, 1.f);
    return true;
}

bool LayerStack::setVisible(int32_t layerId, bool visible) {
    Layer* layer = find(layerId);
    if (!layer) return false;
    layer->visible = visible;
    return true;
}

float LayerStack::opacity(int32_t layerId) const {
    const Layer* layer = find(layerId);
    return layer ? layer->opacity : 0.f;
}

bool LayerStack::visible(int32_t layerId) const {
    const Layer* layer = find(layerId);
    return layer ? layer->visible : false;
}

Layer* LayerStack::active() {
    return find(activeId_);
}

const Layer* LayerStack::active() const {
    return find(activeId_);
}

Layer* LayerStack::find(int32_t layerId) {
    for (auto& layer : layers_) {
        if (layer.id == layerId) return &layer;
    }
    return nullptr;
}

const Layer* LayerStack::find(int32_t layerId) const {
    for (const auto& layer : layers_) {
        if (layer.id == layerId) return &layer;
    }
    return nullptr;
}

Layer* LayerStack::at(int32_t index) {
    if (index < 0 || index >= count()) return nullptr;
    return &layers_[static_cast<size_t>(index)];
}

const Layer* LayerStack::at(int32_t index) const {
    if (index < 0 || index >= count()) return nullptr;
    return &layers_[static_cast<size_t>(index)];
}

int32_t LayerStack::indexOf(int32_t layerId) const {
    for (int32_t i = 0; i < count(); ++i) {
        if (layers_[static_cast<size_t>(i)].id == layerId) return i;
    }
    return -1;
}

} // namespace illus
