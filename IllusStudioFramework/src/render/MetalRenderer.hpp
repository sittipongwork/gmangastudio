//
//  MetalRenderer.hpp
//  IllusStudioFramework — P5 present texture (metal-cpp)
//

#pragma once

#include "../math/Rect.hpp"

#include <cstdint>
#include <memory>

namespace MTL {
class Device;
class Texture;
class CommandQueue;
}

namespace illus {

class MetalRenderer {
public:
    MetalRenderer();
    ~MetalRenderer();

    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;

    bool valid() const { return texture_ != nullptr; }
    bool hasDevice() const { return device_ != nullptr; }

    bool ensureSize(int32_t width, int32_t height);

    /// Upload RGBA8 composite (full or dirty rect). `pixels` is full-frame buffer.
    void upload(const uint8_t* pixels, int32_t width, int32_t height, math::Rect dirty);

    /// Underlying ObjC MTLTexture* (unretained). Null if unavailable.
    void* textureHandle() const;
    /// Underlying ObjC MTLDevice* (unretained). Null if unavailable.
    void* deviceHandle() const;

    MTL::Device* device() const { return device_; }
    MTL::CommandQueue* commandQueue() const { return queue_; }

    static bool selfCheck();

private:
    MTL::Device* device_ = nullptr;
    MTL::CommandQueue* queue_ = nullptr;
    MTL::Texture* texture_ = nullptr;
    int32_t width_ = 0;
    int32_t height_ = 0;
};

} // namespace illus
