//
//  CanvasEditor.cpp
//  IllusStudioFramework — public C++ API over IllusStudioCanvasEditor
//

#include "../CanvasEditor.hpp"
#include "IllusStudioCanvasEditor.hpp"
#include "tools/procreate/ProcreateBrushImporter.hpp"

#include <mutex>

namespace illus {

struct CanvasEditor::Impl {
    IllusStudioCanvasEditor editor;
    mutable std::mutex mutex;
    explicit Impl(int32_t w, int32_t h) : editor(w, h) {}
};

CanvasEditor::CanvasEditor(int32_t width, int32_t height)
    : impl_(std::make_shared<Impl>(width, height))
{
}

CanvasEditor::~CanvasEditor() = default;

void CanvasEditor::setBackground(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.setBackground(r, g, b, a);
}

void CanvasEditor::clearAll(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.clearAll(r, g, b, a);
}

void CanvasEditor::clearActiveLayer() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.clearActiveLayer();
}

int32_t CanvasEditor::width() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.width();
}

int32_t CanvasEditor::height() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.height();
}

int32_t CanvasEditor::layerCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.layers().count();
}

int32_t CanvasEditor::layerIdAt(int32_t index) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const Layer* layer = impl_->editor.layers().at(index);
    return layer ? layer->id : -1;
}

const char* CanvasEditor::layerName(int32_t layerId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const Layer* layer = impl_->editor.layers().find(layerId);
    return layer ? layer->name.c_str() : "";
}

int32_t CanvasEditor::addLayer(const char* name) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.addLayer(name);
}

bool CanvasEditor::removeLayer(int32_t layerId) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.removeLayer(layerId);
}

bool CanvasEditor::setActiveLayer(int32_t layerId) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->editor.layers().setActive(layerId)) return false;
    impl_->editor.markDirty();
    return true;
}

int32_t CanvasEditor::activeLayerId() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.layers().activeId();
}

bool CanvasEditor::setLayerOpacity(int32_t layerId, float opacity) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->editor.layers().setOpacity(layerId, opacity)) return false;
    impl_->editor.markDirty();
    return true;
}

float CanvasEditor::layerOpacity(int32_t layerId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.layers().opacity(layerId);
}

bool CanvasEditor::setLayerVisible(int32_t layerId, bool visible) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->editor.layers().setVisible(layerId, visible)) return false;
    impl_->editor.markDirty();
    return true;
}

bool CanvasEditor::layerVisible(int32_t layerId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.layers().visible(layerId);
}

bool CanvasEditor::layerIsBackground(int32_t layerId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const Layer* layer = impl_->editor.layers().find(layerId);
    return layer ? layer->isBackground : false;
}

int32_t CanvasEditor::duplicateLayer(int32_t layerId) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.duplicateLayer(layerId);
}

bool CanvasEditor::moveLayer(int32_t layerId, int32_t toIndex) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.moveLayer(layerId, toIndex);
}

bool CanvasEditor::mergeLayerDown(int32_t srcId, int32_t dstId) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.mergeLayerDown(srcId, dstId);
}

int32_t CanvasEditor::layerIndex(int32_t layerId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.layers().indexOf(layerId);
}

bool CanvasEditor::copyLayerThumbnailRGBA(
    int32_t layerId,
    int32_t outW,
    int32_t outH,
    uint8_t* outRGBA,
    int32_t outByteCount
) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.copyLayerThumbnailRGBA(layerId, outW, outH, outRGBA, outByteCount);
}

void CanvasEditor::setTool(ToolMode mode) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().setTool(mode);
}

ToolMode CanvasEditor::tool() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().tool();
}

int32_t CanvasEditor::brushSetCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().setCount();
}

const char* CanvasEditor::brushSetName(int32_t setIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().setName(setIndex);
}

int32_t CanvasEditor::brushSetSource(int32_t setIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return static_cast<int32_t>(impl_->editor.brushes().setSource(setIndex));
}

int32_t CanvasEditor::brushPresetCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().presetCount();
}

int32_t CanvasEditor::brushPresetCountInSet(int32_t setIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().presetCountInSet(setIndex);
}

const char* CanvasEditor::brushPresetName(int32_t index) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().presetName(index);
}

const char* CanvasEditor::brushPresetNameInSet(int32_t setIndex, int32_t presetIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().presetNameInSet(setIndex, presetIndex);
}

bool CanvasEditor::brushPresetApproximated(int32_t setIndex, int32_t presetIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().presetApproximated(setIndex, presetIndex);
}

float CanvasEditor::brushPresetLineWidthInSet(int32_t setIndex, int32_t presetIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const int32_t id = impl_->editor.brushes().presetIdInSet(setIndex, presetIndex);
    const BrushPreset* p = impl_->editor.brushes().find(id);
    return p ? p->lineWidthPx : 16.f;
}

bool CanvasEditor::copyBrushPresetPreviewRGBA(
    int32_t setIndex,
    int32_t presetIndex,
    int32_t outW,
    int32_t outH,
    uint8_t* outRGBA,
    int32_t outByteCount
) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().copyPresetPreviewRGBA(
        setIndex, presetIndex, outW, outH, outRGBA, outByteCount
    );
}

bool CanvasEditor::setBrushPreset(int32_t index) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const int32_t id = impl_->editor.brushes().presetIdAt(index);
    return impl_->editor.brushes().setActivePreset(id);
}

bool CanvasEditor::setBrushPresetInSet(int32_t setIndex, int32_t presetIndex) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const int32_t id = impl_->editor.brushes().presetIdInSet(setIndex, presetIndex);
    return impl_->editor.brushes().setActivePreset(id);
}

int32_t CanvasEditor::activeBrushPresetIndex() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const int32_t id = impl_->editor.brushes().activePresetId();
    for (int32_t i = 0; i < impl_->editor.brushes().presetCount(); ++i) {
        if (impl_->editor.brushes().presetIdAt(i) == id) return i;
    }
    return -1;
}

int32_t CanvasEditor::importBrushPackage(const char* path, BrushPackageKind kind, int32_t* outBrushCount) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (outBrushCount) *outBrushCount = 0;
    const auto result = ::illus::importBrushPackagePath(
        impl_->editor.brushes(), impl_->editor.brushes().assets(), path, kind
    );
    if (outBrushCount) *outBrushCount = result.brushCount;
    return result.setId;
}

int32_t CanvasEditor::importBrushPackageBytes(
    const uint8_t* data,
    int32_t size,
    BrushPackageKind kind,
    const char* suggestedName,
    int32_t* outBrushCount
) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (outBrushCount) *outBrushCount = 0;
    const auto result = ::illus::importBrushPackageBytes(
        impl_->editor.brushes(),
        impl_->editor.brushes().assets(),
        data,
        size,
        kind,
        suggestedName
    );
    if (outBrushCount) *outBrushCount = result.brushCount;
    return result.setId;
}

namespace {

BrushPreset resolvedOrBase(const IllusStudioCanvasEditor& editor) {
    return editor.brushes().resolvedPreset();
}

} // namespace

void CanvasEditor::setBrushLineWidth(float px) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().lineWidthPx = px;
}

float CanvasEditor::brushLineWidth() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).lineWidthPx;
}

void CanvasEditor::setBrushLineSmooth(float s) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().lineSmooth = s;
}

float CanvasEditor::brushLineSmooth() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).lineSmooth;
}

void CanvasEditor::setBrushHardness(float h) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().hardness = h;
}

float CanvasEditor::brushHardness() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).hardness;
}

void CanvasEditor::setBrushOpacity(float a) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().opacity = a;
}

float CanvasEditor::brushOpacity() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).opacity;
}

void CanvasEditor::setBrushFlow(float f) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().flow = f;
}

float CanvasEditor::brushFlow() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).flow;
}

void CanvasEditor::setBrushSpacing(float spacing) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().spacing = spacing;
}

float CanvasEditor::brushSpacing() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).spacing;
}

void CanvasEditor::setBrushSizePressure(float g) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().sizePressure = g;
}

float CanvasEditor::brushSizePressure() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).sizePressure;
}

void CanvasEditor::setBrushOpacityPressure(float g) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().opacityPressure = g;
}

float CanvasEditor::brushOpacityPressure() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return resolvedOrBase(impl_->editor).opacityPressure;
}

void CanvasEditor::setBrushColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().session().color = math::RGBA{r, g, b, a};
}

void CanvasEditor::resetBrushSession() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.brushes().resetSession();
}

int32_t CanvasEditor::saveBrushSessionAsPreset(const char* name) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.brushes().saveSessionAsPreset(name);
}

void CanvasEditor::beginStroke(float x, float y, float pressure) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.beginStroke(x, y, pressure);
}

void CanvasEditor::continueStroke(float x, float y, float pressure) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.continueStroke(x, y, pressure);
}

void CanvasEditor::endStroke() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.endStroke();
}

int32_t CanvasEditor::strokeCountOnLayer(int32_t layerId) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.strokeCountOnLayer(layerId);
}

void CanvasEditor::setViewport(float scale, float offsetX, float offsetY) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.setViewport(scale, offsetX, offsetY);
}

float CanvasEditor::viewportScale() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.viewport().scale;
}

float CanvasEditor::viewportOffsetX() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.viewport().offsetX;
}

float CanvasEditor::viewportOffsetY() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.viewport().offsetY;
}

float CanvasEditor::viewToCanvasX(float viewX, float viewY, float viewW, float viewH) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.viewToCanvasX(viewX, viewY, viewW, viewH);
}

float CanvasEditor::viewToCanvasY(float viewX, float viewY, float viewW, float viewH) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.viewToCanvasY(viewX, viewY, viewW, viewH);
}

float CanvasEditor::canvasToViewX(float canvasX, float canvasY, float viewW, float viewH) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.canvasToViewX(canvasX, canvasY, viewW, viewH);
}

float CanvasEditor::canvasToViewY(float canvasX, float canvasY, float viewW, float viewH) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.canvasToViewY(canvasX, canvasY, viewW, viewH);
}

bool CanvasEditor::copyPresentNdcRect(float viewW, float viewH, float* out4, int32_t outCount) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!out4 || outCount < 4) return false;
    impl_->editor.presentNdcRect(viewW, viewH, out4);
    return true;
}

std::uintptr_t CanvasEditor::presentMetalTextureAddress() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return reinterpret_cast<std::uintptr_t>(impl_->editor.presentMetalTexture());
}

std::uintptr_t CanvasEditor::metalDeviceAddress() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return reinterpret_cast<std::uintptr_t>(impl_->editor.metalDevice());
}

bool CanvasEditor::metalAvailable() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.metalAvailable();
}

void CanvasEditor::setTargetPresentFps(int32_t fps) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->editor.setTargetPresentFps(fps);
}

int32_t CanvasEditor::targetPresentFps() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->editor.targetPresentFps();
}

bool CanvasEditor::selfCheck() {
    return IllusStudioCanvasEditor::selfCheck();
}

const char* CanvasEditor::version() {
    return "IllusStudioFramework 0.7.0-cxx";
}

} // namespace illus
