# IllusStudioFramework — Public API

Quick index of the Swift-facing C++ surface. **Source of truth:** [`CanvasEditor.hpp`](../CanvasEditor.hpp).  
Import via `import IllusStudioFramework` (umbrella includes this header).

Related: [README.md](../README.md) · [ROADMAP.md](ROADMAP.md) · [canvas_document.md](canvas_document.md) · [layer.md](layer.md) · [brush_drawing.md](brush_drawing.md)

**Namespace:** `illus`  
**Types:** `ToolMode`, `BrushPackageKind`, `CanvasEditor`  
**Version string:** `IllusStudioFramework 0.7.0-cxx` (`CanvasEditor::version()`)

---

## Enums

| Name | Values | Notes |
|------|--------|-------|
| `ToolMode` | `Brush = 0`, `Eraser = 1`, `Pointer = 2` | Pointer does not paint |
| `BrushPackageKind` | `Auto = 0`, `Brush = 1`, `BrushSet = 2`, `BrushLibrary = 3` | Procreate-style package sniff / force |

---

## Lifecycle / document

| API | Returns | Notes |
|-----|---------|-------|
| `CanvasEditor(width, height)` | — | Create editor (shared ownership; Swift can copy) |
| `~CanvasEditor()` | — | |
| `width()` | `int32_t` | Page width (px) |
| `height()` | `int32_t` | Page height (px) |
| `setBackground(r, g, b, a)` | `void` | Fill Background Layer (page underlay stays transparent) |
| `clearAll(r, g, b, a)` | `void` | Clear paint layers; refill Background Layer |
| `clearActiveLayer()` | `void` | Clear pixels + strokes on active layer |

---

## Layers

| API | Returns | Notes |
|-----|---------|-------|
| `layerCount()` | `int32_t` | |
| `layerIdAt(index)` | `int32_t` | Index 0 = front; `-1` if invalid |
| `layerName(layerId)` | `const char*` | Borrowed C string |
| `addLayer(name)` | `int32_t` | New layer id |
| `removeLayer(layerId)` | `bool` | |
| `setActiveLayer(layerId)` | `bool` | |
| `activeLayerId()` | `int32_t` | |
| `setLayerOpacity(layerId, opacity)` | `bool` | `opacity` typically 0…1 |
| `layerOpacity(layerId)` | `float` | |
| `setLayerVisible(layerId, visible)` | `bool` | |
| `layerVisible(layerId)` | `bool` | |
| `layerIsBackground(layerId)` | `bool` | Document background layer (not removable) |
| `duplicateLayer(layerId)` | `int32_t` | New id, or fail |
| `moveLayer(layerId, toIndex)` | `bool` | Reorder in stack |
| `mergeLayerDown(srcId, dstId)` | `bool` | Merge src into dst |
| `layerIndex(layerId)` | `int32_t` | Stack index |
| `copyLayerThumbnailRGBA(layerId, outW, outH, outRGBA, outByteCount)` | `bool` | Nearest downsample to RGBA8 thumb |

Spec: [layer.md](layer.md). Blend-mode getters/setters: planned ([T0-12](ROADMAP.md#t0-12--layer-blending-modes-procreate-style)).

---

## Tools

| API | Returns | Notes |
|-----|---------|-------|
| `setTool(mode)` | `void` | `Brush` / `Eraser` / `Pointer` |
| `tool()` | `ToolMode` | |

---

## Brush library & session

| API | Returns | Notes |
|-----|---------|-------|
| `brushSetCount()` | `int32_t` | |
| `brushSetName(setIndex)` | `const char*` | Borrowed C string |
| `brushSetSource(setIndex)` | `int32_t` | `0` BuiltIn, `1` User, `2` ImportedProcreate |
| `brushPresetCount()` | `int32_t` | Flat list |
| `brushPresetCountInSet(setIndex)` | `int32_t` | |
| `brushPresetName(index)` | `const char*` | Flat index |
| `brushPresetNameInSet(setIndex, presetIndex)` | `const char*` | Name within set |
| `brushPresetApproximated(setIndex, presetIndex)` | `bool` | Imported preset with incomplete map |
| `setBrushPreset(index)` | `bool` | Flat index |
| `setBrushPresetInSet(setIndex, presetIndex)` | `bool` | |
| `activeBrushPresetIndex()` | `int32_t` | Flat index |
| `setBrushLineWidth(px)` / `brushLineWidth()` | — / `float` | Pre-draw session |
| `setBrushLineSmooth(s)` / `brushLineSmooth()` | — / `float` | 0…1 |
| `setBrushHardness(h)` / `brushHardness()` | — / `float` | |
| `setBrushOpacity(a)` / `brushOpacity()` | — / `float` | |
| `setBrushFlow(f)` / `brushFlow()` | — / `float` | |
| `setBrushSpacing(spacing)` / `brushSpacing()` | — / `float` | |
| `setBrushSizePressure(g)` / `brushSizePressure()` | — / `float` | |
| `setBrushOpacityPressure(g)` / `brushOpacityPressure()` | — / `float` | |
| `setBrushColor(r, g, b, a)` | `void` | Session color |
| `resetBrushSession()` | `void` | Clear overrides → active preset |
| `saveBrushSessionAsPreset(name)` | `int32_t` | New preset id, or fail |

### Procreate-style import

| API | Returns | Notes |
|-----|---------|-------|
| `importBrushPackage(path, kind, outBrushCount)` | `int32_t` | New set id, or `-1`; `outBrushCount` optional |
| `importBrushPackageBytes(data, size, kind, suggestedName, outBrushCount)` | `int32_t` | Same; in-memory / AirDrop |

Spec: [brush_drawing.md](brush_drawing.md) § Procreate-style brush import · ROADMAP [T1-7](ROADMAP.md#t1-7--procreate-style-brush-import)

---

## Strokes (canvas space)

Points are **canvas pixels**, not view space. Map with viewport helpers first.

| API | Returns | Notes |
|-----|---------|-------|
| `beginStroke(x, y, pressure)` | `void` | No-op in `Pointer` tool |
| `continueStroke(x, y, pressure)` | `void` | |
| `endStroke()` | `void` | Commits vector stroke + raster |
| `strokeCountOnLayer(layerId)` | `int32_t` | Query only; hit-test / edit APIs planned (T2-6) |

---

## Viewport (zoom / pan)

Present-time transform only — does **not** re-raster strokes.  
`scale` is relative to fit-to-view (`1` = fit). Offsets are canvas pixels.

| API | Returns | Notes |
|-----|---------|-------|
| `setViewport(scale, offsetX, offsetY)` | `void` | Clamped scale |
| `viewportScale()` | `float` | |
| `viewportOffsetX()` | `float` | |
| `viewportOffsetY()` | `float` | |
| `viewToCanvasX(viewX, viewY, viewW, viewH)` | `float` | View → canvas |
| `viewToCanvasY(viewX, viewY, viewW, viewH)` | `float` | |
| `canvasToViewX(canvasX, canvasY, viewW, viewH)` | `float` | Canvas → view |
| `canvasToViewY(canvasX, canvasY, viewW, viewH)` | `float` | |
| `copyPresentNdcRect(viewW, viewH, out4, outCount)` | `bool` | Scalar NDC xmin/ymin/xmax/ymax; for tests / rotate prep — UI present caches viewport instead |

Spec: [canvas_document.md](canvas_document.md) § Zoom & pan · Math policy: [ROADMAP TX-7](ROADMAP.md#tx-7--math-libraries-glm--eigen)

---

## Present (Metal)

| API | Returns | Notes |
|-----|---------|-------|
| `presentMetalTextureAddress()` | `uintptr_t` | Borrowed `MTLTexture*`; `0` if unavailable |
| `metalDeviceAddress()` | `uintptr_t` | Borrowed `MTLDevice*` — use for `MTKView` |
| `metalAvailable()` | `bool` | |
| `setTargetPresentFps(fps)` | `void` | Cap GPU composite rebuilds to this Hz (`0` = uncapped); UI sends adaptive pick (72 or 120) |
| `targetPresentFps()` | `int32_t` | Current present-FPS cap |

UI also owns idle present (~5fps after 30s) — see [canvas_document.md](canvas_document.md) § App active status. That gate is **not** on `CanvasEditor`.

---

## Diagnostics

| API | Returns | Notes |
|-----|---------|-------|
| `selfCheck()` | `bool` | Static; engine smoke tests |
| `version()` | `const char*` | Static; currently `IllusStudioFramework 0.7.0-cxx` |

---

## Not public yet (planned)

| Area | Spec / roadmap |
|------|----------------|
| Gizmo mode (`None` / `Vector`) | [canvas_document.md](canvas_document.md) · ROADMAP T2-6-0 |
| Layer blend modes | [layer.md](layer.md) · ROADMAP T0-12 |
| Undo / redo / timelapse | [history.md](history.md) · ROADMAP T3 |
| Import / export (PNG, SVG, TIFF) | [canvas_document.md](canvas_document.md) · ROADMAP T4 |
| Stroke hit-test / move / adjust | [brush_drawing.md](brush_drawing.md) · ROADMAP T2-6 |
| Animation / timeline | [animation_timeline.md](animation_timeline.md) · ROADMAP T5 |
| Photoshop `.abr` import | [brush_drawing.md](brush_drawing.md) · ROADMAP T1-7-7 |

**UI-only (not on `CanvasEditor`):** App active status / idle present — implemented in DrawingEditor (`AppActiveStatus.swift`).

Do **not** call internal `src/` types from Swift — only `illus::CanvasEditor`.
