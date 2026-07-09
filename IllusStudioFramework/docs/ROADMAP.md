# IllusStudioFramework — Roadmap & tasks

Single source of truth for **what to build** and **done vs open**.  
Architecture / design: [README.md](../README.md) · Specs: [canvas_document.md](canvas_document.md) · [layer.md](layer.md) · [brush_drawing.md](brush_drawing.md) · [history.md](history.md) · [animation_timeline.md](animation_timeline.md) · App: [AGENTS.md](../../AGENTS.md)

**Status:** `[x]` done · `[ ]` not done  
**Ids:** `T#` epic · `T#-#` task · `T#-#-#` subtask

---

## Overview

| Id | Epic | Status |
|----|------|--------|
| [T0](#t0--foundation-document--layers) | Foundation (document + layers + CPU composite) | done |
| [T1](#t1--hybrid-brush-library--eraser) | Hybrid brush library & eraser (+ session props, move/adjust, Procreate import) | T1-1 done |
| [T2](#t2--viewport-zoom--pan) | Viewport (zoom / pan) | open |
| [T3](#t3--history-undo--redo--timelapse) | History (undo / redo / timelapse) | open |
| [T4](#t4--import--export) | Import & export (PNG / TIFF / SVG) | open |
| [T5](#t5--animation--timeline) | Animation & timeline | open |
| [T6](#t6--metal-present-120hz) | Metal present @ 120Hz | done |

---

## T0 — Foundation (document + layers)

Former **P0**.

- [x] **T0-1** `document/` — `PageSettings` (width, height, background RGBA)
- [x] **T0-2** `layers/` — stack, active layer, add/remove, lazy RGBA buffers
- [x] **T0-3** `render/SoftwareRenderer` — CPU composite + below-cache + dirty rect
- [x] **T0-4** `math/Blend` — src-over (and helpers)
- [x] **T0-5** Public `illus::CanvasEditor` (Swift–C++ interop, pimpl, mutex)
- [x] **T0-6** Soft brush stamp on active layer (CPU) — interim until T1
- [x] **T0-7** `CanvasEditor::selfCheck()` entry
- [x] **T0-8** Layer opacity / visibility API on public `CanvasEditor` (internal stack exists)
- [x] **T0-9** Layer reorder / duplicate / merge (v1 polish)

**v1 out:** layer groups, masks, full blend-mode catalog.

---

## T1 — Hybrid brush library & eraser

Former **P1** (tools) + [brush_drawing.md](brush_drawing.md). Vector source of truth → raster cache → GPU display.

### T1-1 — Vector stroke + CPU raster + pre-draw brush props

- [x] **T1-1-1** `Stroke` / `StrokeSample` / per-layer `LayerStrokeList`
- [x] **T1-1-2** `beginStroke` / `continueStroke` / `endStroke` append samples (not pixels-only)
- [x] **T1-1-3** `StrokeRasterizer` CPU path (migrate `stamp` / `stampLine`)
- [x] **T1-1-4** Eraser mode — dest-out on active layer; erase as vector `Stroke`
- [x] **T1-1-5** `BrushPreset` + `BrushLibrary` + `BrushSet` (built-ins: ink.round, air.soft, erase.soft, erase.hard)
- [x] **T1-1-6** `BrushSession` — pre-draw overrides: **lineWidth**, **lineSmooth**, hardness, opacity, flow, spacing, pressure gains, color
- [x] **T1-1-7** Public API: `setTool` / `setBrushPreset` / session getters-setters / `resetBrushSession` / optional `saveBrushSessionAsPreset`
- [x] **T1-1-8** Apply `lineSmooth` as input filter before samples commit; snapshot session at `beginStroke`
- [x] **T1-1-9** Self-check: paint/erase; lineWidth affects dab; mid-stroke session change ignored; stroke count += 1

### T1-2 — Live overlay + dirty tiles (P1b)

- [ ] **T1-2-1** Live stroke overlay buffer/texture (layer + overlay while stroking)
- [ ] **T1-2-2** Merge overlay → layer on `endStroke`
- [ ] **T1-2-3** Tile dirty tracking for upload / compute

### T1-3 — Metal compute rasterizer (P1c)

- [ ] **T1-3-1** Compute kernel: dab/segment coverage → layer texture
- [ ] **T1-3-2** Keep CPU path for `selfCheck` / headless
- [ ] **T1-3-3** Measure 120Hz stroke on 1920×1080 multi-layer

### T1-4 — GPU layer composite (P1d)

- [ ] **T1-4-1** Per-layer `MTLTexture` cache
- [ ] **T1-4-2** `LayerCompositor` — blend stack on GPU
- [ ] **T1-4-3** Retire full-document CPU composite on hot present path (keep for export until T4)

### T1-5 — Easy move / adjust line (P1e)

Layer-scoped vector query + edit. Depends on **T1-1**.

- [ ] **T1-5-1** Query: `strokeCountOnLayer` / `strokeIdAt` / `strokeBounds`
- [ ] **T1-5-2** `copyStrokePolyline` (Swift-friendly buffer; no `std::vector` in public API)
- [ ] **T1-5-3** `hitTestStroke(layerId, x, y, radius)` — that layer only
- [ ] **T1-5-4** `translateStroke` + `setStrokePoint` (or batch polyline set)
- [ ] **T1-5-5** Invalidate union(old, new) bounds → re-raster one layer
- [ ] **T1-5-6** Self-check: translate moves bounds; other layers unchanged; wrong `layerId` cannot see stroke
- [ ] **T1-5-7** Swift DrawingEditor: select / drag move / point handles (active layer only)

### T1-6 — Hybrid follow-ups

- [ ] **T1-6-1** Optional Bézier fit on `endStroke` (`math/Bezier`) for storage / SVG
- [ ] **T1-6-2** Tilt-aware `beginStrokeEx` / `continueStrokeEx` when UI ready
- [ ] **T1-6-3** Stroke→tile index if undo/re-raster becomes hot

### T1-7 — Procreate-style brush import

Import `.brush` / `.brushset` (later `.brushlibrary`) into `BrushLibrary`. Design: [brush_drawing.md](brush_drawing.md) § Procreate-style brush import. Best-effort map — not 1:1 engine parity.

- [ ] **T1-7-1** Unzip package; discover brush folders; store tip/grain/preview PNGs in `BrushAssetStore`; create `BrushSet`
- [ ] **T1-7-2** Decode `Brush.archive` (bplist / NSKeyedArchiver shim); map → `lineWidth`, `lineSmooth`, hardness, opacity, spacing, pressure gains
- [ ] **T1-7-3** `StrokeRasterizer` tip-texture stamp (CPU then Metal); grain multiply optional after
- [ ] **T1-7-4** Public `importBrushPackage` / `importBrushPackageBytes` + set listing APIs
- [ ] **T1-7-5** Swift DrawingEditor: Import UI, Imported set, “approximated” badge
- [ ] **T1-7-6** `.brushlibrary` as multi-set; fixture self-check in repo test resources
- [ ] **T1-7-7** (Later) Photoshop `.abr` import if still needed

**v1 out (T1):** dual brush, smudge, wet mix parity, multi-stroke lasso, document-wide stroke dump, Illustrator-grade Bézier UI, shipping Procreate’s default packs, claiming full Procreate compatibility.

---

## T2 — Viewport (zoom & pan)

Former **P1** viewport half.

- [ ] **T2-1** `viewport/` — `Viewport { scale, offset }`
- [ ] **T2-2** Public `setViewport` + view↔canvas helpers
- [ ] **T2-3** Tools receive **canvas-space** points only (UI or engine maps)
- [ ] **T2-4** Present applies viewport as transform (no re-raster on pan)
- [ ] **T2-5** Self-check: view→canvas→view round-trip within epsilon

**v1 out:** rotate canvas, snap-to-pixel UI chrome.

---

## T3 — History (undo / redo / timelapse)

Former **P2**. Design hooks land with T1; full stack here.

- [ ] **T3-1** `history/` command stack
- [ ] **T3-2** `StrokeCommand` on `endStroke`
- [ ] **T3-3** `TransformStrokeCommand` / `EditStrokePointCommand` (from T1-5)
- [ ] **T3-4** `LayerCommand` (add/remove/reorder/…)
- [ ] **T3-5** Public `undo` / `redo` / `canUndo` / `canRedo`
- [ ] **T3-6** Timelapse append-only op log + timestamps
- [ ] **T3-7** Self-check: undo restores prior active-layer / stroke-list hash

**v1 out:** branching history, cloud sync of op logs.

---

## T4 — Import & export

Former **P3**.

### T4-1 — Image import

- [ ] **T4-1-1** `import/` — place decoded RGBA onto layer + transform
- [ ] **T4-1-2** Public `importRGBA(w, h, pixels, placement)`
- [ ] **T4-1-3** New layer or replace active

### T4-2 — Export PNG

- [ ] **T4-2-1** Flattened RGBA composite → lossless PNG (alpha)
- [ ] **T4-2-2** Public `export(format, options, …)` — PNG first
- [ ] **T4-2-3** Self-check: tiny canvas PNG starts with signature

### T4-3 — Export TIFF

- [ ] **T4-3-1** Flattened composite → TIFF (RGBA, optional DPI tags)

### T4-4 — Export SVG

- [ ] **T4-4-1** Hybrid SVG: layer groups; embed raster or true paths when vector strokes exist
- [ ] **T4-4-2** Self-check: non-empty / parseable SVG

**Rules:** UI owns pickers; engine owns encode; export uses **document** pixels, not viewport zoom.

**v1 out:** Vector/PDF import, camera pipeline, PSD/PDF, multi-page TIFF, GIF/APNG, CMYK TIFF.

---

## T5 — Animation & timeline

Former **P4**.

- [ ] **T5-1** `animation/` — frames / cels, fps, playhead
- [ ] **T5-2** Onion-skin range data
- [ ] **T5-3** Public timeline APIs on `CanvasEditor`
- [ ] **T5-4** Paint goes through tools on current frame’s layers
- [ ] **T5-5** Keep paint undo vs timeline undo separate (document in API)

**v1 out:** audio scrub, complex tweening, GIF/MP4 export polish.

---

## T6 — Metal present (120Hz)

Former **P5**.

- [x] **T6-1** Vendored `third_party/metal-cpp`
- [x] **T6-2** `render/MetalRenderer` — shared RGBA8 texture, dirty `replaceRegion`
- [x] **T6-3** `presentMetalTextureAddress` / `metalDeviceAddress` / `metalAvailable`
- [x] **T6-4** `CanvasMetalView` MTKView @ 120fps; shared device with engine
- [x] **T6-5** Mutex on `CanvasEditor` for draw vs gesture
- [x] **T6-6** CPU `SoftwareRenderer` kept for composite / self-check / fallback

**Ongoing (covered by T1-3 / T1-4):** compute raster + GPU layer blend on top of this present path.

---

## Cross-cutting

- [x] **TX-1** No C bridge — Swift–C++ interop only (`SWIFT_OBJC_INTEROP_MODE = objcxx`)
- [x] **TX-2** Prefer `uintptr_t` over `void*` for Metal handles in public API
- [x] **TX-3** Lazy layer buffers (allocate on first paint; Clear frees)
- [ ] **TX-4** SIMD / Accelerate only after profiler evidence
- [ ] **TX-5** Sustain 120Hz under heavy layer stacks (verify after T1-3 / T1-4)

---

## Suggested order

```text
T0 (done) → T6 (done)
         → T1-1 (vector + BrushSession props)
         → T1-5 (move/adjust) → T1-2 → T1-3 → T1-4
         → T1-7 (Procreate import; T1-7-1 can start after T1-1-5; tip stamp after T1-1-3)
         → T2 (viewport; can parallel T1 after T1-1)
         → T3 (history; needs T1-1 stroke list)
         → T4 → T5
```

---

## How to update this file

1. Flip `[ ]` → `[x]` when the task is done in code + self-check where required.
2. Add new work as the next free `T#-#` / `T#-#-#` under the right epic.
3. Keep design detail in feature docs ([canvas_document.md](canvas_document.md), [layer.md](layer.md), [brush_drawing.md](brush_drawing.md), [history.md](history.md), [animation_timeline.md](animation_timeline.md)) / [README.md](../README.md); keep **status only** here.
