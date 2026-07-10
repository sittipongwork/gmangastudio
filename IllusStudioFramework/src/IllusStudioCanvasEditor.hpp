//
//  IllusStudioCanvasEditor.hpp
//  IllusStudioFramework — facade + stroke cache + Metal present
//

#pragma once

#include "document/PageSettings.hpp"
#include "layers/LayerStack.hpp"
#include "math/Rect.hpp"
#include "math/TileGrid.hpp"
#include "render/LayerCompositor.hpp"
#include "render/MetalRenderer.hpp"
#include "strokes/Stroke.hpp"
#include "tools/BrushLibrary.hpp"
#include "viewport/Viewport.hpp"

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace MTL {
class Texture;
}

namespace illus {

class IllusStudioCanvasEditor {
public:
    IllusStudioCanvasEditor(int32_t width, int32_t height);

    const PageSettings& page() const { return page_; }
    void setBackground(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    LayerStack& layers() { return layers_; }
    const LayerStack& layers() const { return layers_; }
    BrushLibrary& brushes() { return brushes_; }
    const BrushLibrary& brushes() const { return brushes_; }

    Viewport& viewport() { return viewport_; }
    const Viewport& viewport() const { return viewport_; }
    void setViewport(float scale, float offsetX, float offsetY);

    float viewToCanvasX(float viewX, float viewY, float viewW, float viewH) const;
    float viewToCanvasY(float viewX, float viewY, float viewW, float viewH) const;
    float canvasToViewX(float canvasX, float canvasY, float viewW, float viewH) const;
    float canvasToViewY(float canvasX, float canvasY, float viewW, float viewH) const;

    /// Present NDC quad via GLM (TX-7). Writes xmin,ymin,xmax,ymax.
    void presentNdcRect(float viewW, float viewH, float out[4]) const;

    void markDirty();

    void clearActiveLayer();
    void clearAll(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    int32_t addLayer(const char* name);
    bool removeLayer(int32_t layerId);
    int32_t duplicateLayer(int32_t layerId);
    bool moveLayer(int32_t layerId, int32_t toIndex);
    bool mergeLayerDown(int32_t srcId, int32_t dstId);

    void beginStroke(float x, float y, float pressure);
    void continueStroke(float x, float y, float pressure);
    void endStroke();

    int32_t strokeCountOnLayer(int32_t layerId) const;

    const uint8_t* compositePixels();

    bool copyLayerThumbnailRGBA(int32_t layerId, int32_t outW, int32_t outH, uint8_t* outRGBA, int32_t outByteCount) const;

    void* presentMetalTexture();
    void* metalDevice() const { return metal_.deviceHandle(); }
    bool metalAvailable() const { return metalReady_; }

    /// Cap GPU composite rebuilds to this Hz. 0 = uncapped. UI sets from user pick.
    void setTargetPresentFps(int32_t fps);
    int32_t targetPresentFps() const { return targetPresentFps_; }

    int32_t width() const { return page_.width; }
    int32_t height() const { return page_.height; }

    static bool selfCheck();

private:
    LayerStrokeList& strokeListFor(int32_t layerId);
    const LayerStrokeList* strokeListFor(int32_t layerId) const;
    void ensureBelowCache();
    void flushDirtyComposite();
    void noteDirty(const math::Rect& local);
    void clearLiveOverlay();
    void mergeLiveOverlayIntoLayer();
    void blendLiveOverlayIntoComposite(const math::Rect& rect);
    void syncGpuLayer(int32_t layerId, math::Rect dirty);
    void syncAllGpuLayers();
    bool stampDabGpuOrCpu(
        uint8_t* cpuPixels,
        MTL::Texture* gpuTex,
        float x,
        float y,
        float pressure,
        const BrushPreset& preset,
        math::Rect& dirty
    );
    StrokeSample smoothSample(float x, float y, float pressure, const BrushPreset& preset);

    PageSettings page_;
    LayerStack layers_;
    BrushLibrary brushes_;
    Viewport viewport_;
    std::unordered_map<int32_t, LayerStrokeList> strokesByLayer_;
    int32_t nextStrokeId_ = 1;

    std::vector<uint8_t> composite_;
    std::vector<uint8_t> belowCache_;
    /// T1-2: live paint accumulates here; merged into layer on endStroke.
    std::vector<uint8_t> liveOverlay_;
    bool belowValid_ = false;
    int32_t belowActiveIndex_ = -1;
    bool fullDirty_ = true;
    math::Rect dirtyRect_{};
    math::Rect uploadRect_{};
    bool uploadFull_ = true;
    math::TileGrid dirtyTiles_;

    bool stroking_ = false;
    bool strokeUsesOverlay_ = false; // paint → overlay; erase → layer
    Stroke liveStroke_;
    float dabCarry_ = 0.f;
    float strokeDistPx_ = 0.f;
    bool haveSmoothed_ = false;
    StrokeSample smoothed_{};

    MetalRenderer metal_;
    LayerCompositor gpu_;
    bool metalReady_ = false;
    bool gpuCompositeReady_ = false;

    int32_t targetPresentFps_ = 120;
    std::chrono::steady_clock::time_point lastPresentRebuild_{};
    bool haveLastPresentRebuild_ = false;
};

} // namespace illus
