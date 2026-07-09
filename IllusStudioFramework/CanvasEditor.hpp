//
//  CanvasEditor.hpp
//  IllusStudioFramework — public C++ API (Swift–C++ interop)
//

#pragma once

#include <cstdint>
#include <memory>
#include <cstddef>

namespace illus {

enum class ToolMode : int32_t {
    Brush = 0,
    Eraser = 1,
    Pointer = 2,
};

/// Public editor facade. Shared ownership so Swift can hold/pass copies.
class CanvasEditor {
public:
    CanvasEditor(int32_t width, int32_t height);
    ~CanvasEditor();

    CanvasEditor(const CanvasEditor&) = default;
    CanvasEditor& operator=(const CanvasEditor&) = default;
    CanvasEditor(CanvasEditor&&) noexcept = default;
    CanvasEditor& operator=(CanvasEditor&&) noexcept = default;

    void setBackground(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void clearAll(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void clearActiveLayer();

    int32_t width() const;
    int32_t height() const;

    int32_t layerCount() const;
    int32_t addLayer(const char* name);
    bool removeLayer(int32_t layerId);
    bool setActiveLayer(int32_t layerId);
    int32_t activeLayerId() const;

    bool setLayerOpacity(int32_t layerId, float opacity);
    float layerOpacity(int32_t layerId) const;
    bool setLayerVisible(int32_t layerId, bool visible);
    bool layerVisible(int32_t layerId) const;
    int32_t duplicateLayer(int32_t layerId);
    bool moveLayer(int32_t layerId, int32_t toIndex);
    bool mergeLayerDown(int32_t srcId, int32_t dstId);
    int32_t layerIndex(int32_t layerId) const;

    void setTool(ToolMode mode);
    ToolMode tool() const;

    int32_t brushSetCount() const;
    const char* brushSetName(int32_t setIndex) const;
    int32_t brushPresetCount() const;
    int32_t brushPresetCountInSet(int32_t setIndex) const;
    const char* brushPresetName(int32_t index) const;
    bool setBrushPreset(int32_t index);
    bool setBrushPresetInSet(int32_t setIndex, int32_t presetIndex);
    int32_t activeBrushPresetIndex() const;

    void setBrushLineWidth(float px);
    float brushLineWidth() const;
    void setBrushLineSmooth(float s);
    float brushLineSmooth() const;
    void setBrushHardness(float h);
    float brushHardness() const;
    void setBrushOpacity(float a);
    float brushOpacity() const;
    void setBrushFlow(float f);
    float brushFlow() const;
    void setBrushSpacing(float spacing);
    float brushSpacing() const;
    void setBrushSizePressure(float g);
    float brushSizePressure() const;
    void setBrushOpacityPressure(float g);
    float brushOpacityPressure() const;
    void setBrushColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void resetBrushSession();
    int32_t saveBrushSessionAsPreset(const char* name);

    void beginStroke(float x, float y, float pressure);
    void continueStroke(float x, float y, float pressure);
    void endStroke();

    int32_t strokeCountOnLayer(int32_t layerId) const;

    void setViewport(float scale, float offsetX, float offsetY);
    float viewportScale() const;
    float viewportOffsetX() const;
    float viewportOffsetY() const;
    /// Map view-space point → canvas pixels (tools always use canvas space).
    float viewToCanvasX(float viewX, float viewY, float viewW, float viewH) const;
    float viewToCanvasY(float viewX, float viewY, float viewW, float viewH) const;
    float canvasToViewX(float canvasX, float canvasY, float viewW, float viewH) const;
    float canvasToViewY(float canvasX, float canvasY, float viewW, float viewH) const;

    /// Borrowed MTLTexture* as address (owned by editor). 0 if Metal unavailable.
    std::uintptr_t presentMetalTextureAddress();
    /// Borrowed MTLDevice* as address (owned by editor). Use this for MTKView —
    /// textures cannot be sampled across different MTLDevice instances.
    std::uintptr_t metalDeviceAddress() const;
    bool metalAvailable() const;

    static bool selfCheck();
    static const char* version();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace illus
