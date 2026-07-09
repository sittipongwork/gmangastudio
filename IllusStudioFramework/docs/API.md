# IllusStudioFramework — Public API

Quick index of the Swift-facing C++ surface. **Source of truth:** [`CanvasEditor.hpp`](../CanvasEditor.hpp).  
Import via `import IllusStudioFramework` (umbrella includes this header).

Related: [README.md](../README.md) · [ROADMAP.md](ROADMAP.md) · [canvas_document.md](canvas_document.md) · [layer.md](layer.md) · [brush_drawing.md](brush_drawing.md)

**Namespace:** `illus`  
**Types:** `ToolMode`, `CanvasEditor`

---

## Enums

| Name | Values | Notes |
|------|--------|-------|
| `ToolMode` | `Brush = 0`, `Eraser = 1`, `Pointer = 2` | Pointer does not paint |

---

## Lifecycle / document

| API | Returns | Notes |
|-----|---------|-------|
| `CanvasEditor(width, height)` | — | Create editor (shared ownership; Swift can copy) |
| `~CanvasEditor()` | — | |
| `width()` | `int32_t` | Page width (px) |
| `height()` | `int32_t` | Page height (px) |
| `setBackground(r, g, b, a)` | `void` | Page clear / background RGBA |
| `clearAll(r, g, b, a)` | `void` | Clear document + set background |
| `clearActiveLayer()` | `void` | Clear pixels + strokes on active layer |

---

## Layers

| API | Returns | Notes |
|-----|---------|-------|
| `layerCount()` | `int32_t` | |
| `addLayer(name)` | `int32_t` | New layer id |
| `removeLayer(layerId)` | `bool` | |
| `setActiveLayer(layerId)` | `bool` | |
| `activeLayerId()` | `int32_t` | |
| `setLayerOpacity(layerId, opacity)` | `bool` | `opacity` typically 0…1 |
| `layerOpacity(layerId)` | `float` | |
| `setLayerVisible(layerId, visible)` | `bool` | |
| `layerVisible(layerId)` | `bool` | |
| `duplicateLayer(layerId)` | `int32_t` | New id, or fail |
| `moveLayer(layerId, toIndex)` | `bool` | Reorder in stack |
| `mergeLayerDown(srcId, dstId)` | `bool` | Merge src into dst |
| `layerIndex(layerId)` | `int32_t` | Stack index |

Spec: [layer.md](layer.md)

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
| `brushPresetCount()` | `int32_t` | Flat list |
| `brushPresetCountInSet(setIndex)` | `int32_t` | |
| `brushPresetName(index)` | `const char*` | Flat index |
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

Spec: [brush_drawing.md](brush_drawing.md)

---

## Strokes (canvas space)

Points are **canvas pixels**, not view space. Map with viewport helpers first.

| API | Returns | Notes |
|-----|---------|-------|
| `beginStroke(x, y, pressure)` | `void` | No-op in `Pointer` tool |
| `continueStroke(x, y, pressure)` | `void` | |
| `endStroke()` | `void` | Commits vector stroke + raster |
| `strokeCountOnLayer(layerId)` | `int32_t` | |

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

Spec: [canvas_document.md](canvas_document.md) § Zoom & pan

---

## Present (Metal)

| API | Returns | Notes |
|-----|---------|-------|
| `presentMetalTextureAddress()` | `uintptr_t` | Borrowed `MTLTexture*`; `0` if unavailable |
| `metalDeviceAddress()` | `uintptr_t` | Borrowed `MTLDevice*` — use for `MTKView` |
| `metalAvailable()` | `bool` | |

---

## Diagnostics

| API | Returns | Notes |
|-----|---------|-------|
| `selfCheck()` | `bool` | Static; engine smoke tests |
| `version()` | `const char*` | Static; version string |

---

## Not public yet (planned)

| Area | Spec / roadmap |
|------|----------------|
| Gizmo mode (`None` / `Vector`) | [canvas_document.md](canvas_document.md) · ROADMAP T1-5-0 |
| App active status (UI idle → low_energy_mode) | [canvas_document.md](canvas_document.md) § App active status |
| Undo / redo / timelapse | [history.md](history.md) · ROADMAP T3 |
| Import / export (PNG, SVG, TIFF) | [canvas_document.md](canvas_document.md) · ROADMAP T4 |
| Stroke hit-test / move / adjust | [brush_drawing.md](brush_drawing.md) · ROADMAP T1-5 |
| Animation / timeline | [animation_timeline.md](animation_timeline.md) · ROADMAP T5 |
| Procreate brush import | [brush_drawing.md](brush_drawing.md) · ROADMAP T1-7 |

Do **not** call internal `src/` types from Swift — only `illus::CanvasEditor`.
