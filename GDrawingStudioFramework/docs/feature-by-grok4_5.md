# GDrawingStudio — Feature & Development Plan

Plan for a **Mac + iPad** professional digital painting app.  
UI: **Swift / SwiftUI** (`gmangastudio/`). Core: **C++** (`GDrawingStudioFramework`).  
Reference studied: [KDE/krita](https://github.com/KDE/krita) (docs, brush-engine design, image/UI split) — **inspiration, not a port**.

**Audience (same as Krita’s stated users):** comic artists, illustrators, concept artists, matte & texture painters, digital VFX.

**Status legend:** `[ ]` planned · `[~]` partial / scaffold · `[x]` done

---

## 1. Product thesis

Build a **new-generation** painting studio that feels native on Apple platforms (Pencil, trackpad, multi-window Mac, Stage Manager / split iPad) while the **document, pixels, brushes, and composite** live in a portable C++ engine.

| Layer | Owns | Does not own |
|-------|------|----------------|
| SwiftUI app | Windows, dockers/panels, gestures, Pencil/trackpad routing, file pickers, settings UI, workspaces | Pixel math, brush dab algorithms, tile store, color convert |
| `GDrawingStudioFramework` | Document, tiles, layers, paint ops, composite, history, color, I/O, Metal present | SwiftUI layout, AppKit/UIKit chrome |

**Contract:** ViewModels call a thin public C++ facade (Swift–C++ interop). UI never includes internal `src/` headers.

```text
+----------------------------------------------------------+
|  gmangastudio (SwiftUI) — Mac / iPad chrome & input      |
+----------------------------------------------------------+
                          │ ▲
                          ▼ │  public C++ API
+----------------------------------------------------------+
|  GDrawingStudioFramework — document / paint / present    |
+----------------------------------------------------------+
```

---

## 2. What we take from Krita (methodology)

Krita’s value for us is **how it is structured**, not cloning Qt dockers or every paintop.

### 2.1 Hard split: image core vs UI

Krita: `libs/image` (paint devices, layers, brush engines) vs `libs/ui` (tools, dockers, resources UI).  
**We mirror that:** all stroke/composite/history logic in the framework; Swift only presents state and sends commands.

### 2.2 Tile-based paint device

Krita’s `KisPaintDevice` is auto-extending, tile-backed, can swap under memory pressure.  
**We plan:** tiled RGBA (then multi-depth) buffers per layer; dirty-rect updates; Metal textures per dirty tiles. No full-canvas realloc on every stroke.

### 2.3 Pluggable brush engines, minimal policy

Krita’s brush framework (`KisPaintOp` + registry): engines implement `paintAt` / `paintLine` / `paintBezierCurve`; settings serialize independently; presets are resources. Policy is intentionally thin so engines can be simple (eraser) or complex (smudge).  
**We plan:** `PaintOp` interface + registry; first engines only; shared options (pressure curves, spacing, tip) in a small `paintop` helper — not one mega-brush class.

### 2.4 Layers as a compositing graph

Krita: paint / vector / group / clone / file / fill / filter layers; masks (transparency, filter, transform); blend modes; inherit-alpha / clipping.  
**We plan:** start with **paint layers + groups + opacity/visibility + blend modes**; add masks and specialty layers when the tile/composite path is solid.

### 2.5 Resources are first-class

Brushes, presets, patterns, gradients, workspaces — loadable, taggable, shareable.  
**We plan:** resource store on disk (app support + iCloud later); brush presets as the unit artists switch; import paths for common packs where licensing allows.

### 2.6 Artist workflow concepts (product language)

From Krita’s basic concepts / user manual — these are the nouns our UI and API should speak:

| Concept | Meaning for us |
|---------|----------------|
| Image / Document | Working copy: size, color space, layer tree, metadata |
| View / Viewport | Zoom, pan, rotate, mirror — view-only, not document mutate |
| Canvas | Exportable bounds; layers may store data outside (optional later) |
| Tool → Brush engine | Tool describes path; engine turns path + sensors into dabs |
| Selection | Restrict paint/transform; pixel (and later vector) |
| Assistant / guide | Perspective, rulers, grids — comic & concept critical |
| Workspace | Saved panel layouts (Mac windows / iPad compact layouts) |

### 2.7 What we deliberately do *not* copy

- Qt / KDE plugin ABI, Python scripting (v1), full 18 brush engines on day one  
- `.kra` as primary format (we define our own; optional import later)  
- Desktop-only docker soup on iPad — adapt to NavigationSplitView / sheets / floating bars  
- Feature freeze culture of a 20-year project — we use **thin vertical slices** instead

---

## 3. Platform principles (Mac + iPad)

| Concern | Mac | iPad |
|---------|-----|------|
| Input | Mouse, trackpad, Wacom/Huion via macOS, Pencil if Sidecar | Apple Pencil (pressure, tilt, hover where available), touch gestures |
| Chrome | Multi-window, menu bar, inspector sidebars | Compact toolbars, popovers, optional external display |
| Performance | Large canvases, multi-monitor views | Thermal + memory budget; aggressive tile eviction |
| Present | Metal, ProMotion where available | Metal, 120 Hz when device allows |

**Input rule:** Swift normalizes pointer events → canvas-space samples (`x, y, pressure, tilt, azimuth, timestamp, button`) → framework `beginStroke` / `continueStroke` / `endStroke`. Engine never sees UIKit/AppKit types.

---

## 4. Feature map by audience

Prioritize features that unblock each persona early; defer niche engines.

| Persona | Must-have (early) | Strong later | Defer |
|---------|-------------------|--------------|-------|
| Comic | Inking brushes, layers, selections, assistants (panel/perspective), text (later), export PNG/PDF | Vector balloons, storyboard, animation | Full vector suite |
| Illustrator | Pixel + soft brushes, blend modes, color picker, transform, soft proof (later) | Smudge, filters, liquify | Print CMYK pipeline v1 |
| Concept | Large canvas, fast brushes, color set / gamut, reference images, transform | Symmetry, fill layers | 3D / PBR bake |
| Matte / texture | High-res tiles, 16-f / float later, clone stamp, transform, UDIM-ish export later | Filter layers, wraparound | Full OCIO suite day one |
| VFX | Color management path, EXR/TIFF, non-destructive stack direction, HDR display later | Animation / holdout | Nuke-level node graph |

---

## 5. Krita feature inventory → our phases

Mapped from Krita user/reference manuals and brush-engine list. **Phase = when we build it**, not Krita parity score.

### P0 — Paintable document (foundation)

| Area | Krita analogue | Our scope |
|------|----------------|-----------|
| Document | Image | Width/height/DPI, background, color profile stub (sRGB) |
| Paint device | `KisPaintDevice` tiles | Tiled layer buffers, dirty rects |
| Layers | Paint layer stack | Add/remove/reorder/duplicate, opacity, visibility, Normal blend |
| Brush | Pixel brush (subset) | Soft/hard round tip, pressure→size/opacity, eraser as blend |
| Viewport | View | Zoom, pan, rotate, mirror |
| Present | Canvas | Metal present of composite |
| History | Undo stack | Stroke-level undo/redo |
| I/O | Save | Native project + PNG export |

### P1 — Professional painting core

| Area | Krita analogue | Our scope |
|------|----------------|-----------|
| Brush engines | Pixel, Color Smudge (start) | Registry; pixel engine v1; smudge v1 |
| Presets / resources | Presets, tags | Library UI, session overrides, save preset |
| Blend modes | 76 modes | Procreate/Krita-useful subset first (Multiply, Screen, Overlay, …) |
| Selections | Pixel selection | Rect/lasso/freehand; mask paint; invert/clear |
| Transform | Transform / move / crop | Move, free transform, flip, canvas resize/crop |
| Color | Advanced color | HSV/RGB pickers, history, eyedropper; float/16-bit path design |
| Assistants | Perspective, vanishing | 1–3 point perspective + ruler + grid |
| Reference | Reference images | Pin images beside / overlay (UI-heavy) |

### P2 — Non-destructive & production

| Area | Krita analogue | Our scope |
|------|----------------|-----------|
| Groups / clipping | Group, inherit alpha | Groups, clip-to-below |
| Masks | Transparency / filter / transform masks | Transparency mask first |
| Filters | Filter layer/mask, filter brush | Blur/sharpen/HSV adjust; filter-as-brush later |
| Specialty layers | Fill, clone, file | Fill + file/reference layer |
| Color mgmt | LCMS / soft proof | Display profile, soft proof, out-of-gamut warning |
| HDR / deep color | HDR, 16-f | 16-bit then float; HDR display when Mac supports |
| Import packs | Bundle resources | Brush pack import (own format + selective foreign) |

### P3 — Studio & motion

| Area | Krita analogue | Our scope |
|------|----------------|-----------|
| Animation | Timeline, onion skin | Frame timeline on paint layers, onion skin, GIF/MP4 export |
| Vector / text | Vector layers, text | Comic text + simple vectors |
| Scripting / automation | Python | Swift/JS automation or none until asked |
| Multi-view | New view | Two views same doc (Mac); optional iPad stage |

### Brush engines (Krita list → adopt order)

| Engine (Krita) | Adopt? | Phase |
|----------------|--------|-------|
| Pixel | Yes — primary | P0–P1 |
| Color Smudge | Yes | P1 |
| Sketch | Maybe | P2 |
| Shape / Quick | Maybe (flats) | P2 |
| Clone | Yes (texture/matte) | P2 |
| Filter | Yes | P2 |
| Deform / Liquify-like | Yes (transform tool first) | P2 |
| MyPaint | Optional compatibility | P3 |
| Bristle, Chalk, Curve, Dyna, Grid, Hatching, Particle, Spray, Tangent Normal | Selective / never | Research only |

---

## 6. Framework module plan (`GDrawingStudioFramework`)

Target tree (grow as phases land; do not create empty folders early):

```text
GDrawingStudioFramework/
  GDrawingStudioFramework.h      # umbrella
  GDrawingStudio.hpp             # public facade
  module.modulemap
  docs/
    feature.md                   # this file
  src/
    document/                    # Document, PageSettings, color space
    tiles/                       # TileGrid, PaintDevice, swap policy
    layers/                      # Layer, LayerStack, blend
    paint/                       # Stroke samples, PaintOp, registry, pixel/smudge
    resources/                   # Presets, tips, patterns
    selection/                   # Selection mask
    history/                     # Command stack
    viewport/                    # Camera / present transform
    render/                      # Composite + Metal present
    color/                       # Convert, profiles (later)
    io/                          # Project + raster export
```

**Public facade sketch (evolve; keep small):**

- Document: create/open/save, size, color space  
- Layers: CRUD, opacity, visibility, blend, active  
- Tools: tool mode, brush preset, session overrides  
- Stroke: begin/continue/end with `PointerSample`  
- Viewport: zoom/pan/rotate/mirror; present to Metal drawable  
- History: undo/redo  
- Query: thumbnails, dirty bounds, `version()`, `selfCheck()`

---

## 7. Swift UI plan (`gmangastudio` page COPs)

| Page / surface | Role |
|----------------|------|
| `Studio` (or `Canvas`) | Main painting surface + tool chrome |
| `BrushLibrary` | Presets, tags, editor sheet |
| `Layers` | Stack, blend, masks (when ready) |
| `Color` | Pickers, history, palettes |
| `DocumentBrowser` | New / open / templates (comic, texture, …) |
| Settings / Workspace | Shortcuts, canvas input, layout presets |

**Rules:** one ViewModel per page root; ViewModels talk only to the public C++ facade; shared helpers move to `Shared/` only when a second page needs them. Optimize for **Mac windowing + iPad Pencil**, not phone chrome.

---

## 8. Development methodology (how we build)

Adapted from Krita’s engineering habits + this repo’s ponytail rules.

1. **Vertical slices** — each milestone ships: document change → paint/composite → present → one UI control. No orphan engine code.  
2. **Core before chrome** — tile + stroke + undo before fancy dockers.  
3. **One runnable check** — non-trivial C++ logic gets `selfCheck()` or one XCTest that fails if the math breaks.  
4. **Registry over switch soup** — new brush engines register; UI lists engines from the registry.  
5. **Sensors as data** — pressure/tilt/speed curves are data on presets, not hard-coded in UI.  
6. **Dirty rect discipline** — never full-canvas upload if a tile set suffices.  
7. **Direct vs indirect paint** — support “paint into temp then merge” for soft brushes / smudge when needed (Krita’s incremental/indirect lesson).  
8. **Artist validation** — each phase ends with a short real-task checklist (ink a panel, paint a concept head, clone a texture).  
9. **YAGNI on engines** — do not implement Krita’s full paintop list; add an engine when a persona checklist fails without it.  
10. **Docs as contract** — this file + later per-area specs (`document.md`, `brush.md`, …) stay the source of truth for status.

### Milestone checklist (summary)

| Milestone | Exit criteria |
|-----------|----------------|
| **M0** Scaffold | Framework links; `gdrawing::version()`; empty Studio page hosts Metal view |
| **M1** First stroke | Tiled layer, pixel brush, eraser, zoom/pan, undo stroke, PNG export |
| **M2** Layer studio | Multi-layer, blend subset, reorder, thumbnails, color picker |
| **M3** Pro brush | Presets library, pressure curves, smudge v1, selection v1 |
| **M4** Production | Transform, assistants, groups/clip, transparency mask, project format stable |
| **M5** Deep color & I/O | 16-bit/float path, TIFF/EXR, soft proof |
| **M6** Motion & comic | Timeline v1, text/vector lite, PDF/comic export |

---

## 9. Non-goals (until explicitly requested)

- Phone-first UI  
- Full Krita / Photoshop / Procreate feature parity  
- Node-based compositor (Nuke-style)  
- Built-in generative AI painting (product decision separate)  
- Android / Windows ports of the Swift UI (C++ core may stay portable)

---

## 10. Immediate next steps

1. Spec **M1** public API in a short `docs/api.md` (facade methods only).  
2. Implement `document/` + `tiles/` + `layers/` + pixel `PaintOp` + Metal present.  
3. Add `Studio` page COP: Metal canvas view + pointer → stroke bridge.  
4. Mark progress in this file as milestones complete.

---

## References

- [Krita README / audience](https://github.com/KDE/krita)  
- [Krita Basic Concepts](https://docs.krita.org/en/user_manual/getting_started/basic_concepts.html)  
- [Krita User Manual](https://docs.krita.org/en/user_manual.html)  
- [Krita Reference Manual](https://docs.krita.org/en/reference_manual.html)  
- [Krita Brush Engines](https://docs.krita.org/en/reference_manual/brushes/brush_engines.html)  
- [Krita BrushEngine design (wiki)](https://community.kde.org/Krita/BrushEngine)  
- App structure: [AGENTS.md](../../AGENTS.md)
