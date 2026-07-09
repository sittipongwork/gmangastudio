# Hybrid drawing — Brush library & eraser

Feature spec + design for brush library, eraser, and the **Hybrid (Vector + Raster)** pipeline so the app keeps vector integrity (undo, edit, sharp export) and raster fluidity (soft brushes, blur, fill, 120fps present).

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [layer.md](layer.md) · [canvas_document.md](canvas_document.md) · [history.md](history.md) · [AGENTS.md](../../AGENTS.md)

---

## Summary

| | |
|--|--|
| **Inputs** | Tool mode (brush/eraser); preset id; **pre-draw** line width, line smooth, hardness, opacity, flow, spacing, pressure gains, color; pointer stream with pressure; optional Procreate `.brush` / `.brushset` import |
| **State** | `BrushLibrary` + sets; `BrushSession` overrides; vector strokes on active layer; regenerable raster cache; tip/grain assets |
| **API** | Stroke stream + session setters; layer-scoped stroke query/edit (move/adjust); `importBrushPackage` (**T1-7**) |
| **Eraser** | Dest-out on active layer; recorded as vector stroke |
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
| Fluidity & effects | Raster cache + Metal: soft dabs, hardness falloff, blur/fill later |
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
  ├── tools/           BrushPreset library, ToolMode, StrokeEngine
  ├── strokes/         Vector Stroke / StrokeSample (Data Layer)
  ├── history/         StrokeCommand (T3; design for it with T1)
  ├── layers/          Layer id + optional GPU texture handle
  └── render/
        ├── StrokeRasterizer   Vector → layer texture (Compute or CPU)
        ├── LayerCompositor    Blend layer textures → present texture
        └── MetalRenderer      Present / device (T6 done)
```

| Hybrid layer | Module / type | Notes |
|--------------|---------------|--------|
| Input | UI + `beginStroke` / `continueStroke` / `endStroke` | Canvas-space only; viewport maps in UI or `viewport/` |
| Data (Vector) | `tools/` + `strokes/` | Never treat layer RGBA as the only stroke store |
| Cache (Raster) | Per-layer `MTLTexture` (+ optional CPU tile fallback) | Dirty tiles only |
| Display | GPU blend + existing MTKView path | Prefer blend layer textures; CPU composite is fallback / self-check |

**Current code (pre-T1):** Soft brush stamps **directly** into a CPU layer buffer, then composites and uploads one present texture. **T1** migrates that stamp path into: append vector samples → rasterize into **layer** cache → GPU composite.

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
  path               // optional: fitted cubic Bézier segments (lazy, on endStroke or idle)
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
| `beginStroke` | Create `Stroke` on active layer; append first sample | Clear “live” overlay tiles; stamp first dab |
| `continueStroke` | Append samples; update bounds | Incremental rasterize new segment only |
| `endStroke` | Commit stroke into layer’s stroke list; optional curve fit | Merge live tiles into layer texture; clear live overlay |
| Move / adjust | Mutate that stroke’s samples/path; recompute bounds | Invalidate old∪new bounds → re-raster affected tiles |
| Undo | Remove last stroke (or apply inverse / un-transform) | Invalidate stroke bounds → re-raster affected tiles from remaining strokes |

### Curve fitting (optional in T1-1, recommended in T2-7)

- **T1-1 minimum:** store dense `StrokeSample[]`; rasterizer walks samples with dab spacing (same as today).
- **T2-7-1:** on `endStroke`, fit cubics (`math/` Bézier) for storage + SVG; keep samples if fit error is high.
- Fitting must be **lossy compression of input**, not a second source of truth that diverges from what was painted — rasterize from the same representation you store, or re-raster from samples after fit only if visual delta is within epsilon (self-check).

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

Plus a **live stroke overlay** (one texture or tile set), composited above the active layer during the stroke so unfinished strokes do not rewrite the whole layer every dab.

### Vector → raster (real-time)

```text
StrokeRasterizer
  input:  Stroke (or sample polyline) + BrushPreset + clip rect
  output: writes into layer texture / live overlay (dirty region)
```

**Preferred path (performance):** Metal **Compute** kernel:

- Threadgroup covers dirty tile (e.g. 32×32 or 16×16).
- For each pixel, accumulate coverage from dabs along the new segment (or evaluate distance to polyline + hardness curve).
- Paint: `src-over` with brush color × opacity × pressure curve.
- Erase: `dest-out` with coverage × opacity × pressure.

**Fallback path:** existing CPU stamp in `IllusStudioCanvasEditor::stamp` → `replaceRegion` upload (**T6**). Keep until compute shader is proven; same dirty-rect contract.

### Invalidation rules

| Event | Invalidate |
|-------|------------|
| New dab / segment | Tiles intersecting dab AABB |
| `endStroke` | Merge live → layer; mark present dirty |
| Move / adjust stroke | Tiles intersecting **union(old bounds, new bounds)**; rebuild those tiles from that layer’s strokes |
| Undo/redo stroke | Tiles intersecting that stroke’s `bounds`; rebuild from vector |
| Clear layer | Release texture + clear stroke list |
| Resize canvas | Full rebuild (v1 out: reflow) |

### Separation from Display

- Cache = **per-layer** (and live overlay).
- Do **not** bake the full document into one CPU buffer as the only cache (current **T0** composite can remain as fallback / export helper).
- Present path should evolve: **blend layer textures on GPU** → one drawable (Display). Until then: CPU composite + single upload remains acceptable as an intermediate.

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
- Target **120fps** present while stroking: only dirty tiles + live overlay update; full-stack blend can be full-frame but bandwidth-bound — keep layer count and resolution in mind.
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

Built-ins (minimal set):

| Id | Name | Mode | Notes |
|----|------|------|--------|
| `ink.round` | Round Ink | Paint | Hardness ~0.9, low lineSmooth |
| `air.soft` | Soft Airbrush | Paint | Hardness ~0.2, higher spacing |
| `erase.soft` | Soft Eraser | Erase | Dest-out, soft falloff |
| `erase.hard` | Hard Eraser | Erase | Dest-out, hard edge |

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

### Dab model (v1)

- Round procedural tip if no `tipTextureId`; else stamp tip texture (import path).
- Coverage = `smoothstep` by hardness from center (or tip alpha × hardness).
- Effective width = `lineWidthPx * mix(1, pressure, sizePressure)`.
- Spacing: step ≤ `spacing * effectiveWidth` along the segment.
- Color: straight alpha or premultiplied; match `math/Blend.hpp` conventions.

---

## Procreate-style brush import

Goal: let users import brushes the way Procreate does — **`.brush`**, **`.brushset`**, and later **`.brushlibrary`** — into `BrushLibrary` sets, with tip/grain assets and a **best-effort** parameter map into `BrushPreset`.

Procreate’s formats are **proprietary** (Savage Interactive). There is no official public schema. Community reverse-engineering (ZIP + PNG + `Brush.archive` bplist) is good enough for **import**, not for claiming 1:1 engine parity.

### Package shapes (observed)

| File | Shape | Contents (typical) |
|------|-------|--------------------|
| `.brushset` | ZIP | Many brush folders + often `brushset.plist` |
| `.brush` | ZIP (single brush) | One brush folder |
| `.brushlibrary` | ZIP of sets | Later (**T1-7**); treat as multiple `.brushset` |

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

Procreate key names change across versions; maintain a **synonym table** in `tools/ProcreateBrushMap`. Values below are conceptual targets, not guaranteed key strings.

| Our `BrushPreset` | Map from Procreate-ish params | Notes |
|-------------------|------------------------------|--------|
| `lineWidthPx` | max size / diameter | Scale to canvas px; clamp |
| `lineSmooth` | streamline / stabilization | Normalize 0..1 |
| `hardness` | hardness / softness | Invert if source is “softness” |
| `opacity` | opacity | |
| `flow` | flow / accumulation | |
| `spacing` | spacing | Often % of size |
| `sizePressure` | size pressure graph → gain | v1: sample curve endpoints → linear gain |
| `opacityPressure` | opacity pressure graph → gain | same |
| `roundness` / `angleDeg` | shape dynamics | Store even if rasterizer ignores until later |
| `tipTextureId` | Shape / tip PNG | Required for textured brushes |
| `grainTextureId` | Grain PNG | Optional; multiply in dab later |
| `previewTextureId` | Preview PNG | UI only |
| `mode` | paint vs eraser flag if present | Default Paint; user can assign to Eraser tool |

**Unsupported in v1 (store as ignored extras or drop):** wet mix, dual brush, smudge-only tips, full pressure *graphs* as editable curves, Apple Pencil squeeze, render modes we do not have.

Document in UI: **“Imported — approximated”** when any required key is missing or unmapped.

### Rasterizer impact

| Tip | Behavior |
|-----|----------|
| No tip texture | Procedural round dab (current path) |
| Tip PNG | Stamp tip centered on dab, scaled to effective width, rotated by `angleDeg` |
| Grain | Optional multiply (T1-7 after tip stamps work) |

Import without tip stamp support is still useful: map numeric params onto round brushes so line width / smooth / opacity feel close.

### Public API (import)

```cpp
enum class BrushPackageKind : int32_t {
    Auto = 0,   // sniff .brush / .brushset / .brushlibrary
    Brush = 1,
    BrushSet = 2,
    BrushLibrary = 3
};

/// Import from filesystem path (UI copies into app sandbox first if needed).
/// Returns new BrushSet id, or -1 on failure. outBrushCount optional.
int32_t importBrushPackage(const char* path, BrushPackageKind kind,
                           int32_t* outBrushCount);

/// Optional: import from memory (AirDrop / in-memory drop).
int32_t importBrushPackageBytes(const uint8_t* data, int32_t size,
                                BrushPackageKind kind, int32_t* outBrushCount);

int32_t brushSetCount() const;
const char* brushSetName(int32_t setIndex) const;
int32_t brushPresetCountInSet(int32_t setIndex) const;
// … preset name / select by (setIndex, presetIndex)
```

### Phased import delivery

| Phase | Roadmap | Deliverable |
|-------|---------|-------------|
| Unpack + assets | **T1-7-1** | Unzip `.brush` / `.brushset`; register PNGs; create set with **default** round params + tip if present |
| Param map | **T1-7-2** | Decode `Brush.archive`; map size/opacity/spacing/smooth/hardness/pressure gains |
| Tip stamp raster | **T1-7-3** | `StrokeRasterizer` stamps tip texture (CPU then Metal) |
| Library UX | **T1-7-4** | Swift: Import button, Imported set list, approximate badge |
| Grain / library file | **T1-7-5** | Grain multiply; `.brushlibrary` as multi-set |

### Legal / product note

- Import is for **user-owned** brush files they bring into the app.
- Do not ship or redistribute Procreate’s default brush packs.
- Do not claim “Procreate compatible engine” — claim **“Import Procreate brush files (best-effort)”**.

### Self-check (import)

- Fixture tiny `.brush` (zip in test resources) → `importBrushPackage` → set count +1, tip texture non-null if PNG present.
- Mapped `lineWidthPx` within expected clamp.
- Stroke with imported tip darkens layer (after T1-7-3).

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

## Undo / redo interaction (design now, implement T3)

Even if **T3** lands later, **T1** stroke storage must not paint itself into a corner:

- On `endStroke`, engine is ready to emit `StrokeCommand { layerId, stroke }` for history.
- On move/adjust commit: `TransformStrokeCommand { layerId, strokeId, dx, dy }` or `EditStrokePointCommand { … index, oldPt, newPt }` (inverse = easy undo).
- Undo = detach stroke / inverse transform + invalidate bounds (not store 8MB bitmaps per stroke).
- Timelapse op log can store the same stroke payload with timestamps.

---

## File / module layout (add under `src/`)

```text
src/
  tools/
    BrushPreset.hpp
    BrushSession.hpp           // pre-draw overrides (lineWidth, lineSmooth, …)
    BrushLibrary.hpp/.cpp
    BrushAssetStore.hpp/.cpp   // tip/grain/preview PNG bytes
    ToolMode.hpp
    StrokeEngine.hpp/.cpp      // smooth input → samples + rasterize
    procreate/
      ProcreateBrushImporter.hpp/.cpp
      ProcreateBrushMap.hpp/.cpp   // bplist key → BrushPreset
  strokes/
    Stroke.hpp/.cpp
    StrokeSample.hpp
    StrokeEdit.hpp/.cpp    // hit-test, translate, setPoint (layer-scoped)
  render/
    StrokeRasterizer.hpp/.cpp  // CPU first; tip stamp; Metal compute next
    LayerCompositor.hpp/.cpp   // GPU blend path (can follow rasterizer)
  math/
    Dab.hpp                    // spacing, hardness falloff, lineSmooth filter
    Bezier.hpp                 // optional fit
```

Wire through `IllusStudioCanvasEditor`; expose only what Swift needs on `CanvasEditor`.

---

## Implementation mapping (see ROADMAP T1)

| Design slice | Roadmap |
|--------------|---------|
| Vector stroke + CPU raster + brush library + session props + eraser | **T1-1** |
| Live overlay + dirty tiles | **T1-2** |
| Metal compute rasterizer | **T1-3** |
| GPU layer composite | **T1-4** |
| Easy move / adjust line | **T2-6** |
| Bézier fit / tilt / tile index | **T2-7** |
| Procreate `.brush` / `.brushset` import | **T1-7** |

Do not track `[x]` / `[ ]` here — only in [ROADMAP.md](ROADMAP.md).

---

## Performance contract

| Path | Budget mindset |
|------|----------------|
| Sample append | O(1) / event |
| Rasterize segment | O(dirty pixels × dabs in segment) — tile clipped |
| Present | No full CPU layer stack walk every frame once **T1-4** lands |
| Move / adjust | Re-raster **union(old,new) bounds** tiles on **one layer** only |
| Hit-test | O(strokes on that layer × samples) with bounds reject; spatial index later if needed |
| Undo | Re-raster **stroke bounds** tiles from vector, not whole canvas unless bounds ≈ full frame |
| Memory | Lazy layer textures; empty layers allocate nothing (keep current rule) |

**Shared device:** `metalDeviceAddress()` for MTKView (already required).

**Mutex:** Keep `CanvasEditor` lock across stroke + present so MTKView and gestures do not race (already in place).

---

## API sketch (public)

```cpp
// CanvasEditor.hpp — additive; names illustrative
enum class ToolMode : int32_t { Brush = 0, Eraser = 1, Pointer = 2 };

void setTool(ToolMode mode);
ToolMode tool() const;

int32_t brushSetCount() const;
const char* brushSetName(int32_t setIndex) const;
int32_t brushPresetCount() const;                    // flat, or use InSet variants
int32_t brushPresetCountInSet(int32_t setIndex) const;
const char* brushPresetName(int32_t index) const;
void setBrushPreset(int32_t index);

// Pre-draw session (BrushSession) — adjust before drawing
void setBrushLineWidth(float px);
float brushLineWidth() const;
void setBrushLineSmooth(float s);                    // 0..1
float brushLineSmooth() const;
void setBrushHardness(float h);                      // 0..1
void setBrushOpacity(float a);                       // 0..1
void setBrushFlow(float f);                          // 0..1
void setBrushSpacing(float spacing);                 // fraction of width
void setBrushSizePressure(float g);                  // 0..1
void setBrushOpacityPressure(float g);               // 0..1
void setBrushColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void resetBrushSession();
int32_t saveBrushSessionAsPreset(const char* name);  // new user preset id

// Procreate-style import (T1-7)
int32_t importBrushPackage(const char* path, int32_t kind, int32_t* outBrushCount);
int32_t importBrushPackageBytes(const uint8_t* data, int32_t size,
                                int32_t kind, int32_t* outBrushCount);

void beginStroke(float x, float y, float pressure);
void continueStroke(float x, float y, float pressure);
void endStroke();

// --- Layer-scoped vector query + edit (move / adjust line) ---
// All of these operate on a single layerId (use activeLayerId() when UI is "inside" a layer).

int32_t strokeCountOnLayer(int32_t layerId) const;
int32_t strokeIdAt(int32_t layerId, int32_t index) const;   // index in layer stroke list
bool strokeBounds(int32_t layerId, int32_t strokeId,
                  float* outMinX, float* outMinY, float* outMaxX, float* outMaxY) const;

/// Hit-test strokes on one layer only. Returns stroke id, or -1.
int32_t hitTestStroke(int32_t layerId, float x, float y, float radius) const;

/// Copy polyline points for UI handles. Returns count written (or needed if out==null).
int32_t copyStrokePolyline(int32_t layerId, int32_t strokeId,
                           float* outXY, int32_t maxPoints) const;

bool translateStroke(int32_t layerId, int32_t strokeId, float dx, float dy);
bool setStrokePoint(int32_t layerId, int32_t strokeId, int32_t pointIndex, float x, float y);
```

Internal: never expose `std::vector<Stroke>` to Swift — use count + copy-into-buffer APIs (Swift-friendly).

---

## Self-checks (required)

One runnable check covering hybrid invariants:

1. **Paint:** after stroke, active layer pixels darkened; other layers unchanged.
2. **Erase:** paint then erase same path → pixels near transparent / background show-through on composite.
3. **Session props:** `setBrushLineWidth` before stroke changes dab size; mid-stroke change does not alter in-flight `presetSnapshot`.
4. **Line smooth:** `lineSmooth > 0` reduces high-frequency jitter vs raw samples (distance metric in self-check).
5. **Vector integrity:** after `endStroke`, layer stroke count += 1; undo (when **T3** exists) restores prior hash of layer tiles.
6. **Move / adjust:** stroke on layer A; `translateStroke` shifts polyline; layer B unchanged; wrong `layerId` cannot see A’s stroke.
7. **Import (T1-7):** fixture `.brush` → set +1; tip id set when PNG present.
8. **Device:** `metalAvailable` ⇒ present texture non-null; Swift uses engine device.

`CanvasEditor::selfCheck()` should call into `StrokeEngine` / rasterizer / `StrokeEdit` checks.

---

## Migration from today’s stamp path

| Today | Hybrid target |
|-------|----------------|
| `stamp` writes layer CPU RGBA immediately | `stamp` becomes implementation detail of `StrokeRasterizer` |
| No stroke list | `Stroke` list per layer |
| One composite upload | Per-layer cache → GPU blend (phased) |
| Brush hardcoded gray soft round | `BrushPreset` + eraser mode |

Do **not** keep a parallel “pixels-only” stroke path after **T1-1** — one pipeline, CPU or Metal backend behind `StrokeRasterizer`.

---

## Out of scope (this doc)

- Smudge, dual brush, wet mix (Procreate wet mix not mapped 1:1)  
- Layer masks / groups  
- Multi-stroke lasso transform / document-wide vector dump  
- Pressure/tilt re-paint while moving (v1 move is geometric translate; brush params stay on `presetSnapshot`)  
- Full Illustrator-grade Bézier toolset (**T2-6** = polyline move + point adjust; rich curve UI later)  
- Photoshop `.abr` import (Procreate can import ABR; we can add later as **T1-8**)  
- Shipping or redistributing Procreate’s default brush packs  
- Claiming full Procreate brush-engine parity  
- Replacing SwiftUI page COP structure  

---

## Summary

Implement brushes and eraser as **vector strokes** that are **rasterized into per-layer Metal (or CPU) caches** and **composited on the GPU** for display. Users adjust **line width, line smooth, hardness, opacity, …** on a **`BrushSession` before drawing**; each stroke freezes a `presetSnapshot`. **Procreate `.brush` / `.brushset` import** (**T1-7**) unpacks ZIP + tip PNGs + best-effort `Brush.archive` mapping into `BrushLibrary` sets — not a 1:1 engine clone.

Ship order and checkboxes: [ROADMAP.md](ROADMAP.md) — **T1-1** (incl. session props) → **T1-2** / **T1-3** / **T1-4** → **T1-7** (import; tip stamp can follow T1-1) → **T2-6** / **T2-7**.
