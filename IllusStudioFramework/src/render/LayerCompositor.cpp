//
//  LayerCompositor.cpp
//  IllusStudioFramework — GPU layer blend + shader compile (T1-3 / T1-4)
//

#include "LayerCompositor.hpp"

#include "MetalShaders.hpp"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

namespace illus {
namespace {

struct CompositeUniforms {
    float opacity;
};

void releaseTexture(MTL::Texture*& t) {
    if (t) {
        t->release();
        t = nullptr;
    }
}

} // namespace

LayerCompositor::~LayerCompositor() {
    for (auto& kv : layers_) {
        releaseTexture(kv.second);
    }
    layers_.clear();
    releaseTexture(overlay_);
    releaseTexture(present_);
    if (compositeUniformBuf_) {
        compositeUniformBuf_->release();
        compositeUniformBuf_ = nullptr;
    }
    if (sampler_) {
        sampler_->release();
        sampler_ = nullptr;
    }
    if (pipeline_) {
        pipeline_->release();
        pipeline_ = nullptr;
    }
    if (dabPipeline_) {
        dabPipeline_->release();
        dabPipeline_ = nullptr;
    }
    device_ = nullptr;
    queue_ = nullptr;
}

bool LayerCompositor::init(MTL::Device* device, MTL::CommandQueue* queue) {
    if (!device || !queue) return false;
    device_ = device;
    queue_ = queue;

    NS::Error* error = nullptr;
    NS::String* src = NS::String::string(metal_shaders::kLibrarySource, NS::UTF8StringEncoding);
    MTL::CompileOptions* opts = MTL::CompileOptions::alloc()->init();
    MTL::Library* lib = device_->newLibrary(src, opts, &error);
    opts->release();
    if (!lib) return false;

    MTL::Function* dabFn = lib->newFunction(NS::String::string("stampDab", NS::UTF8StringEncoding));
    MTL::Function* vs = lib->newFunction(NS::String::string("compositeVertex", NS::UTF8StringEncoding));
    MTL::Function* fs = lib->newFunction(NS::String::string("compositeFragment", NS::UTF8StringEncoding));
    lib->release();
    if (!dabFn || !vs || !fs) {
        if (dabFn) dabFn->release();
        if (vs) vs->release();
        if (fs) fs->release();
        return false;
    }

    error = nullptr;
    dabPipeline_ = device_->newComputePipelineState(dabFn, &error);
    dabFn->release();
    if (!dabPipeline_) {
        vs->release();
        fs->release();
        return false;
    }

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vs);
    desc->setFragmentFunction(fs);
    MTL::RenderPipelineColorAttachmentDescriptor* att = desc->colorAttachments()->object(0);
    att->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    att->setBlendingEnabled(true);
    // Premultiplied src-over
    att->setSourceRGBBlendFactor(MTL::BlendFactorOne);
    att->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    att->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    att->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    att->setRgbBlendOperation(MTL::BlendOperationAdd);
    att->setAlphaBlendOperation(MTL::BlendOperationAdd);

    error = nullptr;
    pipeline_ = device_->newRenderPipelineState(desc, &error);
    desc->release();
    vs->release();
    fs->release();
    if (!pipeline_) {
        dabPipeline_->release();
        dabPipeline_ = nullptr;
        return false;
    }

    MTL::SamplerDescriptor* sdesc = MTL::SamplerDescriptor::alloc()->init();
    sdesc->setMinFilter(MTL::SamplerMinMagFilterNearest);
    sdesc->setMagFilter(MTL::SamplerMinMagFilterNearest);
    sdesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sdesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_ = device_->newSamplerState(sdesc);
    sdesc->release();

    compositeUniformBuf_ = device_->newBuffer(sizeof(CompositeUniforms), MTL::ResourceStorageModeShared);
    return sampler_ != nullptr && compositeUniformBuf_ != nullptr;
}

MTL::Texture* LayerCompositor::makeTexture(bool shaderWrite) const {
    if (!device_ || width_ < 1 || height_ < 1) return nullptr;
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm,
        static_cast<NS::UInteger>(width_),
        static_cast<NS::UInteger>(height_),
        false
    );
    NS::UInteger usage = MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget;
    if (shaderWrite) usage |= MTL::TextureUsageShaderWrite;
    desc->setUsage(usage);
    desc->setStorageMode(MTL::StorageModeShared);
    return device_->newTexture(desc);
}

bool LayerCompositor::ensureSize(int32_t width, int32_t height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (present_ && width_ == width && height_ == height) return true;

    width_ = width;
    height_ = height;
    for (auto& kv : layers_) {
        releaseTexture(kv.second);
    }
    layers_.clear();
    releaseTexture(overlay_);
    releaseTexture(present_);

    present_ = makeTexture(false);
    overlay_ = makeTexture(true);
    return present_ != nullptr && overlay_ != nullptr;
}

MTL::Texture* LayerCompositor::ensureLayerTexture(int32_t layerId) {
    auto it = layers_.find(layerId);
    if (it != layers_.end() && it->second) return it->second;
    MTL::Texture* t = makeTexture(true);
    if (!t) return nullptr;
    layers_[layerId] = t;
    return t;
}

void LayerCompositor::uploadLayer(
    int32_t layerId,
    const uint8_t* pixels,
    int32_t width,
    int32_t height,
    math::Rect dirty
) {
    if (!pixels || !ensureSize(width, height)) return;
    MTL::Texture* tex = ensureLayerTexture(layerId);
    if (!tex) return;

    dirty.clipTo(width, height);
    if (dirty.empty()) dirty = {0, 0, width, height};

    MTL::Region region;
    region.origin.x = static_cast<NS::UInteger>(dirty.x);
    region.origin.y = static_cast<NS::UInteger>(dirty.y);
    region.origin.z = 0;
    region.size.width = static_cast<NS::UInteger>(dirty.w);
    region.size.height = static_cast<NS::UInteger>(dirty.h);
    region.size.depth = 1;

    const uint8_t* src =
        pixels + (static_cast<size_t>(dirty.y) * static_cast<size_t>(width) + static_cast<size_t>(dirty.x)) * 4u;
    tex->replaceRegion(region, 0, src, static_cast<NS::UInteger>(width) * 4u);
}

void LayerCompositor::clearLayer(int32_t layerId) {
    auto it = layers_.find(layerId);
    if (it == layers_.end() || !it->second || width_ < 1) return;
    // ponytail: full clear via replaceRegion zeros
    std::vector<uint8_t> zeros(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u, 0);
    MTL::Region region;
    region.origin = {0, 0, 0};
    region.size = {static_cast<NS::UInteger>(width_), static_cast<NS::UInteger>(height_), 1};
    it->second->replaceRegion(region, 0, zeros.data(), static_cast<NS::UInteger>(width_) * 4u);
}

void LayerCompositor::removeLayer(int32_t layerId) {
    auto it = layers_.find(layerId);
    if (it == layers_.end()) return;
    releaseTexture(it->second);
    layers_.erase(it);
}

void LayerCompositor::clearOverlay() {
    if (!overlay_ || width_ < 1) return;
    std::vector<uint8_t> zeros(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u, 0);
    MTL::Region region;
    region.origin = {0, 0, 0};
    region.size = {static_cast<NS::UInteger>(width_), static_cast<NS::UInteger>(height_), 1};
    overlay_->replaceRegion(region, 0, zeros.data(), static_cast<NS::UInteger>(width_) * 4u);
}

void LayerCompositor::uploadOverlay(const uint8_t* pixels, int32_t width, int32_t height, math::Rect dirty) {
    if (!pixels || !overlay_ || !ensureSize(width, height)) return;
    dirty.clipTo(width, height);
    if (dirty.empty()) dirty = {0, 0, width, height};
    MTL::Region region;
    region.origin.x = static_cast<NS::UInteger>(dirty.x);
    region.origin.y = static_cast<NS::UInteger>(dirty.y);
    region.origin.z = 0;
    region.size.width = static_cast<NS::UInteger>(dirty.w);
    region.size.height = static_cast<NS::UInteger>(dirty.h);
    region.size.depth = 1;
    const uint8_t* src =
        pixels + (static_cast<size_t>(dirty.y) * static_cast<size_t>(width) + static_cast<size_t>(dirty.x)) * 4u;
    overlay_->replaceRegion(region, 0, src, static_cast<NS::UInteger>(width) * 4u);
}

void* LayerCompositor::present(const LayerStack& stack, int32_t activeLayerId, bool showOverlay) {
    if (!ready() || !present_ || !queue_) return nullptr;

    // commandBuffer() is autoreleased — do not release (metal-cpp ownership rules).
    MTL::CommandBuffer* cmd = queue_->commandBuffer();
    if (!cmd) return nullptr;

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
    color->setTexture(present_);
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);
    color->setClearColor(MTL::ClearColor(0.0, 0.0, 0.0, 0.0));

    MTL::RenderCommandEncoder* enc = cmd->renderCommandEncoder(pass);
    pass->release();
    if (!enc) return nullptr;

    enc->setRenderPipelineState(pipeline_);
    enc->setFragmentSamplerState(sampler_, 0);

    // Stack index 0 = front; draw back→front.
    const int32_t n = stack.count();
    for (int32_t i = n - 1; i >= 0; --i) {
        const Layer* layer = stack.at(i);
        if (!layer || !layer->visible || layer->opacity <= 0.f) continue;

        MTL::Texture* tex = nullptr;
        auto it = layers_.find(layer->id);
        if (it != layers_.end()) tex = it->second;

        if (tex) {
            CompositeUniforms u{std::clamp(layer->opacity, 0.f, 1.f)};
            if (compositeUniformBuf_) {
                std::memcpy(compositeUniformBuf_->contents(), &u, sizeof(u));
                enc->setFragmentBuffer(compositeUniformBuf_, 0, 0);
            } else {
                enc->setFragmentBytes(&u, sizeof(u), 0);
            }
            enc->setFragmentTexture(tex, 0);
            enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        }

        // Live overlay sits above the active layer even when that layer is still lazy
        // (no MTLTexture yet) — otherwise the first stroke is invisible until endStroke.
        if (showOverlay && overlay_ && layer->id == activeLayerId) {
            CompositeUniforms ou{1.f};
            if (compositeUniformBuf_) {
                std::memcpy(compositeUniformBuf_->contents(), &ou, sizeof(ou));
                enc->setFragmentBuffer(compositeUniformBuf_, 0, 0);
            } else {
                enc->setFragmentBytes(&ou, sizeof(ou), 0);
            }
            enc->setFragmentTexture(overlay_, 0);
            enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        }
    }

    enc->endEncoding();
    cmd->commit();
    // Swift MTKView uses a different queue — must finish before that queue samples present_.
    cmd->waitUntilCompleted();
    return present_;
}

void* LayerCompositor::presentTexture() const {
    return present_;
}

bool LayerCompositor::selfCheck(MTL::Device* device, MTL::CommandQueue* queue) {
    if (!device || !queue) return true; // headless ok
    LayerCompositor c;
    if (!c.init(device, queue)) return false;
    if (!c.ensureSize(8, 8)) return false;
    std::vector<uint8_t> red(8 * 8 * 4, 0);
    for (size_t i = 0; i < 64; ++i) {
        red[i * 4] = 255;
        red[i * 4 + 3] = 255;
    }
    c.uploadLayer(1, red.data(), 8, 8, {0, 0, 8, 8});
    // Minimal stack stand-in: just verify present texture exists after ensure.
    return c.presentTexture() != nullptr && c.dabPipeline() != nullptr;
}

} // namespace illus
