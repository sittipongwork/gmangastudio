# GDrawingStudio — Feature & Development Plan

Professional digital painting for **Mac and iPad**, aimed at comic artists, illustrators,
concept artists, matte/texture painters, and VFX. UI in **SwiftUI**, engine in
**GDrawingStudioFramework (C++)** via Swift–C++ interop (`objcxx`).

Reference: [Krita](https://github.com/KDE/krita) — we borrow its *architecture and
feature taxonomy*, not its code (GPL) and not its Qt/desktop assumptions.

---

## 1. What we learn from Krita

### 1.1 Architecture (the parts worth copying)

Krita is built as a small number of core libraries plus plugins:

| Krita | Responsibility | Our equivalent |
|---|---|---|
| `pigment` | Color spaces, pixel traits, compositing ops | `gdrawing::color` (C++) — thin; lean on Apple **ColorSync/CGColorSpace** for ICC, keep only pixel-level composite math in C++ |
| `kritaimage` | Tiled pixel storage, layer tree, painters, filters, undo | `gdrawing::image` (C++) — the heart of the framework |
| tiles3 (`KisTiledDataManager`) | 64×64 tiles, copy-on-write, hash-table lookup, swap | `gdrawing::tiles` — same design: fixed-size tiles + COW; memory pressure handled via iOS/macOS memory-pressure APIs instead of a custom swapper (at first) |
| brush engines (paintop plugins) | Stroke → dabs → composite | `gdrawing::brush` — engines as C++ classes behind one interface; **no dynamic plugin loading** (App Store, single binary) — compile-time registry instead |
| `KisImage` strokes facade | Async stroke system, undo/redo per stroke | `gdrawing::document` — command/stroke-based undo, background compositing |
| `KisDocument` / `KisView` | Document–view, many views per doc | Swift layer: `DocumentStore` (ObservableObject) + SwiftUI views; canvas is a Metal view |
| OpenGL canvas | GPU display of projection | **Metal/CAMetalLayer** — engine composites to tiles, Swift uploads dirty tiles to a Metal texture atlas |

Key Krita design decisions we keep:

- **The image is a tree of nodes** (layers/masks), each backed by a paint device
  (tiled pixel store). The composited result ("projection") is what the canvas shows.
- **Strokes are transactions**: input events feed a stroke; the stroke produces dabs;
  the whole stroke is one undo step. Undo is command-based, not snapshot-based.
- **Everything non-destructive where possible**: filter layers/masks, transform masks,
  clone layers — recompute, don't bake.
- **Color-managed from day one**: pixel formats are typed (u8/u16/f16/f32 × RGBA/Gray),
  conversions go through ICC. Retrofitting this is what kills painting apps.
- **Dirty-rect propagation**: edits mark tile-aligned dirty rects that bubble up the
  layer tree; only dirty regions are recomposited and re-uploaded to the GPU.

### 1.2 Methodology (how Krita develops, adapted)

- **Core-first, UI-thin**: Krita's `libs/image` has no UI dependency and is unit-tested
  in isolation. We do the same: the C++ framework never includes AppKit/UIKit; it is
  testable from a plain XCTest target with no simulator.
- **Vertical slices per release**: each phase below ends with something an artist can
  actually draw with, not a pile of infrastructure.
- **Tests where the math lives**: tile manager, compositing ops, color conversion, and
  undo get deterministic C++/XCTest coverage. UI gets thin smoke tests only.
- **Feature taxonomy as a checklist**: Krita's plugin categories (colorspaces, brush
  engines, filters, tools, generators, import/export) become our module checklist.

### 1.3 What we deliberately do differently

- **Apple-native, not cross-platform**: Metal instead of OpenGL, ColorSync instead of
  LCMS, `NSDocument`/`UIDocument` + file packages instead of KisPart, PencilKit-grade
  Apple Pencil input (hover, squeeze, barrel roll) as a first-class citizen.
- **No runtime plugins**: compile-time brush/filter registries. Extensibility later via
  data-driven brush presets (JSON), not dylibs.
- **No animation in v1** (Krita's biggest subsystem; explicitly out of scope until the
  painting core is excellent).
- **Touch + Pencil first on iPad**, pointer/tablet first on Mac — same engine, two
  input front-ends.

---

## 2. Feature set (Krita-derived, prioritized)

### P0 — Core painting (must exist before anything else matters)

- Canvas: pan/zoom/rotate, Metal-rendered, 60–120 fps, checkerboard transparency
- Tiled image store: 64×64 tiles, COW, u8/u16/f16/f32 RGBA + Gray
- Layer tree: raster layers, groups, opacity, visibility, lock, alpha-lock
- Blend modes: normal, multiply, screen, overlay, add, darken, lighten, erase (first 8;
  full Krita set ~70 comes later)
- Brush system v1: **pixel engine** (Krita's "pixel brush") — pressure → size/opacity/flow,
  spacing, hardness, anti-aliased dabs; stabilizer (weighted smoothing)
- Stroke-based undo/redo (command stack, per-stroke granularity)
- Eraser, color picker (on-canvas eyedropper), simple color selector (HSV wheel + sliders)
- Document I/O: native file package (`.gdraw`) with tile data + layer tree; PNG/JPEG export
- Apple Pencil (pressure, tilt, hover preview) and tablet pointer input

### P1 — Professional workflow

- Full blend-mode set; layer masks (transparency masks first)
- Selections: rectangle, ellipse, lasso, polygon, magic wand; select-transform-fill flow
- Transform tool: move/scale/rotate/flip on layer or selection
- Fill tool with gap closing (Krita 5.3 headline feature — artists expect it now)
- Brush engines: **color-smudge** (mixing/blending) and **shape/stamp** engines;
  brush presets with tagging, import of brush tip PNGs
- Color management: ICC working profiles, soft-proofing for CMYK print
- Gradients, patterns; bucket + gradient tools
- PSD import/export (layers, blend modes, masks) — non-negotiable for the target market
- Drawing assistants: straight-line ruler, ellipse, vanishing-point perspective
- Reference images docker; customizable workspace (Mac: panels; iPad: floating docks)

### P2 — Studio-grade

- Filter layers & filter masks (non-destructive adjustments: levels, curves, HSL, blur, sharpen, high-pass)
- Clone layers, file layers; transform masks
- Vector layers (comic lettering, panel frames) — text tool with OpenType
- Comic tooling: panel knife tool, multi-page documents, export to CBZ/PDF
- Symmetry / multibrush; wrap-around (seamless texture) mode — matte/texture painters
- HDR: f16/f32 painting end-to-end, EXR import/export, OCIO-style view transforms
  (start with ColorSync + custom LUT views)
- Colorize mask (line-art flatting — huge for comic production)
- Recorder/timelapse export; snapshot docker (named image states)

### Out of scope until further notice

Animation, scripting (Python/Lua), G'MIC-style filter packs, runtime plugins, Windows/Linux.

---

## 3. Framework architecture (`GDrawingStudioFramework`)

```text
GDrawingStudioFramework/
  GDrawingStudioFramework.h      Umbrella (exists)
  GDrawingStudio.hpp             Public C++ API surface for Swift
  src/
    color/       PixelFormat, ColorSpace tags, composite ops (SIMD via <simd/simd.h>)
    tiles/       Tile, TileStore (COW), DirtyRegion
    image/       Node tree (Layer, Group, Mask), Projection compositor
    brush/       BrushEngine interface, PixelEngine, dab generation, StrokeSmoother
    document/    Document, StrokeTransaction, UndoStack, serialization
    io/          .gdraw package read/write; PSD (P1); PNG/EXR via Apple APIs from Swift side
```

Interop rules:

- Public API is a small set of C++ types exposed through the umbrella header; Swift sees
  them directly (`SWIFT_OBJC_INTEROP_MODE = objcxx`). Keep the surface narrow:
  `Document`, `Canvas` (tile→texture bridge), `BrushPreset`, value types for geometry.
- Pixel data crosses to Swift only as opaque tile buffers handed to Metal; Swift never
  iterates pixels.
- Engine is UI-framework-free; all Apple UI types stay in the app target.

App side (per AGENTS.md page-COP structure):

```text
gmangastudio/
  Canvas/           Metal canvas view, input handling, CanvasViewModel → engine strokes
  Layers/           Layer panel COP
  Brushes/          Brush picker/editor COP
  ColorPicker/      Color selector COP
  ProjectBrowser/   Document browser (UIDocument/NSDocument backed)
```

---

## 4. Phased roadmap

Each phase ships a runnable, drawable build. Engine tests accompany each phase.

**Phase 0 — Spike (1–2 weeks)**
Prove the pipeline: C++ tile store → Metal texture → SwiftUI canvas; draw hard-edged
dabs with a finger/pencil on iPad and mouse on Mac. No layers, no undo. This validates
interop, threading, and the tile→GPU upload path before any real architecture lands.

**Phase 1 — Painting core (P0, ~2–3 months)**
Tiles + COW, layer tree with 8 blend modes, pixel brush engine with pressure dynamics
and stabilizer, stroke undo, eyedropper, HSV picker, `.gdraw` save/load, PNG export.
Exit criteria: an illustrator can complete a small piece start-to-finish.

**Phase 2 — Workflow (P1, ~3–4 months)**
Selections, transform, fill with gap closing, masks, full blend modes, smudge engine,
brush preset system, ICC management, PSD round-trip, assistants, reference images.
Exit criteria: a comic artist can ink and flat a page; PSD survives a Photoshop round-trip.

**Phase 3 — Studio (P2, ongoing)**
Non-destructive filter layers/masks, vector/text layers, comic panels + multi-page,
symmetry/wrap-around, HDR/EXR, colorize mask, timelapse.

---

## 5. Testing & quality gates

- **C++ unit tests** (XCTest target calling the framework): tile COW semantics,
  extent/dirty-rect math, each composite op against reference values, color
  conversion round-trips, undo/redo invariants, `.gdraw` round-trip.
- **Golden-image tests**: scripted strokes → composited projection → compare against
  checked-in PNGs (tolerance-based). This is how brush regressions get caught.
- **Performance budgets** (checked per phase, not optimized prematurely):
  4k×4k canvas, 20 layers, brush latency < 20 ms pencil-to-pixel on iPad,
  memory pressure handled without data loss (autosave on `didReceiveMemoryWarning`).
- **PSD conformance suite** (Phase 2): fixture files from Photoshop, round-trip diffs.
