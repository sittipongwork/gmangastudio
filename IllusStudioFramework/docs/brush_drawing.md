# Hybrid drawing — Brush library & eraser

Feature spec + design for brush library, eraser, **Color Fill (ColorDrop)**, and the **Hybrid (Vector + Raster)** pipeline so the app keeps vector integrity (undo, edit, sharp export) and raster fluidity (soft brushes, blur, fill, high-rate present).

**Status (code):** T1-1…T1-4, T1-7, **T1-9 BrushModel v2** (StampEngine Shape+Grain), T2-7-1 done. Paint stamps **into the active layer** via tip silhouette + Moving/Texturized grain. Open: T2-6 move/adjust, **T1-8 Color Fill**, T3 history, image import (T4-1), GPU tip+grain (T1-9-5).  
**Tasks & checkboxes:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [API.md](API.md) · [layer.md](layer.md) · [canvas_document.md](canvas_document.md) · [history.md](history.md) · [AGENTS.md](../../AGENTS.md)

---

## Summary

| | |
|--|--|
| **Inputs** | Tool mode (brush/eraser/fill); preset id; **pre-draw** line width, line smooth, hardness, opacity, flow, spacing, pressure gains, color; pointer stream with pressure; Procreate `.brush` / `.brushset` / `.brushlibrary` import; ColorDrop seed + threshold |
| **State** | `BrushLibrary` + sets; `BrushSession` overrides; vector strokes on active layer; regenerable raster cache; tip/grain assets; fill session (threshold, continue-filling); optional **Reference** layer |
| **API** | Stroke stream + session setters + `importBrushPackage*` (**shipped**); layer-scoped stroke query/edit (**T2-6**); Color Fill / ColorDrop (**T1-8**, planned) |
| **Eraser** | Dest-out on active layer; recorded as vector stroke |
| **Fill** | Flood-fill active layer from seed (Procreate ColorDrop); optional Reference layer for line boundaries |
| **v1 out** | Dual brush, smudge, wet-mix parity, full Procreate engine clone, shipping Procreate default packs |

---

## Image import (placed raster)

Import a decoded image onto the canvas as a **movable, resizable** placed object (typically a new layer or content on the active layer). UI owns file pickers; engine owns pixels + transform.

| | |
|--|--|
| **Inputs** | Decoded RGBA bytes (or path decoded at UI) + initial placement (fit / fill / center / offset / scale) |
| **State** | Placed image: source pixels (or regenerable from import), `transform` (translation + size), optional layer id; selection chrome is UI |
| **API** | `importRGBA(w, h, pixels, placement)` (planned); transform setters for move / resize; hit-test for select |
| **v1 out** | Vector/PDF import; camera capture pipeline; free rotate / perspective warp (unless added later) |

### Placement & transform

After import, the image is an editable **AABB** in canvas space:

```text
Placement {
  x, y          // top-left (or anchor — document which)
  width, height // display size in canvas px (may differ from source w/h)
  // optional later: rotationDeg, flipX/Y
}
```

| Interaction | Behavior |
|-------------|----------|
| **Move** | Drag interior (or move tool) → translate `(x, y)`; source pixels unchanged |
| **8-corner resize** | Eight handles: **4 corners** + **4 edge midpoints** (N, S, E, W, NE, NW, SE, SW) |
| Corner drag | Scales width **and** height from the opposite corner as anchor; optional Shift = lock aspect |
| Edge drag | Scales **one** axis only (E/W → width, N/S → height); opposite edge stays fixed |
| Commit | Engine stores new `Placement`; invalidates dirty rect = union(old bounds, new bounds); re-composite / re-upload |

```text
        NW ---- N ---- NE
        |               |
        W      image    E
        |               |
        SW ---- S ---- SE
```

**Rules**

- Handles and hit-testing live in **canvas space** (same as strokes); viewport zoom only affects how large handles appear on screen.
- Minimum size clamp (e.g. a few px) so the AABB cannot invert or vanish.
- Aspect lock: UI modifier or a “lock aspect” session flag; engine can expose `setPlacement(..., lockAspect)`.
- Move/resize should not resample the source on every drag if avoidable — store source at import resolution; sample into layer/cache at current `width`×`height` on commit or while dragging (live preview OK).
- Undo (when history lands): `ImportImageCommand` / `TransformPlacementCommand` with inverse transform — not a full framebuffer dump.

### Engine vs UI

| Layer | Owns |
|-------|------|
| **UI** | Document picker, selection outline, 8 handles, drag gestures, aspect-lock chrome |
| **Engine** | RGBA buffer, placement rect, hit-test bounds, rasterize-into-layer / keep as placed object, dirty rects |

### Self-check targets

- Import → non-empty pixels on target layer (or placed object exists).
- Move by `(dx, dy)` → bounds shift by that delta; other layers unchanged.
- Corner resize → opposite corner fixed within epsilon; edge resize → only one axis changes.

**AI path:** reference → line-art → line/color layers — see [AI_Integration.md](AI_Integration.md#1-import-reference-image).

---

## Goals

| Goal | How hybrid delivers it |
|------|------------------------|
| Fluidity & effects | Raster cache + Metal: soft dabs, hardness falloff, **Color Fill**, blur later |
| Sharpness & integrity | Authoritative stroke is vector (samples / curves), not pixels |
| Anti-corruption | Undo/redo and stroke edit mutate vector data; raster is regenerable |
| Max performance | Live stroke → GPU rasterize dirty tiles; idle layers stay as textures; no full CPU re-stamp of history |
| **Easy move / adjust line** | While working in a layer, UI can **query vector coordinates for strokes on that layer only**, then move or reshape the line; engine updates vector → invalidates raster tiles |

**v1 out of scope** (same as Summary above): shape stamps, dual brushes, smudge, wet mix. Those plug into the same stroke → rasterize path later.

### Goal detail — Easy move / adjust line

Users must be able to **select a line (stroke) inside the current layer**, read its vector geometry, and **move or adjust** it without redrawing and without touching other layers.

| Requirement | Rule |
|-------------|------|
| Scope | Vector query + edit apply to **one layer at a time** (active layer, or an explicit `layerId`) |
| Isolation | Never return or mutate strokes that belong to other layers |
| Source of truth | Edit **samples / path points** in the Data Layer; do not “move pixels” as the primary op |
| Feedback | After translate / point edit → invalidate that stroke’s old∪new bounds → re-raster layer tiles → present |
| UI role | Hit-test, handles, drag gestures live in Swift; engine owns geometry + commit |

This is why hybrid stores strokes as vectors: raster alone cannot offer clean line move/adjust.

---

## Hybrid pipeline (authoritative)

```text
[Input Layer] --------> Receives pen coordinates (X, Y, Pressure, Tilt)
│
▼
[1. Data Layer (Vector)]
│ - Stores raw data in mathematical format (Stroke Object, Bézier / samples)
│ - Manages Undo/Redo and reverse modification (line modification)
│
▼ (Real-time conversion)
│
[2. Cache Layer (Raster / Metal Texture)]
│ - Metal Compute (or CPU fallback) converts vectors → pixel buffers
│ - Separates textures per user layer (Layer 1, Layer 2, …)
│
▼
[3. Display Layer (GPU Rendering)]
- Blends metal textures of all layers
- Presents via MTKView (SwiftUI)
```

**Source of truth:** Vector Data Layer.  
**What the user sees:** Display Layer (composited GPU textures).  
**Cache Layer:** Disposable; rebuild from vector when invalidated (undo, brush param change, resolution change).

---

## Mapping onto IllusStudioFramework

```text
UI (DrawingEditor)
  pointer → canvas space (X,Y,pressure[,tilt])
       │
       ▼
illus::CanvasEditor                    ← public API (Swift–C++ interop)
       │
       ▼
IllusStudioCanvasEditor
  ├── tools/           BrushLibrary, BrushSession, BrushAssetStore
  │     └── procreate/ ProcreateBrushImporter, ProcreateBrushMap
  ├── strokes/         Stroke / StrokeSample (Data Layer)
  ├── history/         StrokeCommand (planned T3)
  ├── layers/          Layer id + GPU texture handle
  └── render/
        ├── StrokeRasterizer        Vector → layer (CPU; procedural round)
        ├── MetalStrokeRasterizer   Compute dab (exists; not hot path yet)
        ├── LayerCompositor         Blend layer textures → present
        └── MetalRenderer           Present / device (T6)
```

| Hybrid layer | Module / type | Notes |
|--------------|---------------|--------|
| Input | UI + `beginStroke` / `continueStroke` / `endStroke` | Canvas-space only; viewport maps in UI or `viewport/` |
| Data (Vector) | `tools/` + `strokes/` | Never treat layer RGBA as the only stroke store |
| Cache (Raster) | Per-layer `MTLTexture` (+ CPU fallback) | Dirty upload while stroking |
| Display | GPU blend + MTKView path | Prefer blend layer textures; CPU composite is fallback / self-check |

**Current code:** Vector samples → CPU `StrokeRasterizer` (StampEngine) into **layer** pixels → dirty GPU upload → `LayerCompositor` present. Soft paint live-overlay retired (overlap quality). Tip+grain stamp shipped (T1-7-3b / T1-9).

---

## 1. Input Layer

### Pointer sample

```text
PointerSample {
  x, y          // canvas space (float)
  pressure      // 0..1 (default 1 if unknown)
  tiltX, tiltY  // optional; 0 if unavailable
  azimuth       // optional
  timestamp     // for spacing / timelapse
}
```

### Rules

- UI converts view → canvas **before** calling the engine (or via `viewport/` once **T2** lands).
- Engine does **not** own screen gestures.
- Coalesce high-rate samples only if spacing math still sees enough points (never drop the last sample of a stroke).
- Apple Pencil / trackpad: pass pressure when present; mouse → pressure `1`.

### Public API (extend `CanvasEditor`)

Keep the existing stream; add tool/preset selection + **pre-draw brush properties** (see Brush library):

```text
setTool(Brush | Eraser | Pointer)
setBrushPreset(presetId)
// Session overrides — user adjusts in UI *before* drawing (do not mutate library until "Save brush")
setBrushLineWidth(float) / setBrushLineSmooth(float) / setBrushHardness(float) / …
setBrushOpacity(float) / setBrushColor(r,g,b,a) / setBrushSpacing(float) / …
beginStroke(x, y, pressure)           // optional: overload with tilt later
continueStroke(x, y, pressure)
endStroke()
```

Tilt: add `beginStrokeEx` / `continueStrokeEx` when UI is ready — do not block **T1-1** on tilt (**T2-7-2**).

---

## 2. Data Layer (Vector) — source of truth

### Why vector first

- **Undo/redo:** push/pop `Stroke` objects (or inverse commands), not full layer bitmaps.
- **Edit later:** reshape control points / resample without irreversible pixel mush.
- **Export:** SVG / path-ish export and “re-raster at new DPI” stay possible.
- **Anti-corruption:** if a layer texture is wrong, rebuild from strokes; pixels alone cannot recover intent.

### Core types (`src/tools/` + `src/strokes/`)

```text
// Library entry (defaults). User tweaks live in BrushSession until "reset" or "save as preset".
BrushPreset {
  id, name
  mode: Paint | Erase
  source: BuiltIn | User | ImportedProcreate

  // --- Shape / stroke look (user-adjustable before draw) ---
  lineWidthPx          // max dab diameter at pressure=1 (aka size)
  lineSmooth           // 0..1 input stream smoothing before samples commit
  hardness             // 0..1 edge falloff (soft → hard)
  opacity              // 0..1 base alpha
  flow                 // 0..1 per-dab alpha accumulate
  spacing              // fraction of lineWidth between dabs
  roundness            // 1 = circle; <1 = ellipse (later)
  angleDeg             // tip rotation (later / import)

  // --- Dynamics (curves; v1 can be linear gain) ---
  sizePressure         // 0..1 how much pressure scales lineWidth
  opacityPressure      // 0..1 how much pressure scales opacity
  // optional later: tilt→size, speed→opacity, jitter

  color RGBA           // paint only; ignored for Erase

  // --- Tip / grain (round procedural if null; filled by Procreate import) ---
  tipTextureId         // optional stamp PNG in brush asset store
  grainTextureId       // optional
  previewTextureId     // UI thumbnail only
}

BrushSession {
  // Active tool state: selected preset id + overrides the user set in the inspector
  // before / between strokes. Copied into Stroke.presetSnapshot at beginStroke.
  presetId
  overrides            // BrushPreset fields that differ from library defaults
  color RGBA           // often session-level even when preset has a default
}

StrokeSample {
  x, y, pressure, tiltX, tiltY, t
}

Stroke {
  id
  layerId
  presetSnapshot     // resolved BrushPreset (library ⊕ session) at stroke start — immutable
  samples[]          // raw input (editable for adjust; translate for move)
  path / cubics      // optional: fitted cubics (lazy on export — not on endStroke)
  bounds             // canvas AABB of stamped coverage (recompute after edit)
}

LayerStrokeList {
  layerId
  strokes[]          // ordered; only these are visible to vector query for that layer
}
```

**Layer ownership:** every `Stroke` carries `layerId`. Query APIs take `layerId` (default: active) and return **only** that layer’s strokes. Moving a line never pulls geometry from another layer.

**Pre-draw rule:** Changing `BrushSession` (line width, smooth, etc.) affects the **next** stroke only. In-progress strokes keep their `presetSnapshot`.

### Live stroke vs committed stroke

| Phase | Vector state | Raster state |
|-------|--------------|--------------|
| `beginStroke` | Create `Stroke` on active layer; append first sample; snapshot `BrushSession` | Stamp first dab **into active layer** pixels; dirty-upload GPU layer texture |
| `continueStroke` | Append samples; update bounds | Incremental stamp of new segment into layer; dirty-upload |
| `endStroke` | Commit stroke (dense samples only; cubics empty) | Stroke already on layer; sync GPU if needed |
| Move / adjust | Mutate that stroke’s samples/path; recompute bounds | Invalidate old∪new bounds → re-raster affected tiles |
| Undo | Remove last stroke (or apply inverse / un-transform) | Invalidate stroke bounds → re-raster affected tiles from remaining strokes |

**Why not live overlay for paint?** Soft strokes composited as a separate overlay on top of the layer went dark/noisy where strokes crossed (double soft coverage in present). Same-color paint uses `math::blendPaintOver` (keep brush RGB, accumulate alpha). Overlay merge helpers remain for a possible future path; paint+erase currently share the layer stamp path.

### Curve fitting (T2-7-1 done — internal)

- Dense `StrokeSample[]` is the paint source of truth; rasterizer walks samples with dab spacing.
- **`math/Bezier` + Eigen:** `ensureStrokeCubics` / `fitStrokeCubics` **lazy** (export T4 / demand) — **not** on `endStroke` under the editor mutex ([TX-7](ROADMAP.md#tx-7--math-libraries-glm--eigen)).
- If fit error is high, leave `cubics` empty and keep samples.
- No public cubic-export API yet (lands with T4 SVG).

### Eraser as vector

Eraser is **not** a separate buffer. It is a `Stroke` with `preset.mode == Erase`:

- Rasterizer uses **dest-out** (or equivalent premultiplied erase) into the **active layer texture only**.
- Vector list still records the erase stroke so undo restores paint underneath (by re-rasterizing the layer from remaining strokes — see Cache).

**ponytail:** Per-layer stroke lists + full layer re-raster on undo is O(strokes) in the dirty AABB. If that becomes hot, upgrade to tile stamps + stroke→tile index (see Perf).

### Move & adjust (vector edit on one layer)

Editing is a first-class Data Layer operation, not a pixel transform.

```text
Select (UI)     hit-test canvas point → strokeId on layerId only
Query (API)     strokeCount / strokeIdAt / copy samples or path points for that stroke
Move            translate all samples (and path controls) by (dx, dy); clamp optional
Adjust          move one sample or Bézier control; or reshape a segment
Commit          recompute bounds; invalidate raster; history records TransformStroke / EditStroke
```

| Op | Vector mutation | Raster |
|----|-----------------|--------|
| **Move** | `sample.x += dx`, `sample.y += dy` (all samples); same for path controls | Dirty = union(bounds_before, bounds_after) |
| **Adjust point** | Update one sample or control point; optional neighbor smoothing later | Same dirty union |
| **Adjust via handles** | UI drags exposed points from query; engine applies `setStrokePoint` | Same |

**Hit-test (engine helper, canvas space):**

- Input: `layerId`, point `(x,y)`, optional pick radius (e.g. max(brush size, 8px) in canvas units).
- Search **that layer’s strokes only** (top-most / last-drawn first).
- Distance to polyline (samples) or to cubic segments if `path` exists.
- Return `strokeId` or none — UI draws selection chrome from a follow-up geometry query.

**What UI may request (layer-scoped):**

| Query | Returns | Use |
|-------|---------|-----|
| Stroke ids on layer | Ordered ids + bounds | List / lasso candidates |
| Stroke polyline | `(x,y)[]` or samples with pressure | Draw outline, hit handles |
| Stroke path (if fitted) | Cubic control points | Finer adjust when available |
| Preset snapshot (read-only) | size/color/mode | Show brush chip; not required for move |

Do **not** expose a document-wide “all strokes” dump in v1 — always pass `layerId`.

**Live preview while dragging:** either (a) mutate vector + cheap re-raster of dirty tiles each move event, or (b) UI draws a ghost polyline in Swift and calls `commitStrokeTransform` on finger-up. Prefer (a) once tile re-raster is cheap; (b) is fine for **T2-6**.

---

## 3. Cache Layer (Raster / Metal)

### Per-layer textures

Each user layer owns:

```text
LayerGpu {
  texture        // MTLTexture RGBA8 (or R8 for mask later)
  dirtyTiles[]   // sparse invalidation
  // CPU fallback: existing lazy std::vector<uint8_t> pixels (keep for self-check / no-GPU)
}
```

**Current paint path:** stamp into the active layer’s CPU pixels, then dirty-upload that layer’s `MTLTexture`. A live overlay texture still exists in `LayerCompositor` but is **not** used for paint (see Live stroke vs committed stroke).

### Vector → raster (real-time)

```text
StrokeRasterizer
  input:  Stroke (or sample polyline) + BrushPreset + clip rect
  output: writes into active layer pixels (dirty region) → GPU upload
```

**Hot path (current):** CPU `StrokeRasterizer::stampDab` / `stampSegment` → `syncGpuLayer` dirty `replaceRegion`.

- Paint: `math::blendPaintOver` (same/near-same color → coverage accumulate; else straight-alpha src-over).
- Erase: `math::blendDestOut`.
- Layers stay **straight alpha**; `LayerCompositor` premultiplies at present.

**Metal compute dab** (`MetalStrokeRasterizer` / `stampDab` kernel) exists but is **not** on the hot stroke path (per-dab `waitUntilCompleted` was too slow). Re-enable when batched.

### Invalidation rules

| Event | Invalidate |
|-------|------------|
| New dab / segment | Tiles / dirty rect intersecting dab AABB; upload layer texture |
| `endStroke` | Mark present dirty; GPU layer already synced during stroke |
| Move / adjust stroke | Tiles intersecting **union(old bounds, new bounds)**; rebuild those tiles from that layer’s strokes |
| Undo/redo stroke | Tiles intersecting that stroke’s `bounds`; rebuild from vector |
| Clear layer | Release texture + clear stroke list |
| Resize canvas | Full rebuild (v1 out: reflow) |

### Separation from Display

- Cache = **per-layer** textures (straight alpha).
- Do **not** bake the full document into one CPU buffer as the only cache (CPU composite remains for export / fallback).
- Present: **blend layer textures on GPU** → one drawable (Display).

---

## 4. Display Layer (GPU)

### Target

```text
Layer textures (N) + background
    → Metal render/compute blend (opacity, visible, Normal / Erase already in layer)
    → present texture / MTKView drawable
```

### Rules

- Same `MTLDevice` for engine textures and `MTKView` (already required; cross-device sample = crash).
- Target **120fps** present while stroking: dirty layer upload + GPU stack blend; keep layer count and resolution in mind.
- Viewport zoom/pan (**T2**): apply as vertex transform on the composited quad (or tile atlas); do not re-raster strokes for pan.

### SwiftUI

- Keep `CanvasMetalView` as the present surface.
- ViewModel only calls `CanvasEditor`; no internal `src/` headers.

---

## Brush library (T1 feature fill)

### Library storage

```text
BrushLibrary {
  sets: [BrushSet]           // Procreate-like folders / imported .brushset
  presets: [BrushPreset]     // flat index; each preset belongs to a set
  assets: BrushAssetStore    // tip/grain/preview PNG bytes keyed by textureId
  activePresetId
}

BrushSet {
  id, name
  source: BuiltIn | User | ImportedProcreate
  presetIds[]
}
```

Built-ins (four sets):

| Set | Presets | Notes |
|-----|---------|--------|
| **Sketching** | `pencil.hard`, `pencil.soft`, `sketch.rough` | Pressure-friendly pencils |
| **Inking** | `ink.fine`, `ink.round`, `ink.brush` | Hard / round / brush ink (default paint = `ink.round`) |
| **Drawing** | `technical.pen`, `marker`, `erase.soft`, `erase.hard` | Line tools + erasers |
| **Painting** | `paint.round`, `air.soft`, `paint.wash` | Soft / airbrush / wash |

All procedural round for now (tip textures: T1-7-3b).

### Pre-draw properties (user adjusts before drawing)

UI inspector binds to **`BrushSession`**, not the library row (unless user taps Save). Engine applies session → resolved preset at `beginStroke`.

| Property | API (illustrative) | Range | Effect |
|----------|-------------------|-------|--------|
| **Line width** | `setBrushLineWidth` / `brushLineWidth` | > 0 px | Max dab diameter (`lineWidthPx`) |
| **Line smooth** | `setBrushLineSmooth` / `brushLineSmooth` | 0..1 | EMA / lag on input before sample append; 0 = raw, 1 = heavy stabilize |
| **Hardness** | `setBrushHardness` | 0..1 | Soft edge → hard stamp |
| **Opacity** | `setBrushOpacity` | 0..1 | Base stroke alpha |
| **Flow** | `setBrushFlow` | 0..1 | Per-dab accumulate |
| **Spacing** | `setBrushSpacing` | ~0.01..2 | Dab step as fraction of width |
| **Size pressure** | `setBrushSizePressure` | 0..1 | Pressure → width |
| **Opacity pressure** | `setBrushOpacityPressure` | 0..1 | Pressure → alpha |
| **Color** | `setBrushColor` | RGBA | Paint only |

**Line smooth (engine):**

```text
on pointer sample:
  if lineSmooth == 0: append sample as-is
  else: smoothed = mix(prevSmoothed, raw, 1 - lineSmooth * k)
        append smoothed (still store pressure from raw or smoothed — pick one; document it)
```

Smoothing is **input filtering** in the Data Layer (before samples land on the stroke). It is not a post-blur of pixels.

**Reset / save:**

- `resetBrushSession()` — drop overrides; reload from selected `BrushPreset`
- `saveBrushSessionAsPreset(name)` — write new User preset into library (optional set)
- Changing preset via `setBrushPreset` loads that preset’s defaults into session (clear overrides)

### Selection flow

1. UI lists sets → presets (`brushSetCount` / `brushPresetCountInSet` / names) — Swift-friendly.
2. `setBrushPreset` loads defaults into `BrushSession`.
3. User tweaks line width / smooth / etc. in inspector **before** drawing.
4. `setTool(Eraser)` may auto-select last erase preset; brush tool restores last paint preset.

### Dab model (v1 — current code)

- **Procedural round only** for stamp coverage (imported `Shape.png` tips are stored for UI / **T1-7-3b**, not stamped — raw tip coverage speckled strokes).
- Coverage = hard core + smooth hardness falloff + ~1px AA fringe.
- Effective width = `lineWidthPx * mix(1, pressure, sizePressure)`.
- Spacing: step ≤ ~`0.10–0.15 × diameter` (imported tip presets floored tighter; Procreate map caps spacing).
- Flow/hardness: tip-bearing presets floored toward solid ink until tip+grain lands.
- Color: straight-alpha layer pixels; paint uses `blendPaintOver` so same-color overlaps stay vivid.

---

## Procreate-style brush import

Goal: let users import brushes the way Procreate does — **`.brush`**, **`.brushset`**, and **`.brushlibrary`** — into `BrushLibrary` sets, with tip/grain assets and a **best-effort** parameter map into `BrushPreset`.

**Status:** T1-7-1…T1-7-6 + **T1-9** StampEngine shipped. Best-effort Procreate map — **not** “Procreate compatible engine.” List chips use **engine strip** first (QuickLook is not truth). Open: Photoshop `.abr` ([T1-7-7](ROADMAP.md#t1-7--procreate-style-brush-import)).

Procreate’s formats are **proprietary** (Savage Interactive). There is no official public schema. Community reverse-engineering (ZIP + PNG + `Brush.archive` bplist) is good enough for **import**, not for claiming 1:1 engine parity.

### Package shapes (observed)

| File | Shape | Contents (typical) |
|------|-------|--------------------|
| `.brushset` | ZIP | Many brush folders + often `brushset.plist` |
| `.brush` | ZIP (single brush) | One brush folder |
| `.brushlibrary` | ZIP of sets | **Done** (T1-7-6); treat as multiple `.brushset` |

Per-brush folder (names vary by Procreate version):

```text
<BrushName>/
  Brush.archive          # binary plist (NSKeyedArchiver) — parameters
  Shape.png / tip PNG    # brush tip / stamp
  Grain.png              # optional texture
  Preview.png / QuickLook thumbnail
  (optional author / signature images)
```

**ponytail:** Treat archive layout as version-sensitive. Detect keys; skip unknown; never crash the app on a weird pack.

### Pipeline

```text
UI (DocumentPicker / drop)
  → bytes + filename
  → CanvasEditor.importBrushPackage(pathOrBytes, kind)
       │
       ├─ 1. Unzip (Compression / lib in C++ or Swift unzip → temp dir)
       ├─ 2. Discover brush folders + Brush.archive
       ├─ 3. Decode bplist → key/value map (ObjC/Swift NSPropertyListSerialization
       │     or C++ plist reader; NSKeyedArchiver may need ObjC++ shim)
       ├─ 4. Map known keys → BrushPreset fields (see table)
       ├─ 5. Store tip/grain/preview PNGs in BrushAssetStore → textureIds
       ├─ 6. Create BrushSet (name from archive / filename) + append presets
       └─ 7. Return setId + count imported / skipped
```

**Ownership split:**

| Layer | Owns |
|-------|------|
| **UI** | File picker, drag-drop, progress, “Imported” set chrome |
| **Engine** | Parse, map, asset store, library mutation, self-check on fixture |
| **Shim (if needed)** | ObjC++ helper only for NSKeyedArchiver decode — keep out of `CanvasEditor.hpp` |

### Parameter map (best-effort)

Procreate key names change across versions; maintain a **synonym table** in `src/tools/procreate/ProcreateBrushMap`. Values below are conceptual targets, not guaranteed key strings.

| Our `BrushPreset` | Map from Procreate-ish params | Notes |
|-------------------|------------------------------|--------|
| `lineWidthPx` | max size / diameter | Scale normalized Procreate sizes (×40 when <8) |
| `lineSmooth` | streamline / stabilization | Normalize 0..1 |
| `hardness` | hardness / softness | Invert if source is “softness” |
| `opacity` | opacity / paintOpacity | |
| `flow` | flow / accumulation | |
| `spacing` | plotSpacing / spacing | Fraction of diameter |
| `minSize` | minSize + size curve Y0 | Size at pressure 0 |
| `sizePressure` | dynamicsPressureSize + size curve | 0 = off |
| `opacityPressure` | dynamicsPressureOpacity + curve | |
| `taperSize` | taperSize / pencilTaperSize | Stroke-start taper |
| `taperOpacity` | taperOpacity | |
| `orientTip` | oriented | Rotate tip to stroke |
| `shapeInverted` | shapeInverted | Force tip invert |
| `grainScale` | textureScale | Grain tiling |
| `roundness` / `angleDeg` | shapeRoundness / shapeRotation | |
| `tipTextureId` | Shape / tip PNG | Required for textured brushes |
| `grainTextureId` | Grain PNG | Multiplied into tip coverage |
| `previewTextureId` | Preview PNG | UI only |
| `mode` | paint vs eraser flag if present | Default Paint; user can assign to Eraser tool |

**Unsupported in v1 (store as ignored extras or drop):** wet mix, dual brush, smudge-only tips, full editable pressure *graphs* (endpoints only), Apple Pencil squeeze, render modes we do not have.

Document in UI: **“Imported — approximated”** when any required key is missing or unmapped.

### Rasterizer impact

| Tip | Behavior |
|-----|----------|
| No tip texture | Procedural round dab |
| Tip PNG (`tipTextureId`) | Stamp tip (bilinear, invert dark-on-light or `shapeInverted`, silhouette). Skip AuthorPicture/Signature. |
| Grain | Multiply into tip coverage ([T1-7-3b](ROADMAP.md#t1-7--procreate-style-brush-import)) |

`.brushset`: folders are UUIDs; display names come from `Brush.archive` (`SilicaBrush` stub decode). Set name + order from `brushset.plist`.

### Public API (import)

Shipped on `CanvasEditor` — see [API.md](API.md). Signatures:

```cpp
enum class BrushPackageKind : int32_t {
    Auto = 0,   // sniff .brush / .brushset / .brushlibrary
    Brush = 1,
    BrushSet = 2,
    BrushLibrary = 3
};

/// Returns new BrushSet id, or -1 on failure. outBrushCount optional.
int32_t importBrushPackage(const char* path, BrushPackageKind kind,
                           int32_t* outBrushCount);

int32_t importBrushPackageBytes(const uint8_t* data, int32_t size,
                                BrushPackageKind kind,
                                const char* suggestedName,
                                int32_t* outBrushCount);

int32_t brushSetSource(int32_t setIndex) const;           // 0 BuiltIn, 1 User, 2 ImportedProcreate
bool brushPresetApproximated(int32_t setIndex, int32_t presetIndex) const;
bool setBrushPresetInSet(int32_t setIndex, int32_t presetIndex);
```

DrawingEditor: `presentBrushImport()` / `fileImporter` + approximated badge in Brush Library widget.

### Phased import delivery

| Phase | Roadmap | Status | Deliverable |
|-------|---------|--------|-------------|
| Unpack + assets | **T1-7-1** | done | Unzip `.brush` / `.brushset`; register PNGs; create set |
| Param map | **T1-7-2** | done | Decode `Brush.archive`; map size/opacity/spacing/smooth/hardness/pressure |
| Tip assets | **T1-7-3** | done | Import/store tip PNG; stamp deferred (procedural round) |
| Public + listing APIs | **T1-7-4** | done | `importBrushPackage*`, `brushSetSource`, approximated |
| Library UX | **T1-7-5** | done | Swift Import UI; list chips = **engine strip** (QuickLook fallback) |
| `.brushlibrary` | **T1-7-6** | done | Multi-set package |
| Tip silhouette + grain | **T1-7-3b / T1-9** | done | Soft tip mask + Moving/Texturized grain, scatter/count, taper |
| Photoshop `.abr` | **T1-7-7** | open | Later if needed |

### Legal / product note

- Import is for **user-owned** brush files they bring into the app.
- Do not ship or redistribute Procreate’s default brush packs.
- Do not claim “Procreate compatible engine” — claim **“Import Procreate brush files (best-effort)”**.

### Self-check (import)

- Fixture tiny `.brush` (zip in test resources) → `importBrushPackage` → set count +1, tip texture non-null if PNG present.
- Mapped `lineWidthPx` within expected clamp.
- Stroke with imported tip darkens layer.

---

## Eraser

| Topic | Decision |
|-------|----------|
| Buffer | No separate erase layer |
| Blend | Dest-out on **active layer** texture only |
| Vector | Recorded as `Stroke` with erase preset |
| Undo | Remove erase stroke → re-raster layer tiles from remaining strokes |
| Cross-layer | Never erases other layers |

---

## Color Fill (Procreate-style ColorDrop)

Flood-fill a closed region on the **active layer** with the session color — Procreate’s ColorDrop. Complements brush/eraser: ink with strokes, then drop flats into closed shapes.

**Status:** design only — implement as [T1-8](ROADMAP.md#t1-8--color-fill-colordrop).  
**Inspiration:** [Procreate ColorDrop](https://help.procreate.com/articles/zmlayd-fill-an-area-using-colordrop) + Reference Layer (not a 1:1 clone).

### Summary

| | |
|--|--|
| **Inputs** | Seed point (canvas space); fill color (session RGBA); **threshold** 0…1; optional Reference layer; Continue Filling taps |
| **State** | `FillSession { threshold, continueFilling }`; at most **one** layer marked `isReference`; fill writes **active layer** pixels only |
| **API** | `ToolMode::Fill`; `fillAt` / `previewFillAt` / `commitFill`; `setFillThreshold` / `fillThreshold`; `setLayerReference` / `layerIsReference`; `setContinueFilling` |
| **Rules** | Flood from seed until boundary; threshold controls bleed into edges; Reference layer supplies boundaries while paint lands on active; never fill a layer group |
| **v1 out** | Gradient fill, pattern fill, fill into selection (until selection exists), fill whole layer from Layers menu (can add as thin wrapper), recolor-replace |

### UX (match Procreate feel)

| Gesture | Behavior |
|---------|----------|
| **ColorDrop** | Drag active color disc onto canvas → seed at drop point → flood with session color |
| **Threshold adjust** | Hold after drop (don’t lift) → show threshold bar → drag **left = less bleed**, **right = more bleed** → live preview → lift to **commit** |
| **Remember threshold** | Last committed threshold persists for next ColorDrop (session + UserDefaults) |
| **Continue Filling** | After a commit, optional mode: tap other closed regions to fill with same color/threshold (no re-drag from disc); exit on tool change / confirm |
| **Fill tool** | `ToolMode::Fill`: tap = fill at point (same engine path as ColorDrop); hold-drag adjusts threshold like ColorDrop |
| **Reference** | Layers panel: mark one layer as Reference (line art). ColorDrop on a **different** active layer uses Reference pixels as flood boundaries |

```text
                    ColorDrop / Fill tap
                            │
                            ▼
              seed (canvas x,y) + color + threshold
                            │
         ┌──────────────────┴──────────────────┐
         ▼                                     ▼
   Boundary source                      Paint target
   (Reference layer if set,             (active layer only)
    else active layer)
         │                                     │
         └──────── flood mask ─────────────────┘
                            │
                            ▼
              write fill color into active (src-over / replace)
              dirty AABB → GPU layer upload → present
```

### Boundary & threshold semantics

Flood-fill walks 4-connected (v1) neighbors from the seed.

| Concept | Rule |
|---------|------|
| **Seed color** | Sample boundary-source pixel at seed (premultiplied RGBA). Transparent seed → fill transparent / near-empty region until opaque ink |
| **Same region** | Neighbor is “inside” if color distance to seed ≤ `threshold` (see metric below) |
| **Boundary** | Neighbor fails the threshold test → stop (do not paint that pixel) |
| **Canvas edge** | Always a hard stop |
| **Gaps in line art** | Fill leaks through gaps — expected; user raises threshold carefully or closes gaps with brush |
| **Threshold 0** | Strict: only near-exact seed color (tight pockets) |
| **Threshold ~1** | Aggressive: bleeds through soft / anti-aliased edges into neighbors (can flood whole layer) |

**Color distance (v1):** max-channel delta in straight RGBA, or CIE-ish luma+chroma later:

```text
dist(a, b) = max(|a.r-b.r|, |a.g-b.g|, |a.b-b.b|, |a.a-b.a|) / 255
inside     = dist(neighbor, seedColor) <= threshold
```

**ponytail:** Start with max-channel + 4-connected CPU scan. If AA fringing is ugly, upgrade to 8-connected + separate alpha weight, or a 1px morphological “close gaps” pass — don’t ship both on day one.

**Anti-flood guard:** If preview mask area &gt; ~95% of layer and threshold was not explicitly maxed, clamp remembered threshold slightly below 1 (Procreate remembers ~97.6% instead of 100%) and/or require confirm — avoid accidental full-canvas fills.

### Reference layer

| Rule | Detail |
|------|--------|
| Count | **At most one** `isReference == true` layer in the document |
| Setting | `setLayerReference(layerId, true)` clears any previous reference |
| Fill target | Always **active** layer (must not be the Reference layer itself for the “ink vs color” workflow; if active == reference, behave as normal same-layer fill) |
| Boundary read | Sample **Reference** layer RGBA (ignore its opacity/visibility for boundary? **v1:** use raw layer buffer; skip if reference invisible — treat as no reference) |
| Paint write | Active layer only; Reference pixels unchanged |
| Background Layer | May be Reference (unusual); cannot remove Background |

Cartoon / manga workflow: Line layer = Reference; Color layer = active → ColorDrop flats without destroying ink.

### Hybrid / data model

Fill is **raster-first** (unlike brush strokes). Do not pretend a flood is a vector polyline.

```text
FillOp {
  layerId           // paint target (active at commit)
  referenceLayerId  // -1 if none
  seedX, seedY      // canvas px
  color RGBA
  threshold         // 0..1 snapshot at commit
  bounds            // dirty AABB of painted pixels
  // undo payload (T3): RLE mask of written pixels OR tile snapshots before write
}
```

| Phase | Behavior |
|-------|----------|
| Preview | Compute mask into scratch; composite fill color over active **without** committing; update present dirty rect |
| Commit | Apply mask → active layer pixels; append `FillOp` to layer op list (or history); clear preview |
| Undo (T3) | Restore previous pixels under mask (tile snapshot / RLE inverse) — **not** “re-flood” |
| Re-raster strokes | Fill sits **above** regenerable stroke raster for that layer **or** is baked into layer RGBA after strokes — pick one: |

**Decision (v1):** Bake fill into the **layer RGBA cache** like a committed raster edit. Stroke list stays authoritative for ink; fills are separate `FillOp` records so undo can remove them without replaying every stroke. On full layer rebuild (rare): replay strokes then replay `FillOp`s in order.

**ponytail:** Until T3, commit fill directly to layer pixels + keep last `FillOp` for a single-level undo if cheap; otherwise rely on upcoming history.

### Live preview (threshold drag)

1. On ColorDrop hold: run flood at current threshold → preview overlay (or write to transient texture).
2. On horizontal drag: remap pointer delta → threshold; **re-flood** from same seed (debounce to 1× per frame / 8ms).
3. On lift: commit; persist threshold.
4. Cancel: Escape / second finger / tool change → discard preview, no pixel change.

Large canvas: flood only within a growing AABB or tile set; abort if mask exceeds budget and show “threshold too high”.

### Engine vs UI

| Layer | Owns |
|-------|------|
| **UI** | Color disc drag → canvas seed; threshold bar chrome; Continue Filling banner; Reference toggle in Layers panel; confirm on huge fills |
| **Engine** | Flood algorithm, threshold metric, Reference sampling, preview/commit, dirty rects, `FillOp` record |
| **Not engine** | Color picker UI, disc hit-testing in Swift chrome |

### Public API (planned)

```cpp
enum class ToolMode : int32_t {
    Brush = 0,
    Eraser = 1,
    Pointer = 2,
    Fill = 3,   // tap-to-fill + threshold hold
};

/// Threshold 0..1 (bleed). Persisted by UI; engine keeps last set value.
void setFillThreshold(float t);
float fillThreshold() const;

/// Preview flood (no commit). outBounds optional AABB of mask.
bool previewFillAt(float x, float y, float threshold,
                   float* outMinX, float* outMinY, float* outMaxX, float* outMaxY);

/// Commit flood with session color + threshold (or explicit args).
bool commitFillAt(float x, float y, float threshold);

/// One-shot: preview+commit with current fillThreshold + brush color (ColorDrop lift).
bool fillAt(float x, float y);

void setContinueFilling(bool on);
bool continueFilling() const;

bool setLayerReference(int32_t layerId, bool isReference);
bool layerIsReference(int32_t layerId) const;
int32_t referenceLayerId() const;   // -1 if none
```

Color for fill = current `setBrushColor` / session color (same disc). No separate fill-color API in v1.

**Swift ColorDrop path:** map disc-drop view point → `viewToCanvas*` → `previewFillAt` while held → `commitFillAt` on lift. Continue Filling: subsequent taps call `fillAt`.

### Algorithm sketch (CPU v1)

```text
seed = round(x,y) clamped to layer
src  = referenceLayerId >= 0 ? layer(reference) : layer(active)
dst  = layer(active)
seedColor = src.pixel(seed)
queue BFS/scanline from seed
for each pixel p in region where dist(src[p], seedColor) <= threshold:
    if preview: mask[p] = 1
    else: dst[p] = srcOver(fillColor, dst[p])  // or replace if fillOpacity==1
track dirty AABB
upload dirty tiles to GPU layer texture
```

**Scanline fill** preferred over naive BFS for cache locality on large flats.

**GPU later (optional):** compute shader flood is non-trivial (multi-pass / label propagate). Keep CPU for v1; Metal only if Instruments shows fill &gt; frame budget on iPad.

### Interaction with other features

| Feature | Interaction |
|---------|-------------|
| Brush / Eraser | Switching tool exits Continue Filling; commits or cancels preview |
| Alpha lock (later) | Fill only where active layer alpha &gt; 0 |
| Selection (later) | Clip flood mask to selection |
| Layer opacity / blend | Fill writes pre-blend layer pixels; opacity applies at composite |
| Background Layer | Fill allowed if active; usually user fills paint layers instead |
| Undo T3 | `FillCommand` with mask RLE + previous texels (or tile snapshots) |
| Timelapse | Log `FillOp` seed/color/threshold/bounds |
| Export SVG | Fills stay raster (embedded) unless later vectorized |

### File / module layout (add)

```text
src/
  tools/
    FillSession.hpp          // threshold, continueFilling
  render/
    FloodFill.hpp/.cpp       // scanline flood; preview mask; apply to layer
  // FillOp record beside strokes/ or layers/
```

### Phased delivery (T1-8)

| Phase | Roadmap | Deliverable |
|-------|---------|-------------|
| Core flood | **T1-8-1** | CPU scanline fill on active layer; `fillAt` / threshold; dirty upload |
| ColorDrop UX | **T1-8-2** | Swift: drag color disc → preview + threshold bar → commit; remember threshold |
| Continue Filling | **T1-8-3** | Post-commit tap-to-fill mode; exit chrome |
| Reference layer | **T1-8-4** | `isReference` flag; boundary from reference; Layers panel toggle |
| History | **T1-8-5** | `FillCommand` for T3 (can land with T3-*) |
| Self-check | **T1-8-6** | Closed box fixture; threshold leak; reference isolation |

### Self-check targets (fill)

1. Closed black ring on transparent layer → `fillAt` center → interior ≈ fill color; exterior unchanged.
2. Gap in ring + high threshold → fill leaks outside (document expected).
3. Low threshold → stops at anti-aliased edge sooner than high threshold.
4. Reference layer has ring; active empty → fill paints active interior; reference pixels unchanged.
5. Fill does not modify non-active layers.
6. Preview then cancel → active layer hash unchanged.

### Out of scope (fill v1)

- Gradient / pattern / texture fill  
- Fill Layer menu that ignores seed (whole-layer tint) — trivial wrapper later  
- Recolor (replace color range)  
- Vector “fill path” from stroke closed contour  
- Multi-layer / group fill  
- On-device ML gap closing  

---

## Undo / redo interaction (design now, implement T3)

Even if **T3** lands later, **T1** stroke storage must not paint itself into a corner:

- On `endStroke`, engine is ready to emit `StrokeCommand { layerId, stroke }` for history.
- On move/adjust commit: `TransformStrokeCommand { layerId, strokeId, dx, dy }` or `EditStrokePointCommand { … index, oldPt, newPt }` (inverse = easy undo).
- On fill commit: `FillCommand { layerId, FillOp }` with RLE mask + previous texels (or dirty-tile snapshots) — inverse restores pixels under the mask.
- Undo = detach stroke / inverse transform / restore fill mask (not store 8MB bitmaps per stroke when RLE fits).
- Timelapse op log can store the same stroke / fill payload with timestamps.

---

## File / module layout (`src/`)

```text
src/
  tools/
    BrushPreset.hpp
    BrushSession.hpp           // pre-draw overrides (lineWidth, lineSmooth, …)
    BrushLibrary.hpp/.cpp
    BrushAssetStore.hpp/.cpp   // tip/grain/preview PNG bytes
    FillSession.hpp            // threshold, continueFilling (T1-8)
    procreate/
      ProcreateBrushImporter.hpp/.cpp
      ProcreateBrushMap.mm     // bplist key → BrushPreset (ObjC++)
      ZipReader.* / FixtureBrushBytes.*
  strokes/
    Stroke.hpp                 // samples + lazy cubics (ensureCubics)
    StrokeSample.hpp
    // StrokeEdit — planned with T2-6
    // FillOp — planned with T1-8
  render/
    StrokeRasterizer.hpp/.cpp  // CPU procedural round → layer
    MetalStrokeRasterizer.*    // compute dab (not hot path)
    LayerCompositor.*          // GPU blend
    FloodFill.hpp/.cpp         // scanline ColorDrop (T1-8)
  math/
    Blend.hpp / Rect.hpp / TileGrid.hpp
    Bezier.hpp/.cpp            // Eigen fit (lazy / export)
    PresentTransform.hpp/.cpp  // scalar NDC + GLM MVP helper
```

Stroke input smoothing + rasterize live in `IllusStudioCanvasEditor` (no separate `StrokeEngine`). Wire through the facade; expose only what Swift needs on `CanvasEditor`.

---

## Implementation mapping (see ROADMAP T1)

| Design slice | Roadmap |
|--------------|---------|
| Vector stroke + CPU raster + brush library + session props + eraser | **T1-1** |
| Dirty tiles + (retired) live overlay; paint stamps into layer | **T1-2** |
| Metal compute rasterizer | **T1-3** |
| GPU layer composite | **T1-4** |
| Procreate `.brush` / `.brushset` import | **T1-7** |
| Color Fill / ColorDrop (+ Reference layer) | **T1-8** |
| Easy move / adjust line | **T2-6** |
| Bézier fit / tilt / tile index | **T2-7** |

Do not track `[x]` / `[ ]` here — only in [ROADMAP.md](ROADMAP.md).

---

## Performance contract

| Path | Budget mindset |
|------|----------------|
| Sample append | O(1) / event |
| Rasterize segment | O(dirty pixels × dabs in segment) — tile clipped |
| Present | No full CPU layer stack walk every frame once **T1-4** lands; NDC = scalar (no GLM @ 120Hz) |
| Curve fit | Off hot path — Eigen only on export / demand |
| Move / adjust | Re-raster **union(old,new) bounds** tiles on **one layer** only |
| Hit-test | O(strokes on that layer × samples) with bounds reject; spatial index later if needed |
| Undo | Re-raster **stroke bounds** tiles from vector, not whole canvas unless bounds ≈ full frame |
| Fill (ColorDrop) | Flood + preview re-flood ≤ 1× / frame; scanline; abort if mask ≫ budget |
| Memory | Lazy layer textures; empty layers allocate nothing (keep current rule) |

**Shared device:** `metalDeviceAddress()` for MTKView (already required).

**Mutex:** Keep `CanvasEditor` lock across stroke + present so MTKView and gestures do not race (already in place).

---

## API sketch (public)

**Shipped** (see [API.md](API.md) / `CanvasEditor.hpp`): `ToolMode`, brush set/preset listing, session setters, `importBrushPackage*`, stroke stream, `strokeCountOnLayer`.

**Planned (T1-8)** — Color Fill / ColorDrop:

```cpp
// ToolMode gains Fill = 3
void setFillThreshold(float t);
float fillThreshold() const;
bool previewFillAt(float x, float y, float threshold,
                   float* outMinX, float* outMinY, float* outMaxX, float* outMaxY);
bool commitFillAt(float x, float y, float threshold);
bool fillAt(float x, float y);
void setContinueFilling(bool on);
bool continueFilling() const;
bool setLayerReference(int32_t layerId, bool isReference);
bool layerIsReference(int32_t layerId) const;
int32_t referenceLayerId() const;
```

**Planned (T2-6)** — layer-scoped vector query + edit:

```cpp
int32_t strokeIdAt(int32_t layerId, int32_t index) const;
bool strokeBounds(int32_t layerId, int32_t strokeId,
                  float* outMinX, float* outMinY, float* outMaxX, float* outMaxY) const;
int32_t hitTestStroke(int32_t layerId, float x, float y, float radius) const;
int32_t copyStrokePolyline(int32_t layerId, int32_t strokeId,
                           float* outXY, int32_t maxPoints) const;
bool translateStroke(int32_t layerId, int32_t strokeId, float dx, float dy);
bool setStrokePoint(int32_t layerId, int32_t strokeId, int32_t pointIndex, float x, float y);
```

Internal: never expose `std::vector<Stroke>` to Swift — use count + copy-into-buffer APIs.

---

## Self-checks (required)

One runnable check covering hybrid invariants:

1. **Paint:** during/after stroke, active layer pixels painted; other layers unchanged.
2. **Erase:** paint then erase same path → pixels near transparent / background show-through on composite.
3. **Session props:** `setBrushLineWidth` before stroke changes dab size; mid-stroke change does not alter in-flight `presetSnapshot`.
4. **Line smooth:** `lineSmooth > 0` reduces high-frequency jitter vs raw samples (distance metric in self-check).
5. **Vector integrity:** after `endStroke`, layer stroke count += 1; undo (when **T3** exists) restores prior hash of layer tiles.
6. **Move / adjust:** stroke on layer A; `translateStroke` shifts polyline; layer B unchanged; wrong `layerId` cannot see A’s stroke.
7. **Import (T1-7):** fixture `.brush` → set +1; tip id set when PNG present; stamp stays bright green (procedural, not tip grain).
8. **Overlap quality:** crossing soft lime strokes stay bright green at intersection (not dark/noisy).
9. **Device:** `metalAvailable` ⇒ present texture non-null; Swift uses engine device.
10. **Fill (T1-8):** closed ring → interior filled; reference isolation; cancel preview leaves hash unchanged.

`CanvasEditor::selfCheck()` covers paint/erase/session/import/viewport/Bezier/overlap smoke paths (+ fill when T1-8 lands).

---

## Migration note

Pipeline: append vector samples → StampEngine (`StrokeRasterizer`) into **layer** pixels → dirty GPU upload → `LayerCompositor` present. Live-overlay paint was tried (T1-2) and **retired** for quality. Tip+grain uses silhouette + Moving/Texturized grain (T1-9), not raw Shape.png coverage. List previews are **engine-true**; QuickLook is fallback only (not the fidelity bar).

---

## Beyond Procreate (north star)

After StampEngine fidelity is good enough for patterned brushes:

1. **Editable vector strokes (T2-6)** — move/reshape/recolor without raster corruption.
2. **Manga tools** — screen-tone presets, Color Fill (T1-8), panel-aware hatch later.
3. **GPU StampEngine (T1-9-5)** — tip+grain at 120Hz once the CPU model is correct.

Still out: wet mix / dual / smudge parity; “Procreate compatible” marketing.

---

## Out of scope (this doc)

- Smudge, dual brush, wet mix (Procreate wet mix not mapped 1:1)  
- Layer masks / groups  
- Multi-stroke lasso transform / document-wide vector dump  
- Pressure/tilt re-paint while moving (v1 move is geometric translate; brush params stay on `presetSnapshot`)  
- Full Illustrator-grade Bézier toolset (**T2-6** = polyline move + point adjust; rich curve UI later)  
- Photoshop `.abr` import (Procreate can import ABR; we can add later as **T1-7-7**)  
- Gradient / pattern fill; vector closed-path fill; ML gap closing (see Color Fill § Out of scope)  
- Shipping or redistributing Procreate’s default brush packs  
- Claiming full Procreate brush-engine parity  
- Replacing SwiftUI page COP structure  

---

## Summary

Implement brushes and eraser as **vector strokes** that are **rasterized into per-layer Metal (or CPU) caches** and **composited on the GPU** for display. Users adjust **line width, line smooth, hardness, opacity, …** on a **`BrushSession` before drawing**; each stroke freezes a `presetSnapshot`. **Procreate `.brush` / `.brushset` / `.brushlibrary` import** unpacks ZIP + tip PNGs + best-effort `Brush.archive` mapping into `BrushLibrary` sets — not a 1:1 engine clone. **Color Fill (ColorDrop)** floods flats into closed regions with threshold + optional Reference layer for ink/color separation.

**Shipped:** T1-1…T1-4, T1-7, T1-9 BrushModel v2, T2-7-1. **Next:** T1-8 Color Fill, T2-6 move/adjust (beyond-Procreate), T1-9-5 GPU stamp, T3 history, T4 import/export. Checkboxes: [ROADMAP.md](ROADMAP.md).
