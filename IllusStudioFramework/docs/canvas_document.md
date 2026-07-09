# Canvas document

Feature spec for the canvas document: page settings, viewport (zoom/pan), gizmo mode, app active status, and export.

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [API.md](API.md) · [layer.md](layer.md) · [brush_drawing.md](brush_drawing.md) · [AGENTS.md](../../AGENTS.md)

---

## Page setting

| | |
|--|--|
| **Inputs** | Width, height, background RGBA (optional DPI later) |
| **State** | `PageSettings { width, height, background }`; background as clear color or locked bottom layer |
| **API** | `CanvasEditor(w,h)`, `setBackground(...)`, size getters |
| **v1 out** | Arbitrary canvas resize with content reflow; print templates catalog |

### Detail

_(Add page templates, DPI, resize rules, background-as-layer vs clear color, etc. here.)_

---

## Zoom & pan

| | |
|--|--|
| **Inputs** | Scale, pan delta, or absolute `Viewport { scale, offset }`; optional focus point |
| **State** | Viewport transform; dirty rects in canvas space |
| **API** | `setViewport`, `viewportScale` / offsets, `viewToCanvas*` / `canvasToView*` |
| **Rules** | UI may drive gestures; **engine owns** transform math; tools receive **canvas-space** points |
| **v1 out** | Rotate canvas, snap-to-pixel UI chrome |

### Detail

- Viewport is a present-time transform: pan/zoom must **not** re-raster strokes.
- Pointer events reaching tools are always in **canvas space** (UI or engine maps view → canvas).
- Self-check target: view→canvas→view round-trip within epsilon.

_(Add min/max scale, focus-point zoom, fit-to-view, etc. here.)_

---

## Gizmo mode

Overlay for inspecting / editing vector stroke geometry. Independent of paint tools; default is off.

| | |
|--|--|
| **Inputs** | `GizmoMode` (`None` \| `Vector`); pointer drag when editable |
| **State** | `gizmoMode` on editor (present-time overlay; does not mutate pixels until a point edit commits) |
| **API** | `setGizmoMode(mode)` / `gizmoMode()` (planned) |
| **Rules** | Overlay is **view chrome** (drawn in present / UI); stroke edits go through canvas-space APIs (`setStrokePoint`, … — see [brush_drawing.md](brush_drawing.md) T1-5) |
| **v1 out** | Bézier handles, multi-stroke lasso, rotate/scale gizmo, snap guides |

### Modes

| Mode | Show gizmo | Edit |
|------|------------|------|
| **`None`** (default) | Hidden | — |
| **`Vector`** | Vector / polygon points + connecting lines | In **`ToolMode::Pointer` only**: drag a point to adjust |

### Vector gizmo look

- **Points:** one handle per stroke sample (polyline vertices) on the **active layer** (v1).
- **Lines:** segments between consecutive points; **4px solid** stroke (screen/view pixels — constant width under zoom).
- Gizmo is drawn **above** the composite; not part of export / layer pixels.

### Pointer + Vector interaction

1. User sets `GizmoMode::Vector` (gizmos visible).
2. User selects `ToolMode::Pointer` (no paint).
3. Hit-test nearest point (or segment) in canvas space → drag updates that vertex via `setStrokePoint` (or equivalent).
4. On drag: invalidate stroke bounds → re-raster that layer (same path as T1-5).
5. If tool is Brush / Eraser, gizmos may still **show** when mode is `Vector`, but **drag does not edit** points (paint/erase wins).

### Detail

```text
enum class GizmoMode : int32_t {
    None = 0,    // default — no gizmo
    Vector = 1,  // points + 4px solid polyline
};

void setGizmoMode(GizmoMode mode);
GizmoMode gizmoMode() const;
```

- Engine owns mode state + hit-test / `setStrokePoint`; UI may draw the overlay from `copyStrokePolyline` **or** engine may composite a gizmo pass at present — pick one in T1-5 / present work; prefer UI overlay if cheaper for 120Hz.
- Self-check target: `None` → no edit path; `Vector` + Pointer drag moves a sample; Brush + Vector does not move samples on drag.

---

## App active status

UI-owned present performance gate. Keeps **120fps** while the user is working; drops to a low-power present when idle.

| | |
|--|--|
| **Inputs** | User activity (tap / click, move, touch, pan, zoom, tool change, …) |
| **State** | `AppActiveStatus` + `lastUse` timestamp (`lastuse_counting`) |
| **API** | App / DrawingEditor owns this (not required on `CanvasEditor`) |
| **Rules** | Any activity → `performance_mode` and **reset** `lastUse`; idle ≥ **60s** → `low_energy_mode` |
| **v1 out** | Per-window status when multi-document; background-app policy beyond idle |

### Statuses

| Status | Default | Present |
|--------|---------|---------|
| **`performance_mode`** | Yes | High performance — continuous present @ **120fps** (when display supports it) |
| **`low_energy_mode`** | After idle | Low refresh (**~10fps**) continuous present — lower GPU/CPU/display load; restore 120fps on next activity |

### Idle rule (`lastuse_counting`)

1. On enter / first show: `status = performance_mode`, `lastUse = now`.
2. On **any** user action (tap, move, touch, scroll/pan, zoom, sidebar, toolbar): set `lastUse = now`; if was `low_energy_mode`, set `performance_mode` and restore 120fps present.
3. If `now - lastUse >= 60s` with no activity: set `status = low_energy_mode` and drop present to low refresh (~10fps).
4. Mouse-only hover without click/drag does **not** count as activity (v1).

### Detail

```text
enum AppActiveStatus {
    case performanceMode  // default — 120fps present (performance_mode)
    case lowEnergyMode    // idle ≥ 60s — ~10fps present (low_energy_mode)
}

let idleTimeout: TimeInterval = 60
let lowEnergyPresentFPS = 10
```

- Self-check: activity resets idle; after timeout without activity → `low_energy_mode`; activity from `low_energy_mode` → `performance_mode`.
- `low_energy_mode` stays on a continuous present path (not fully paused) so the canvas can still refresh, but at a low rate to decrease hardware use.
---

## Export (PNG / SVG / TIFF)

| | |
|--|--|
| **Inputs** | Format enum (`PNG` / `SVG` / `TIFF`); options: include background, flatten vs preserve structure, DPI/resolution scale, current frame (animation) |
| **State** | Read-only snapshot of composite (and layer list for structured SVG); no mutation of document |
| **API** | `export(format, options, …)` (planned) |
| **PNG** | Flattened RGBA composite → lossless PNG (alpha preserved); primary raster export |
| **TIFF** | Flattened composite → TIFF (RGBA, optional DPI tags); prefer for print / high-bit workflows later |
| **SVG** | Vector-first where possible: background rect + per-layer groups; raster layer contents as embedded PNG when needed; true path SVG when stroke vector data exists |
| **Rules** | UI owns file picker / share sheet; engine owns encode. Export uses **document pixels**, not viewport zoom |
| **v1 out** | PSD/PDF, multi-page TIFF stacks, animated GIF/APNG, CMYK TIFF |

### Format matrix

| Format | Kind | Source | Notes |
|--------|------|--------|-------|
| PNG | Raster | Flattened composite | Default share/export; alpha OK |
| TIFF | Raster | Flattened composite | DPI metadata; good for archival/print |
| SVG | Hybrid | Layers → groups; bitmaps embedded if needed | Best effort vector; not a full Illustrator path model in v1 |

### Detail

_(Add export options schema, per-layer export, animation frame selection, etc. here.)_
