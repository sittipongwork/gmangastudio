# IllusStudioFramework — IllusStudioCanvasEditor Architecture

C++ canvas engine for gmangastudio. Swift UI lives in page COPs (`DrawingEditor`); this framework owns document, layers, tools, viewport, history, animation, and rendering. See [AGENTS.md](../AGENTS.md) for the UI ↔ Bridge ↔ Engine contract.

**Tasks & roadmap (status):** [docs/ROADMAP.md](docs/ROADMAP.md)  
**Public API index:** [docs/API.md](docs/API.md)  
**Feature specs:** [canvas_document.md](docs/canvas_document.md) · [layer.md](docs/layer.md) · [brush_drawing.md](docs/brush_drawing.md) · [history.md](docs/history.md) · [animation_timeline.md](docs/animation_timeline.md) · [AI_Integration.md](docs/AI_Integration.md)

## Purpose

Build **IllusStudioCanvasEditor** (Procreate-style drawing editor) core:

- Page settings, layers, brushes/eraser, image import
- Zoom/pan, undo/redo, timelapse recording
- Animation + timeline
- Export image formats: **PNG**, **SVG**, **TIFF**
- Hybrid vector + raster drawing; Metal (metal-cpp) for present / GPU paths

Swift UI uses the public C++ API via Swift–C++ interop (`SWIFT_OBJC_INTEROP_MODE = objcxx`). No C bridge.

```text
+-------------------------------------------------------+
|                 UI Layer (Swift / SwiftUI)            |  <-- Xcode Managed
+-------------------------------------------------------+
                           │ ▲
                           ▼ │  (Swift–C++ interop)
+-------------------------------------------------------+
|          Public C++ API (`illus::CanvasEditor`)       |
+-------------------------------------------------------+
                           │ ▲
                           ▼ │
+-------------------------------------------------------+
|     IllusStudioCanvasEditor / Core (C++)              |  <-- This framework
+-------------------------------------------------------+
```

## Target architecture

```mermaid
flowchart TB
  subgraph ui [UI_Swift]
    DrawingEditorView
    DrawingEditorViewModel
  end
  subgraph api [Public_Cpp_API]
    CanvasEditor
  end
  subgraph core [IllusStudioCanvasEditor_Cpp]
    Document
    Layers
    Tools
    Viewport
    History
    Animation
    Renderer
    Export
  end
  DrawingEditorView --> DrawingEditorViewModel
  DrawingEditorViewModel --> CanvasEditor
  CanvasEditor --> Document
  Document --> Layers
  Tools --> Layers
  Viewport --> Renderer
  History --> Document
  Animation --> Document
  Renderer --> Layers
  Export --> Renderer
  Export --> Layers
```

**Public facade:** `illus::CanvasEditor`  
**Internal facade:** `illus::IllusStudioCanvasEditor`

Session entry for: open/create document, pointer events (canvas space), tool select, viewport, undo/redo, composite/export frame (PNG/SVG/TIFF), timeline ops.

Swift ViewModels call `CanvasEditor` only; they do not include internal `src/` headers.

## Module map (planned)

```text
IllusStudioFramework/
  IllusStudioFramework.h      Umbrella (C++)
  CanvasEditor.hpp            Public C++ API
  module.modulemap            requires cplusplus
  README.md                   Architecture (this file)
  docs/
    ROADMAP.md                Tasks & status (single checklist)
    API.md                    Public CanvasEditor API index
    canvas_document.md        Page setting, zoom/pan, export
    layer.md                  Layer management
    brush_drawing.md          Brush / eraser / hybrid drawing + image import
    history.md                Undo / redo / timelapse
    animation_timeline.md     Animation & timeline
    AI_Integration.md         AI-assisted features (reference → line-art, …)
  src/
    CanvasEditor.cpp          Public API pimpl
    IllusStudioCanvasEditor.hpp/.cpp   Internal facade
    document/                 Page size, background, metadata
    layers/                   Stack, opacity, blend, active layer
    tools/                    Brush library, stroke engine, eraser
    strokes/                  Vector Stroke / edit (planned)
    import/                   Raster import onto layers
    export/                   PNG / SVG / TIFF writers
    viewport/                 Zoom, pan, transforms, dirty rects
    history/                  Undo/redo commands + timelapse op log
    animation/                Frames, timeline, onion-skin data
    render/                   SoftwareRenderer + MetalRenderer
    math/                     Vec/mat, curves, dab spacing, blend helpers
  third_party/
    metal-cpp/                Vendored Apple metal-cpp
```

| Module | Owns | Does not own |
|--------|------|--------------|
| `CanvasEditor` | Swift-facing C++ surface | Internal headers |
| `document/` | `PageSettings`, document lifetime, background | UI chrome |
| `layers/` | Ordered stack, visibility/opacity/blend, active id, reorder/merge | Gestures |
| `tools/` | Brush presets, stroke dabs, eraser mode | Screen-space input |
| `strokes/` | Vector stroke list, hit-test, move/adjust | UI selection chrome |
| `import/` | Place decoded RGBA into a layer + transform | File pickers (UI) |
| `export/` | Encode composite (and optional per-layer) to PNG, SVG, TIFF | Save panels / sharing UI |
| `viewport/` | Scale, offset, canvas↔view maps, dirty rects | SwiftUI MagnifyGesture |
| `history/` | Command stacks, timelapse event stream | Full video encode |
| `animation/` | Cels/frames, playhead, fps, timeline ops | Paint undo stack (separate) |
| `render/` | Composite → presentable buffer/texture | Windowing |
| `math/` | Hot-path math, premultiplied blend | Third-party deps by default |

## Feature specs

Detailed specs live under `docs/` — expand those files, not this README.  
**Task ids / completion:** [docs/ROADMAP.md](docs/ROADMAP.md) only.

| Feature | Doc |
|---------|-----|
| Canvas page setting, zoom/pan, export | [docs/canvas_document.md](docs/canvas_document.md) |
| Layer management | [docs/layer.md](docs/layer.md) |
| Brush library, eraser, image import | [docs/brush_drawing.md](docs/brush_drawing.md) |
| History (undo / redo / timelapse) | [docs/history.md](docs/history.md) |
| Animation & timeline | [docs/animation_timeline.md](docs/animation_timeline.md) |
| AI integration | [docs/AI_Integration.md](docs/AI_Integration.md) |

## Performance notes

### Metal present

- Vendored under `third_party/metal-cpp`.
- `render/MetalRenderer`: shared `RGBA8Unorm` texture; dirty-rect `replaceRegion` upload from CPU composite.
- Public API: `presentMetalTextureAddress()` / `metalDeviceAddress()` / `metalAvailable()`.
- UI: `CanvasMetalView` (`MTKView`, `preferredFramesPerSecond = 120`) must use the **engine device**.
- `SoftwareRenderer` remains for composite math, self-check, and fallback.

### Image processing & math

- Hot paths live in `math/`: float/premultiplied RGBA, blend, dab spacing, viewport (**scalar**).
- **Vendored math libs** (keep; gated use — [docs/ROADMAP.md](docs/ROADMAP.md) TX-7 best-use table + [cpp-math-libs skill](../.cursor/skills/cpp-math-libs/SKILL.md)):
  - `third_party/eigen/` (**5.0.1**) — `math/Bezier` least-squares; **lazy** on export (`ensureStrokeCubics`), never under `endStroke` mutex
  - `third_party/glm/` (**1.0.3**) — `presentModelMatrix` / canvas rotate later; **axis-aligned present stays scalar**
- No GLM/Eigen types in public `CanvasEditor.hpp`.
- Microbench: `tools/tx7_math_bench.cpp`.
- Optional later: SIMD / Accelerate — only after CPU profiler evidence ([TX-4](docs/ROADMAP.md)).

### Display refresh (120fps)

- Sustain presents up to **120Hz** while stroking; UI must not throttle below `1/120` s.
- Dirty-rect + below-cache (and later GPU layer blend) keep the budget feasible.

## Public API notes (Swift–C++ interop)

- Expose only `CanvasEditor.hpp` (+ umbrella). Keep `std::vector` / internal types out of the public header.
- Prefer `std::uintptr_t` over `void*` for Metal handles (Swift imports the former).
- App target: `SWIFT_OBJC_INTEROP_MODE = objcxx`.

## Self-check rule

Non-trivial modules leave **one** runnable check that fails if the core invariant breaks. Examples:

- Layers: composite of two opaque layers matches expected pixel
- Stroke: dab darkens active layer only
- History: undo restores prior active-layer hash
- Viewport: view→canvas→view round-trip within epsilon
- Export: tiny canvas → PNG bytes start with signature; TIFF/SVG non-empty and parseable

`CanvasEditor::selfCheck()` is the Swift-callable entry.

## Out of scope (framework-wide, for now)

- Full Procreate-style feature parity (liquify, complex blend catalogs, cloud)
- SwiftUI screens (those stay in `gmangastudio/DrawingEditor/`)
- New package-manager dependencies without an explicit ask
