//
//  IllusStudioCanvasEditor.hpp
//  IllusStudioFramework — facade + stroke cache + Metal present
//

#pragma once

#include "document/PageSettings.hpp"
#include "layers/LayerStack.hpp"
#include "math/Rect.hpp"
#include "render/MetalRenderer.hpp"
#include "strokes/Stroke.hpp"
#include "tools/BrushLibrary.hpp"
#include "viewport/Viewport.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

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

    void markDirty();

    void clearActiveLayer();
    void clearAll(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // Layer ops that also keep stroke lists coherent.
    int32_t addLayer(const char* name);
    bool removeLayer(int32_t layerId);
    int32_t duplicateLayer(int32_t layerId); // -1 on fail
    bool moveLayer(int32_t layerId, int32_t toIndex);
    bool mergeLayerDown(int32_t srcId, int32_t dstId);

    void beginStroke(float x, float y, float pressure);
    void continueStroke(float x, float y, float pressure);
    void endStroke();

    int32_t strokeCountOnLayer(int32_t layerId) const;

    /// Composite into CPU cache; pointer valid until next mutating call.
    const uint8_t* compositePixels();

    /// Nearest-neighbor downsample of one layer into RGBA8 `out` (outW*outH*4).
    /// Lazy/empty layer → transparent. Returns false if invalid args / missing layer.
    bool copyLayerThumbnailRGBA(int32_t layerId, int32_t outW, int32_t outH, uint8_t* outRGBA, int32_t outByteCount) const;

    /// Flush composite, upload dirty region to Metal, return MTLTexture* (or null).
    void* presentMetalTexture();
    void* metalDevice() const { return metal_.deviceHandle(); }
    bool metalAvailable() const { return metalReady_; }

    int32_t width() const { return page_.width; }
    int32_t height() const { return page_.height; }

    static bool selfCheck();

private:
    LayerStrokeList& strokeListFor(int32_t layerId);
    const LayerStrokeList* strokeListFor(int32_t layerId) const;
    void ensureBelowCache();
    void flushDirtyComposite();
    StrokeSample smoothSample(float x, float y, float pressure, const BrushPreset& preset);

    PageSettings page_;
    LayerStack layers_;
    BrushLibrary brushes_;
    Viewport viewport_;
    std::unordered_map<int32_t, LayerStrokeList> strokesByLayer_;
    int32_t nextStrokeId_ = 1;

    std::vector<uint8_t> composite_;
    std::vector<uint8_t> belowCache_;
    bool belowValid_ = false;
    int32_t belowActiveIndex_ = -1;
    bool fullDirty_ = true;
    math::Rect dirtyRect_{};
    math::Rect uploadRect_{};
    bool uploadFull_ = true;

    bool stroking_ = false;
    Stroke liveStroke_;
    float dabCarry_ = 0.f;
    bool haveSmoothed_ = false;
    StrokeSample smoothed_{};

    MetalRenderer metal_;
    bool metalReady_ = false;
};

} // namespace illus
