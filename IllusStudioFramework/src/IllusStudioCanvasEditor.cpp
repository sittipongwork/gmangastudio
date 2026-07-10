//
//  IllusStudioCanvasEditor.cpp
//  IllusStudioFramework
//

#include "IllusStudioCanvasEditor.hpp"

#include "math/Blend.hpp"
#include "math/Bezier.hpp"
#include "math/PresentTransform.hpp"
#include "render/MetalStrokeRasterizer.hpp"
#include "render/SoftwareRenderer.hpp"
#include "render/StrokeRasterizer.hpp"
#include "tools/procreate/FixtureBrushBytes.hpp"
#include "tools/procreate/ProcreateBrushImporter.hpp"

#include <Metal/Metal.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace illus {

IllusStudioCanvasEditor::IllusStudioCanvasEditor(int32_t width, int32_t height)
    : page_{std::max(1, width), std::max(1, height), {0, 0, 0, 0}}
    , layers_(page_.width, page_.height)
{
    const size_t n = static_cast<size_t>(page_.width) * static_cast<size_t>(page_.height) * 4u;
    composite_.assign(n, 0);
    belowCache_.assign(n, 0);
    uploadFull_ = true;
    metalReady_ = metal_.ensureSize(page_.width, page_.height);
    if (metalReady_ && metal_.device() && metal_.commandQueue()) {
        gpuCompositeReady_ = gpu_.init(metal_.device(), metal_.commandQueue())
            && gpu_.ensureSize(page_.width, page_.height);
        if (gpuCompositeReady_) syncAllGpuLayers();
    }
    strokeListFor(layers_.activeId());
}

void IllusStudioCanvasEditor::setBackground(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // White (and any page fill) lives on Background Layer — page clear stays transparent.
    page_.background = {0, 0, 0, 0};
    for (int32_t i = layers_.count() - 1; i >= 0; --i) {
        Layer* layer = layers_.at(i);
        if (layer && layer->isBackground) {
            layer->fillSolid(page_.width, page_.height, {r, g, b, a});
            if (gpuCompositeReady_) {
                syncGpuLayer(layer->id, {0, 0, page_.width, page_.height});
            }
            break;
        }
    }
    markDirty();
}

void IllusStudioCanvasEditor::setViewport(float scale, float offsetX, float offsetY) {
    viewport_.scale = scale;
    viewport_.offsetX = offsetX;
    viewport_.offsetY = offsetY;
    viewport_.clampScale();
}

float IllusStudioCanvasEditor::viewToCanvasX(float viewX, float viewY, float viewW, float viewH) const {
    float x = 0.f, y = 0.f;
    viewport_.viewToCanvas(viewX, viewY, viewW, viewH, static_cast<float>(page_.width), static_cast<float>(page_.height), x, y);
    return x;
}

float IllusStudioCanvasEditor::viewToCanvasY(float viewX, float viewY, float viewW, float viewH) const {
    float x = 0.f, y = 0.f;
    viewport_.viewToCanvas(viewX, viewY, viewW, viewH, static_cast<float>(page_.width), static_cast<float>(page_.height), x, y);
    return y;
}

float IllusStudioCanvasEditor::canvasToViewX(float canvasX, float canvasY, float viewW, float viewH) const {
    float x = 0.f, y = 0.f;
    viewport_.canvasToView(canvasX, canvasY, viewW, viewH, static_cast<float>(page_.width), static_cast<float>(page_.height), x, y);
    return x;
}

float IllusStudioCanvasEditor::canvasToViewY(float canvasX, float canvasY, float viewW, float viewH) const {
    float x = 0.f, y = 0.f;
    viewport_.canvasToView(canvasX, canvasY, viewW, viewH, static_cast<float>(page_.width), static_cast<float>(page_.height), x, y);
    return y;
}

void IllusStudioCanvasEditor::presentNdcRect(float viewW, float viewH, float out[4]) const {
    ::illus::presentNdcRect(
        viewport_,
        static_cast<float>(page_.width),
        static_cast<float>(page_.height),
        viewW,
        viewH,
        out
    );
}

LayerStrokeList& IllusStudioCanvasEditor::strokeListFor(int32_t layerId) {
    auto& list = strokesByLayer_[layerId];
    list.layerId = layerId;
    return list;
}

const LayerStrokeList* IllusStudioCanvasEditor::strokeListFor(int32_t layerId) const {
    const auto it = strokesByLayer_.find(layerId);
    return it == strokesByLayer_.end() ? nullptr : &it->second;
}

void IllusStudioCanvasEditor::syncGpuLayer(int32_t layerId, math::Rect dirty) {
    if (!gpuCompositeReady_) return;
    const Layer* layer = layers_.find(layerId);
    if (!layer || !layer->hasPixels()) {
        gpu_.clearLayer(layerId);
        return;
    }
    if (dirty.empty()) dirty = {0, 0, page_.width, page_.height};
    gpu_.uploadLayer(layerId, layer->pixels.data(), page_.width, page_.height, dirty);
}

void IllusStudioCanvasEditor::syncAllGpuLayers() {
    if (!gpuCompositeReady_) return;
    for (int32_t i = 0; i < layers_.count(); ++i) {
        const Layer* layer = layers_.at(i);
        if (!layer) continue;
        if (layer->hasPixels()) {
            gpu_.uploadLayer(layer->id, layer->pixels.data(), page_.width, page_.height, {0, 0, page_.width, page_.height});
        } else {
            gpu_.clearLayer(layer->id);
        }
    }
}

bool IllusStudioCanvasEditor::stampDabGpuOrCpu(
    uint8_t* cpuPixels,
    MTL::Texture* /*gpuTex*/,
    float x,
    float y,
    float pressure,
    const BrushPreset& preset,
    math::Rect& dirty
) {
    // ponytail: CPU stamp + dirty uploadOverlay; dual GPU dab was waitUntilCompleted per dab (~70fps).
    return StrokeRasterizer::stampDab(
        cpuPixels, page_.width, page_.height, x, y, pressure, preset, dirty, &brushes_.assets()
    );
}

void IllusStudioCanvasEditor::clearActiveLayer() {
    if (Layer* layer = layers_.active()) {
        layer->clearTransparent();
        strokeListFor(layer->id).clear();
        if (gpuCompositeReady_) gpu_.clearLayer(layer->id);
        markDirty();
    }
}

void IllusStudioCanvasEditor::clearAll(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    page_.background = {0, 0, 0, 0};
    for (int32_t i = 0; i < layers_.count(); ++i) {
        if (Layer* layer = layers_.at(i)) {
            if (layer->isBackground) {
                layer->fillSolid(page_.width, page_.height, {r, g, b, a});
            } else {
                layer->clearTransparent();
            }
            strokeListFor(layer->id).clear();
        }
    }
    markDirty();
}

int32_t IllusStudioCanvasEditor::addLayer(const char* name) {
    const int32_t id = layers_.add(name);
    strokeListFor(id);
    markDirty();
    return id;
}

bool IllusStudioCanvasEditor::removeLayer(int32_t layerId) {
    if (!layers_.remove(layerId)) return false;
    strokesByLayer_.erase(layerId);
    if (gpuCompositeReady_) gpu_.removeLayer(layerId);
    markDirty();
    return true;
}

int32_t IllusStudioCanvasEditor::duplicateLayer(int32_t layerId) {
    const auto newId = layers_.duplicate(layerId);
    if (!newId) return -1;
    // Copy vector strokes with new ids (raster already duplicated with pixels).
    const LayerStrokeList* src = static_cast<const IllusStudioCanvasEditor*>(this)->strokeListFor(layerId);
    if (src) {
        LayerStrokeList& dst = strokeListFor(*newId);
        dst.strokes.clear();
        for (const Stroke& s : src->strokes) {
            Stroke copy = s;
            copy.id = nextStrokeId_++;
            copy.layerId = *newId;
            dst.strokes.push_back(std::move(copy));
        }
    }
    markDirty();
    return *newId;
}

bool IllusStudioCanvasEditor::moveLayer(int32_t layerId, int32_t toIndex) {
    if (!layers_.move(layerId, toIndex)) return false;
    markDirty();
    return true;
}

bool IllusStudioCanvasEditor::mergeLayerDown(int32_t srcId, int32_t dstId) {
    if (!layers_.mergeDown(srcId, dstId)) return false;
    // Append src strokes onto dst, then drop src list.
    const LayerStrokeList* src = static_cast<const IllusStudioCanvasEditor*>(this)->strokeListFor(srcId);
    if (src) {
        LayerStrokeList& dst = strokeListFor(dstId);
        for (const Stroke& s : src->strokes) {
            Stroke copy = s;
            copy.id = nextStrokeId_++;
            copy.layerId = dstId;
            dst.strokes.push_back(std::move(copy));
        }
    }
    strokesByLayer_.erase(srcId);
    markDirty();
    return true;
}

StrokeSample IllusStudioCanvasEditor::smoothSample(float x, float y, float pressure, const BrushPreset& preset) {
    StrokeSample raw{x, y, pressure, 0, 0, 0};
    if (preset.lineSmooth <= 0.f || !haveSmoothed_) {
        haveSmoothed_ = true;
        smoothed_ = raw;
        return raw;
    }
    // EMA: higher lineSmooth → heavier lag (mix toward previous).
    const float alpha = 1.f - preset.lineSmooth * 0.85f;
    smoothed_.x = smoothed_.x + (raw.x - smoothed_.x) * alpha;
    smoothed_.y = smoothed_.y + (raw.y - smoothed_.y) * alpha;
    smoothed_.pressure = raw.pressure; // keep raw pressure (documented)
    return smoothed_;
}

void IllusStudioCanvasEditor::beginStroke(float x, float y, float pressure) {
    if (brushes_.tool() == ToolMode::Pointer) return;
    Layer* layer = layers_.active();
    if (!layer) return;

    stroking_ = true;
    ensureBelowCache();
    haveSmoothed_ = false;
    dabCarry_ = 0.f;

    liveStroke_ = Stroke{};
    liveStroke_.id = nextStrokeId_++;
    liveStroke_.layerId = layer->id;
    liveStroke_.presetSnapshot = brushes_.resolvedPreset();
    liveStroke_.bounds.reset();

    // Paint → live overlay (T1-2); erase still hits the layer (dest-out needs backdrop).
    strokeUsesOverlay_ = liveStroke_.presetSnapshot.mode != BrushMode::Erase;
    if (strokeUsesOverlay_) {
        const size_t n = static_cast<size_t>(page_.width) * static_cast<size_t>(page_.height) * 4u;
        liveOverlay_.assign(n, 0);
        if (gpuCompositeReady_) gpu_.clearOverlay();
    } else {
        liveOverlay_.clear();
    }

    const StrokeSample s = smoothSample(x, y, pressure, liveStroke_.presetSnapshot);
    liveStroke_.samples.push_back(s);

    math::Rect localDirty{};
    if (strokeUsesOverlay_) {
        stampDabGpuOrCpu(
            liveOverlay_.data(),
            gpuCompositeReady_ ? gpu_.overlayTexture() : nullptr,
            s.x,
            s.y,
            s.pressure,
            liveStroke_.presetSnapshot,
            localDirty
        );
        if (gpuCompositeReady_ && !localDirty.empty()) {
            gpu_.uploadOverlay(liveOverlay_.data(), page_.width, page_.height, localDirty);
        }
    } else {
        layer->ensurePixels(page_.width, page_.height);
        // Erase: CPU into layer; sync GPU texture so present stays current.
        StrokeRasterizer::stampDab(
            *layer,
            page_.width,
            page_.height,
            s.x,
            s.y,
            s.pressure,
            liveStroke_.presetSnapshot,
            localDirty,
            &brushes_.assets()
        );
        if (gpuCompositeReady_) syncGpuLayer(layer->id, localDirty);
    }
    liveStroke_.bounds.expand(s.x, s.y, StrokeRasterizer::effectiveRadius(s.pressure, liveStroke_.presetSnapshot) + 1.f);
    noteDirty(localDirty);
}

void IllusStudioCanvasEditor::continueStroke(float x, float y, float pressure) {
    if (!stroking_) {
        beginStroke(x, y, pressure);
        return;
    }
    Layer* layer = layers_.active();
    if (!layer || liveStroke_.samples.empty()) return;

    const StrokeSample next = smoothSample(x, y, pressure, liveStroke_.presetSnapshot);
    const StrokeSample& prev = liveStroke_.samples.back();
    math::Rect localDirty{};
    if (strokeUsesOverlay_ && !liveOverlay_.empty()) {
        StrokeRasterizer::stampSegment(
            liveOverlay_.data(),
            page_.width,
            page_.height,
            prev,
            next,
            liveStroke_.presetSnapshot,
            dabCarry_,
            localDirty,
            &liveStroke_.bounds,
            &brushes_.assets()
        );
        if (gpuCompositeReady_ && !localDirty.empty()) {
            gpu_.uploadOverlay(liveOverlay_.data(), page_.width, page_.height, localDirty);
        }
    } else {
        StrokeRasterizer::stampSegment(
            *layer,
            page_.width,
            page_.height,
            prev,
            next,
            liveStroke_.presetSnapshot,
            dabCarry_,
            localDirty,
            &liveStroke_.bounds,
            &brushes_.assets()
        );
        if (gpuCompositeReady_) syncGpuLayer(layer->id, localDirty);
    }
    liveStroke_.samples.push_back(next);
    noteDirty(localDirty);
}

void IllusStudioCanvasEditor::endStroke() {
    if (!stroking_) return;
    stroking_ = false;
    haveSmoothed_ = false;
    const int32_t layerId = liveStroke_.layerId;
    math::Rect syncDirty = dirtyRect_;
    if (strokeUsesOverlay_) {
        mergeLiveOverlayIntoLayer();
        clearLiveOverlay();
    }
    strokeUsesOverlay_ = false;
    if (!liveStroke_.samples.empty()) {
        // T2-7-1: keep dense samples; fit cubics lazily (export/SVG) — Eigen under
        // this mutex blocked present and spiked CPU on every pen-up.
        strokeListFor(liveStroke_.layerId).strokes.push_back(std::move(liveStroke_));
    }
    liveStroke_ = Stroke{};
    if (gpuCompositeReady_) {
        if (syncDirty.empty()) syncDirty = {0, 0, page_.width, page_.height};
        syncGpuLayer(layerId, syncDirty);
    }
}

void IllusStudioCanvasEditor::noteDirty(const math::Rect& local) {
    if (local.empty()) return;
    dirtyRect_.unionWith(local.x, local.y, local.x + local.w - 1, local.y + local.h - 1);
    dirtyRect_.clipTo(page_.width, page_.height);
    dirtyTiles_.markRect(local, page_.width, page_.height);
}

void IllusStudioCanvasEditor::clearLiveOverlay() {
    liveOverlay_.clear();
    if (gpuCompositeReady_) gpu_.clearOverlay();
}

void IllusStudioCanvasEditor::mergeLiveOverlayIntoLayer() {
    if (liveOverlay_.empty()) return;
    Layer* layer = layers_.find(liveStroke_.layerId);
    if (!layer) return;
    layer->ensurePixels(page_.width, page_.height);
    math::Rect r = dirtyRect_;
    if (r.empty()) {
        r = {0, 0, page_.width, page_.height};
    }
    math::blendLayerRect(layer->pixels.data(), liveOverlay_.data(), page_.width, r, 1.f);
    // Layer changed — below-cache still valid; re-composite active+above for dirty.
    noteDirty(r);
}

void IllusStudioCanvasEditor::blendLiveOverlayIntoComposite(const math::Rect& rect) {
    if (liveOverlay_.empty() || rect.empty()) return;
    math::blendLayerRect(composite_.data(), liveOverlay_.data(), page_.width, rect, 1.f);
}

int32_t IllusStudioCanvasEditor::strokeCountOnLayer(int32_t layerId) const {
    const LayerStrokeList* list = strokeListFor(layerId);
    return list ? static_cast<int32_t>(list->strokes.size()) : 0;
}

const uint8_t* IllusStudioCanvasEditor::compositePixels() {
    flushDirtyComposite();
    return composite_.data();
}

bool IllusStudioCanvasEditor::copyLayerThumbnailRGBA(
    int32_t layerId,
    int32_t outW,
    int32_t outH,
    uint8_t* outRGBA,
    int32_t outByteCount
) const {
    if (!outRGBA || outW < 1 || outH < 1) return false;
    const int32_t need = outW * outH * 4;
    if (outByteCount < need) return false;

    const Layer* layer = layers_.find(layerId);
    if (!layer) return false;

    std::fill(outRGBA, outRGBA + need, static_cast<uint8_t>(0));
    if (!layer->hasPixels()) return true;

    const int32_t srcW = page_.width;
    const int32_t srcH = page_.height;
    const size_t srcBytes = static_cast<size_t>(srcW) * static_cast<size_t>(srcH) * 4u;
    if (layer->pixels.size() != srcBytes) return true;

    // Nearest-neighbor downsample (ponytail: good enough for ~40px thumbs).
    for (int32_t y = 0; y < outH; ++y) {
        const int32_t sy = std::min(srcH - 1, (y * srcH) / outH);
        for (int32_t x = 0; x < outW; ++x) {
            const int32_t sx = std::min(srcW - 1, (x * srcW) / outW);
            const size_t si = (static_cast<size_t>(sy) * static_cast<size_t>(srcW) + static_cast<size_t>(sx)) * 4u;
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(outW) + static_cast<size_t>(x)) * 4u;
            outRGBA[di] = layer->pixels[si];
            outRGBA[di + 1] = layer->pixels[si + 1];
            outRGBA[di + 2] = layer->pixels[si + 2];
            outRGBA[di + 3] = layer->pixels[si + 3];
        }
    }
    return true;
}

void IllusStudioCanvasEditor::setTargetPresentFps(int32_t fps) {
    targetPresentFps_ = std::max(0, fps);
    haveLastPresentRebuild_ = false; // apply new cadence on next present
}

void* IllusStudioCanvasEditor::presentMetalTexture() {
    // T1-4: GPU stack blend; stroke path already dirty-uploads overlay/layer.
    // ponytail: do not syncAll + CPU flush every present — that capped ~70fps.
    if (gpuCompositeReady_) {
        const auto now = std::chrono::steady_clock::now();
        const bool forceSync = uploadFull_ || fullDirty_;
        if (!forceSync && haveLastPresentRebuild_ && targetPresentFps_ > 0) {
            const auto minInterval =
                std::chrono::duration<double>(1.0 / static_cast<double>(targetPresentFps_));
            if ((now - lastPresentRebuild_) < minInterval) {
                // Skip GPU composite — return last present_ (UI may still blit).
                void* cached = gpu_.presentTexture();
                metalReady_ = cached != nullptr;
                return cached;
            }
        }
        if (forceSync) {
            syncAllGpuLayers();
            uploadFull_ = false;
        }
        uploadRect_.setEmpty();
        void* tex = gpu_.present(
            layers_,
            layers_.activeId(),
            stroking_ && strokeUsesOverlay_
        );
        lastPresentRebuild_ = now;
        haveLastPresentRebuild_ = true;
        metalReady_ = tex != nullptr;
        return tex;
    }

    const bool needUpload = uploadFull_ || fullDirty_ || !dirtyRect_.empty() || !uploadRect_.empty() || !metal_.valid();
    if (!needUpload) {
        return metal_.textureHandle();
    }

    const bool full = uploadFull_ || fullDirty_ || !metal_.valid();
    math::Rect region = dirtyRect_;
    if (!uploadRect_.empty()) {
        if (region.empty()) {
            region = uploadRect_;
        } else {
            region.unionWith(
                uploadRect_.x,
                uploadRect_.y,
                uploadRect_.x + uploadRect_.w - 1,
                uploadRect_.y + uploadRect_.h - 1
            );
        }
    }

    flushDirtyComposite();

    if (full || region.empty()) {
        region = {0, 0, page_.width, page_.height};
    }
    region.clipTo(page_.width, page_.height);

    metal_.upload(composite_.data(), page_.width, page_.height, region);
    uploadRect_.setEmpty();
    uploadFull_ = false;
    metalReady_ = metal_.valid();
    return metal_.textureHandle();
}

void IllusStudioCanvasEditor::ensureBelowCache() {
    const int32_t activeIndex = layers_.indexOf(layers_.activeId());
    if (belowValid_ && belowActiveIndex_ == activeIndex && !fullDirty_) return;
    SoftwareRenderer::compositeBelow(page_, layers_, activeIndex, belowCache_);
    belowValid_ = true;
    belowActiveIndex_ = activeIndex;
    SoftwareRenderer::compositeFromBelow(
        page_, layers_, activeIndex, belowCache_, composite_, {0, 0, page_.width, page_.height}
    );
    fullDirty_ = false;
    dirtyRect_.setEmpty();
    uploadFull_ = true;
}

void IllusStudioCanvasEditor::flushDirtyComposite() {
    if (fullDirty_) {
        SoftwareRenderer::composite(page_, layers_, composite_);
        fullDirty_ = false;
        dirtyRect_.setEmpty();
        dirtyTiles_.clear();
        belowValid_ = false;
        uploadFull_ = true;
        if (stroking_ && strokeUsesOverlay_ && !liveOverlay_.empty()) {
            blendLiveOverlayIntoComposite({0, 0, page_.width, page_.height});
        }
        return;
    }
    if (dirtyRect_.empty()) {
        // Still need overlay refresh while stroking if tiles marked.
        if (!(stroking_ && strokeUsesOverlay_ && !dirtyTiles_.empty())) return;
    }

    const int32_t activeIndex = layers_.indexOf(layers_.activeId());
    if (!belowValid_ || belowActiveIndex_ != activeIndex) {
        ensureBelowCache();
        if (stroking_ && strokeUsesOverlay_ && !liveOverlay_.empty()) {
            blendLiveOverlayIntoComposite({0, 0, page_.width, page_.height});
        }
        return;
    }

    math::Rect region = dirtyRect_;
    if (region.empty()) {
        region = dirtyTiles_.bounds(page_.width, page_.height);
    }
    if (region.empty()) return;

    if (uploadRect_.empty()) uploadRect_ = region;
    else {
        uploadRect_.unionWith(
            region.x,
            region.y,
            region.x + region.w - 1,
            region.y + region.h - 1
        );
    }
    SoftwareRenderer::compositeFromBelow(page_, layers_, activeIndex, belowCache_, composite_, region);
    if (stroking_ && strokeUsesOverlay_ && !liveOverlay_.empty()) {
        blendLiveOverlayIntoComposite(region);
    }
    dirtyRect_.setEmpty();
    dirtyTiles_.clear();
}

void IllusStudioCanvasEditor::markDirty() {
    fullDirty_ = true;
    belowValid_ = false;
    dirtyRect_.setEmpty();
    dirtyTiles_.clear();
    uploadFull_ = true;
    uploadRect_.setEmpty();
    clearLiveOverlay();
    stroking_ = false;
    strokeUsesOverlay_ = false;
    if (gpuCompositeReady_) syncAllGpuLayers();
}

bool IllusStudioCanvasEditor::selfCheck() {
    if (!SoftwareRenderer::selfCheck()) return false;
    if (!MetalRenderer::selfCheck()) return false;
    {
        // GPU compositor stacking (top→down / index 0 front).
        IllusStudioCanvasEditor probe(4, 4);
        if (probe.metalAvailable() && probe.metal_.device() && probe.metal_.commandQueue()) {
            if (!LayerCompositor::selfCheck(probe.metal_.device(), probe.metal_.commandQueue())) {
                return false;
            }
        }
    }

    // Paint darkens active layer; stroke count += 1.
    {
        IllusStudioCanvasEditor editor(32, 32);
        const int32_t layerId = editor.layers().activeId();
        editor.beginStroke(16, 16, 1);
        editor.endStroke();
        if (editor.strokeCountOnLayer(layerId) != 1) return false;
        const uint8_t* p = editor.compositePixels();
        const size_t i = (16u * 32u + 16u) * 4u;
        if (p[i] == 255 && p[i + 1] == 255 && p[i + 2] == 255) return false;
        if (editor.metalAvailable() && editor.presentMetalTexture() == nullptr) return false;
    }

    // T1-2: live overlay merges into layer on endStroke (pixels change only after end).
    {
        IllusStudioCanvasEditor editor(32, 32);
        Layer* layer = editor.layers().active();
        if (!layer) return false;
        editor.beginStroke(16, 16, 1);
        // During stroke, layer pixels stay clean for paint-overlay path.
        if (layer->hasPixels()) {
            const uint8_t* px = layer->pixelAt(16, 16, 32);
            if (px[3] > 8) return false;
        }
        editor.endStroke();
        if (!layer->hasPixels()) return false;
        if (layer->pixelAt(16, 16, 32)[3] < 10) return false;
    }

    // T1-3 / T1-4: GPU present path returns a texture when Metal is available.
    {
        IllusStudioCanvasEditor editor(16, 16);
        editor.beginStroke(8, 8, 1);
        editor.endStroke();
        if (editor.metalAvailable() && editor.presentMetalTexture() == nullptr) return false;
    }

    // Target present FPS is stored and readable (throttle exercised by UI).
    {
        IllusStudioCanvasEditor editor(8, 8);
        editor.setTargetPresentFps(60);
        if (editor.targetPresentFps() != 60) return false;
        editor.setTargetPresentFps(120);
        if (editor.targetPresentFps() != 120) return false;
    }

    // Erase: paint then erase → near-transparent at center.
    {
        IllusStudioCanvasEditor editor(48, 48);
        editor.brushes().setActivePreset(editor.brushes().presetIdAt(0));
        editor.beginStroke(24, 24, 1);
        editor.continueStroke(24, 24, 1);
        editor.endStroke();
        Layer* layer = editor.layers().active();
        if (!layer || !layer->hasPixels()) return false;
        const uint8_t paintedA = layer->pixelAt(24, 24, 48)[3];
        if (paintedA < 10) return false;

        editor.brushes().setTool(ToolMode::Eraser);
        editor.beginStroke(24, 24, 1);
        editor.continueStroke(24, 24, 1);
        editor.endStroke();
        const uint8_t erasedA = layer->pixelAt(24, 24, 48)[3];
        if (erasedA > paintedA / 2) return false;
        if (editor.strokeCountOnLayer(layer->id) != 2) return false;
    }

    // lineWidth affects dab; mid-stroke session change ignored.
    {
        IllusStudioCanvasEditor thin(64, 64);
        thin.brushes().session().lineWidthPx = 4.f;
        thin.beginStroke(32, 32, 1);
        thin.endStroke();
        const Layer* a = thin.layers().active();
        if (!a || !a->hasPixels()) return false;

        IllusStudioCanvasEditor thick(64, 64);
        thick.brushes().session().lineWidthPx = 40.f;
        thick.beginStroke(32, 32, 1);
        // Mid-stroke change must not affect in-flight snapshot.
        thick.brushes().session().lineWidthPx = 4.f;
        thick.endStroke();
        const Layer* b = thick.layers().active();
        if (!b || !b->hasPixels()) return false;

        auto countOpaque = [](const Layer& layer, int32_t w) {
            int n = 0;
            for (size_t i = 3; i < layer.pixels.size(); i += 4) {
                if (layer.pixels[i] > 8) ++n;
            }
            (void)w;
            return n;
        };
        if (countOpaque(*b, 64) <= countOpaque(*a, 64)) return false;
        if (thick.strokeCountOnLayer(b->id) != 1) return false;
        // Snapshot kept wide: live stroke used 40px despite session change.
        if (thick.strokeListFor(b->id).strokes[0].presetSnapshot.lineWidthPx < 30.f) return false;
    }

    // New project seeds Background Layer + Layer 1; Add → Layer N at front.
    // White is on Background Layer; hide it → transparent composite (not page white).
    {
        IllusStudioCanvasEditor e(8, 8);
        if (e.layers().count() != 2) return false;
        const Layer* front = e.layers().at(0);
        const Layer* back = e.layers().at(1);
        if (!front || !back) return false;
        if (front->name != "Layer 1" || front->isBackground) return false;
        if (back->name != "Background Layer" || !back->isBackground) return false;
        if (!back->hasPixels()) return false;
        if (e.layers().activeId() != front->id) return false;
        if (e.layers().remove(back->id)) return false; // background locked

        const uint8_t* white = e.compositePixels();
        if (white[0] != 255 || white[1] != 255 || white[2] != 255 || white[3] != 255) return false;
        if (!e.layers().setVisible(back->id, false)) return false;
        e.markDirty();
        const uint8_t* clear = e.compositePixels();
        if (clear[3] != 0) return false; // page underlay is transparent
        if (!e.layers().setVisible(back->id, true)) return false;

        const int32_t id2 = e.addLayer(nullptr);
        if (e.layers().count() != 3) return false;
        const Layer* top = e.layers().at(0);
        if (!top || top->id != id2 || top->name != "Layer 2") return false;
    }

    // Stacking top→down: opaque paint on front layer covers under layer (CPU + GPU present).
    {
        IllusStudioCanvasEditor e(8, 8);
        const int32_t underId = e.layers().activeId();
        Layer* under = e.layers().find(underId);
        if (!under) return false;
        under->ensurePixels(8, 8);
        for (size_t i = 0; i < under->pixels.size(); i += 4) {
            under->pixels[i] = 255;
            under->pixels[i + 1] = 0;
            under->pixels[i + 2] = 0;
            under->pixels[i + 3] = 255;
        }
        const int32_t topId = e.addLayer("Top");
        Layer* top = e.layers().find(topId);
        if (!top || e.layers().indexOf(topId) != 0) return false;
        top->ensurePixels(8, 8);
        for (size_t i = 0; i < top->pixels.size(); i += 4) {
            top->pixels[i] = 0;
            top->pixels[i + 1] = 0;
            top->pixels[i + 2] = 255;
            top->pixels[i + 3] = 255;
        }
        e.markDirty();
        const uint8_t* cpu = e.compositePixels();
        // Front is blue — must not show under red.
        if (cpu[0] != 0 || cpu[1] != 0 || cpu[2] != 255) return false;

        if (e.metalAvailable()) {
            void* handle = e.presentMetalTexture();
            if (!handle) return false;
            auto* tex = static_cast<MTL::Texture*>(handle);
            uint8_t px[4] = {0, 0, 0, 0};
            MTL::Region region;
            region.origin = {0, 0, 0};
            region.size = {1, 1, 1};
            tex->getBytes(px, 4, region, 0);
            if (px[0] != 0 || px[1] != 0 || px[2] != 255) return false;
        }
    }

    // Layer thumbnail downsample (white bg → opaque white pixel).
    {
        IllusStudioCanvasEditor e(40, 60);
        const Layer* bg = e.layers().at(e.layers().count() - 1);
        if (!bg || !bg->isBackground) return false;
        uint8_t thumb[4 * 4 * 4];
        if (!e.copyLayerThumbnailRGBA(bg->id, 4, 4, thumb, static_cast<int32_t>(sizeof(thumb)))) return false;
        if (thumb[0] != 255 || thumb[1] != 255 || thumb[2] != 255 || thumb[3] != 255) return false;
        const int32_t paintId = e.layers().activeId();
        uint8_t empty[4 * 4 * 4];
        if (!e.copyLayerThumbnailRGBA(paintId, 4, 4, empty, static_cast<int32_t>(sizeof(empty)))) return false;
        if (empty[3] != 0) return false; // lazy paint layer → transparent
    }

    // Layer opacity / visibility / reorder / duplicate / merge.
    {
        IllusStudioCanvasEditor e(16, 16);
        const int32_t backId = e.layers().activeId();
        e.beginStroke(8, 8, 1);
        e.endStroke();
        const int32_t frontId = e.addLayer("Front");
        if (!e.layers().setOpacity(frontId, 0.5f)) return false;
        if (std::abs(e.layers().opacity(frontId) - 0.5f) > 1e-3f) return false;
        if (!e.layers().setVisible(frontId, false)) return false;
        if (e.layers().visible(frontId)) return false;
        if (!e.layers().setVisible(frontId, true)) return false;

        const int32_t dupId = e.duplicateLayer(backId);
        if (dupId < 0) return false;
        if (e.layers().count() < 4) return false;
        if (!e.moveLayer(dupId, 0)) return false;
        if (e.layers().indexOf(dupId) != 0) return false;

        // Merge front into back (pixels); stroke lists combine.
        const int32_t strokesBefore = e.strokeCountOnLayer(backId);
        if (!e.mergeLayerDown(frontId, backId)) return false;
        if (e.layers().find(frontId) != nullptr) return false;
        if (e.strokeCountOnLayer(backId) < strokesBefore) return false;
    }

    // Untouched paint layer stays lazy; Background Layer keeps its fill.
    {
        IllusStudioCanvasEditor e2(16, 16);
        const int32_t paintId = e2.layers().activeId();
        e2.addLayer("Front");
        e2.layers().setActive(e2.layers().at(0)->id);
        e2.markDirty();
        e2.beginStroke(8, 8, 1);
        e2.endStroke();
        const Layer* paint = e2.layers().find(paintId);
        if (!paint || paint->isBackground) return false;
        if (paint->hasPixels()
            && !std::all_of(paint->pixels.begin(), paint->pixels.end(), [](uint8_t v) { return v == 0; })) {
            return false;
        }
        const Layer* bg = e2.layers().at(e2.layers().count() - 1);
        if (!bg || !bg->isBackground || !bg->hasPixels()) return false;
    }

    // Pointer tool does not paint.
    {
        IllusStudioCanvasEditor editor(32, 32);
        editor.brushes().setTool(ToolMode::Pointer);
        const int32_t layerId = editor.layers().activeId();
        editor.beginStroke(16, 16, 1);
        editor.endStroke();
        if (editor.strokeCountOnLayer(layerId) != 0) return false;
        if (editor.brushes().tool() != ToolMode::Pointer) return false;
    }

    // Viewport view↔canvas round-trip.
    {
        IllusStudioCanvasEditor editor(100, 50);
        editor.setViewport(2.f, 10.f, -5.f);
        const float vw = 400.f, vh = 300.f;
        const float cx = 40.f, cy = 20.f;
        const float vx = editor.canvasToViewX(cx, cy, vw, vh);
        const float vy = editor.canvasToViewY(cx, cy, vw, vh);
        const float rx = editor.viewToCanvasX(vx, vy, vw, vh);
        const float ry = editor.viewToCanvasY(vx, vy, vw, vh);
        if (std::abs(rx - cx) > 1e-3f || std::abs(ry - cy) > 1e-3f) return false;
        if (std::abs(editor.viewport().scale - 2.f) > 1e-6f) return false;
        if (std::abs(editor.viewport().offsetX - 10.f) > 1e-6f) return false;
    }

    // TX-7: Eigen Bézier fit + present NDC (fit is opt-in, not on endStroke).
    if (!bezierSelfCheck()) return false;
    if (!presentTransformSelfCheck()) return false;
    {
        IllusStudioCanvasEditor editor(32, 32);
        editor.beginStroke(4, 4, 1);
        editor.continueStroke(8, 6, 1);
        editor.continueStroke(12, 4, 1);
        editor.continueStroke(16, 8, 1);
        editor.endStroke();
        LayerStrokeList& list = editor.strokeListFor(editor.layers().activeId());
        if (list.strokes.empty()) return false;
        fitStrokeCubics(list.strokes[0].samples, list.strokes[0].cubics, 2.f);
        if (list.strokes[0].cubics.empty()) return false;
        list.strokes[0].cubics.clear();
        if (!list.strokes[0].ensureCubics(2.f)) return false;
        float ndc[4];
        editor.presentNdcRect(64.f, 64.f, ndc);
        if (!std::isfinite(ndc[0]) || !std::isfinite(ndc[3])) return false;
    }

    // T1-7: fixture .brush import → set + tip texture + tip stamp paints.
    {
        IllusStudioCanvasEditor editor(48, 48);
        const int32_t setsBefore = editor.brushes().setCount();
        const auto imported = importBrushPackageBytes(
            editor.brushes(),
            editor.brushes().assets(),
            fixture::kBrushZip,
            fixture::kBrushZipSize,
            BrushPackageKind::Brush,
            "fixture.brush"
        );
        if (!imported.ok || imported.brushCount < 1) return false;
        if (editor.brushes().setCount() != setsBefore + 1) return false;
        const int32_t setIndex = editor.brushes().indexOfSetId(imported.setId);
        if (setIndex < 0) return false;
        if (editor.brushes().setSource(setIndex) != BrushSource::ImportedProcreate) return false;
        const int32_t presetId = editor.brushes().presetIdInSet(setIndex, 0);
        const BrushPreset* p = editor.brushes().find(presetId);
        if (!p || p->tipTextureId < 0) return false;
        if (!editor.brushes().assets().find(p->tipTextureId)) return false;
        if (std::abs(p->lineWidthPx - 24.f) > 0.5f) return false;
        if (!editor.brushes().setActivePreset(presetId)) return false;
        editor.beginStroke(24, 24, 1);
        editor.endStroke();
        Layer* layer = editor.layers().active();
        if (!layer || !layer->hasPixels()) return false;
        if (layer->pixelAt(24, 24, 48)[3] < 10) return false;
    }

    return true;
}

} // namespace illus
