# Canvas document

Feature spec for the canvas document: page settings, viewport (zoom/pan), and export.

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [layer.md](layer.md) · [brush_drawing.md](brush_drawing.md) · [AGENTS.md](../../AGENTS.md)

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
| **API** | `setViewport`, view↔canvas helpers (planned) |
| **Rules** | UI may drive gestures; **engine owns** transform math; tools receive **canvas-space** points |
| **v1 out** | Rotate canvas, snap-to-pixel UI chrome |

### Detail

- Viewport is a present-time transform: pan/zoom must **not** re-raster strokes.
- Pointer events reaching tools are always in **canvas space** (UI or engine maps view → canvas).
- Self-check target: view→canvas→view round-trip within epsilon.

_(Add min/max scale, focus-point zoom, fit-to-view, etc. here.)_

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
