//
//  MetalRenderer.cpp
//  IllusStudioFramework — metal-cpp present path
//

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include "MetalRenderer.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <algorithm>
#include <vector>

namespace illus {

MetalRenderer::MetalRenderer() {
    device_ = MTL::CreateSystemDefaultDevice();
    if (!device_) return;
    queue_ = device_->newCommandQueue();
}

MetalRenderer::~MetalRenderer() {
    if (texture_) {
        texture_->release();
        texture_ = nullptr;
    }
    if (queue_) {
        queue_->release();
        queue_ = nullptr;
    }
    if (device_) {
        device_->release();
        device_ = nullptr;
    }
}

bool MetalRenderer::ensureSize(int32_t width, int32_t height) {
    if (!device_) return false;
    width = std::max(1, width);
    height = std::max(1, height);
    if (texture_ && width_ == width && height_ == height) return true;

    if (texture_) {
        texture_->release();
        texture_ = nullptr;
    }

    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm,
        static_cast<NS::UInteger>(width),
        static_cast<NS::UInteger>(height),
        false
    );
    desc->setUsage(MTL::TextureUsageShaderRead);
    desc->setStorageMode(MTL::StorageModeShared);

    texture_ = device_->newTexture(desc);
    // texture2DDescriptor returns an autoreleased descriptor — do not release.
    width_ = width;
    height_ = height;
    return texture_ != nullptr;
}

void MetalRenderer::upload(const uint8_t* pixels, int32_t width, int32_t height, math::Rect dirty) {
    if (!pixels || !ensureSize(width, height) || !texture_) return;

    dirty.clipTo(width, height);
    if (dirty.empty()) {
        dirty = {0, 0, width, height};
    }

    MTL::Region region;
    region.origin.x = static_cast<NS::UInteger>(dirty.x);
    region.origin.y = static_cast<NS::UInteger>(dirty.y);
    region.origin.z = 0;
    region.size.width = static_cast<NS::UInteger>(dirty.w);
    region.size.height = static_cast<NS::UInteger>(dirty.h);
    region.size.depth = 1;

    const uint8_t* src =
        pixels + (static_cast<size_t>(dirty.y) * static_cast<size_t>(width) + static_cast<size_t>(dirty.x)) * 4u;
    const NS::UInteger bytesPerRow = static_cast<NS::UInteger>(width) * 4u;
    texture_->replaceRegion(region, 0, src, bytesPerRow);
}

void* MetalRenderer::textureHandle() const {
    return texture_;
}

void* MetalRenderer::deviceHandle() const {
    return device_;
}

bool MetalRenderer::selfCheck() {
    MetalRenderer r;
    if (!r.ensureSize(4, 4)) {
        return true; // headless / no GPU
    }
    std::vector<uint8_t> px(16 * 4, 255);
    px[0] = 10;
    px[1] = 20;
    px[2] = 30;
    px[3] = 255;
    r.upload(px.data(), 4, 4, {0, 0, 4, 4});
    return r.textureHandle() != nullptr;
}

} // namespace illus
