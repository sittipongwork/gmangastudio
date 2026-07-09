//
//  LayerCompositor.hpp
//  IllusStudioFramework — per-layer MTLTexture + GPU Normal blend (T1-4)
//

#pragma once

#include "../layers/LayerStack.hpp"
#include "../math/Rect.hpp"

#include <cstdint>
#include <unordered_map>

namespace MTL {
class Device;
class Texture;
class CommandQueue;
class RenderPipelineState;
class ComputePipelineState;
class SamplerState;
class Buffer;
}

namespace illus {

/// Owns layer textures, live overlay texture, and present target. Normal blend only (T0-12 later).
class LayerCompositor {
public:
    LayerCompositor() = default;
    ~LayerCompositor();

    LayerCompositor(const LayerCompositor&) = delete;
    LayerCompositor& operator=(const LayerCompositor&) = delete;

    bool init(MTL::Device* device, MTL::CommandQueue* queue);
    bool ready() const { return pipeline_ != nullptr; }

    bool ensureSize(int32_t width, int32_t height);

    void uploadLayer(int32_t layerId, const uint8_t* pixels, int32_t width, int32_t height, math::Rect dirty);
    void clearLayer(int32_t layerId);
    void removeLayer(int32_t layerId);

    MTL::Texture* overlayTexture() { return overlay_; }
    void clearOverlay();
    void uploadOverlay(const uint8_t* pixels, int32_t width, int32_t height, math::Rect dirty);

    /// Blend stack bottom→top into present. Overlay drawn above active layer when `showOverlay`.
    void* present(const LayerStack& stack, int32_t activeLayerId, bool showOverlay);

    void* presentTexture() const;
    MTL::Device* device() const { return device_; }
    MTL::CommandQueue* queue() const { return queue_; }

    MTL::ComputePipelineState* dabPipeline() const { return dabPipeline_; }

    static bool selfCheck(MTL::Device* device, MTL::CommandQueue* queue);

private:
    MTL::Texture* ensureLayerTexture(int32_t layerId);
    MTL::Texture* makeTexture(bool shaderWrite) const;

    MTL::Device* device_ = nullptr;       // not owned
    MTL::CommandQueue* queue_ = nullptr;  // not owned
    MTL::RenderPipelineState* pipeline_ = nullptr;
    MTL::ComputePipelineState* dabPipeline_ = nullptr;
    MTL::SamplerState* sampler_ = nullptr;
    MTL::Buffer* compositeUniformBuf_ = nullptr;

    MTL::Texture* present_ = nullptr;
    MTL::Texture* overlay_ = nullptr;
    std::unordered_map<int32_t, MTL::Texture*> layers_;

    int32_t width_ = 0;
    int32_t height_ = 0;
};

} // namespace illus
