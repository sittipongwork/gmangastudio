# Layer management

Feature spec for the layer stack (order, opacity, visibility, merge, blend modes, etc.). Expand this file as the design grows.

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [canvas_document.md](canvas_document.md) · [brush_drawing.md](brush_drawing.md) · [AGENTS.md](../../AGENTS.md)

---

## Summary

| | |
|--|--|
| **Inputs** | Add/delete/duplicate/reorder; set opacity/visibility/blend; set active layer |
| **State** | Ordered `Layer` list (top = front); each layer has RGBA buffer (or tiles later), id, name, opacity, visible, blend |
| **API** | `addLayer` / `removeLayer` / `setActiveLayer` / opacity+visibility / `duplicateLayer` / `moveLayer` / `mergeLayerDown` / `copyLayerThumbnailRGBA`; blend getters-setters (**planned**, T0-12) |
| **Rules** | New project seeds **Background Layer** (back) + **Layer 1** (front, active). `addLayer` inserts at front as **Layer N**. New paint layers default blend = **Normal (`N`)**. Strokes apply only to active layer; display = composite |
| **v1 out** | Layer groups, masks; engine ships Normal first and adds the catalog below incrementally |

---

## Detail

- Index **0 = front** (top of Layers panel); last index = back.
- Background Layer: `isBackground == true`; cannot remove / duplicate / move off the back. Owns the page white fill — hide it to reveal transparent composite (view clear color).
- Page underlay (`PageSettings.background`) is transparent; `setBackground` / `clearAll` write the fill onto Background Layer.
- `addLayer(nullptr)` / empty / `"Layer"` → auto-name `Layer 1`, `Layer 2`, …; blend mode **`N` (Normal)**.
- UI thumbs: `copyLayerThumbnailRGBA` (document aspect; refresh after paint / layer edits) — **done** (T0-11 / TX-6).
- Layers panel blend badge + picker: **planned** (T0-12-7); not in DrawingEditor yet.
- **Reference layer** (Color Fill): at most one layer with `isReference`; ColorDrop uses it as flood boundary while painting the active layer — see [brush_drawing.md](brush_drawing.md) § Color Fill · ROADMAP T1-8-4.

---

## Layer blending modes (Procreate-style)

Composite walks **back → front**. For each visible layer with opacity &gt; 0:

1. Compute RGB blend of **upper (A)** over **lower composite (B)** using the layer’s mode (formulas below; channels in **0…1** linear).
2. Apply layer **opacity** (and alpha of A) via standard src-over / Porter–Duff with the blended RGB.

**Convention:** `A` = upper layer color (source), `B` = backdrop (destination). Clamp results to `[0, 1]` after each mode unless noted.

**Default:** every new paint layer is **`N` / Normal**. Background Layer is also Normal (its fill is opaque white).

**UI note:** Procreate-style badges collide for some modes (`H` = Hard Light **and** Hue; `S` = Screen **and** Saturation; `L` = Lighten **and** Luminosity). Engine enum must be unique; UI may use the same letter in context of the mode picker group, or a longer label on hover.

| UI | Mode | Group | Primary use | Formula (core) |
|:--:|:-----|:-----|:------------|:---------------|
| **N** | **Normal** | Default | Standard rendering; upper obscures lower by opacity | \(Blend(A,B) = A\) |
| **M** | **Multiply** | Darken | Shadows; removes white, darkens uniformly | \(Blend(A,B) = A \times B\) |
| **K** | **Darken** | Darken | Keep darkest of A vs B | \(Blend(A,B) = \min(A,B)\) |
| **CB** | **Color Burn** | Darken | Deep, heavy shadows; more contrast | \(Blend(A,B) = 1 - \dfrac{1 - B}{A}\) (protect \(A=0\)) |
| **LB** | **Linear Burn** | Darken | Darker / more saturated than Multiply | \(Blend(A,B) = A + B - 1\) |
| **S** | **Screen** | Lighten | Specular highlights; removes black | \(Blend(A,B) = 1 - (1-A)(1-B)\) |
| **L** | **Lighten** | Lighten | Keep brightest of A vs B | \(Blend(A,B) = \max(A,B)\) |
| **CD** | **Color Dodge** | Lighten | Extreme glow; brightens backdrop | \(Blend(A,B) = \dfrac{B}{1 - A}\) (protect \(A=1\)) |
| **A** | **Add** (Linear Dodge) | Lighten | Sci‑fi glows / FX; additive | \(Blend(A,B) = A + B\) |
| **O** | **Overlay** | Contrast | Texture / tint; Multiply+Screen mix | If \(B &lt; 0.5 \rightarrow 2AB\), else \(1 - 2(1-A)(1-B)\) |
| **H** | **Hard Light** | Contrast | Stronger Overlay keyed on upper | If \(A &lt; 0.5 \rightarrow 2AB\), else \(1 - 2(1-A)(1-B)\) |
| **SL** | **Soft Light** | Contrast | Gentle lighting shifts | Non-linear / quadratic (match Procreate / PDF Soft Light) |
| **E** | **Exclusion** | Difference | Soft invert vs Difference | \(Blend(A,B) = A + B - 2AB\) |
| **H** | **Hue** | Component | Tint only; keep sat + luminosity of B | HSL/HSV: replace \(H_B\) with \(H_A\) |
| **S** | **Saturation** | Component | Purity only; keep hue + luminosity of B | HSL/HSV: replace \(S_B\) with \(S_A\) |
| **C** | **Color** | Component | Color B&W sketches; hue+sat of A, lum of B | \(H_A, S_A\) + \(L_B\) |
| **L** | **Luminosity** | Component | Brightness of A; colors of B | \(L_A\) + \(H_B, S_B\) |

### Functional groups (UI picker)

| Group | Modes |
|-------|--------|
| Default | Normal |
| Darken | Multiply, Darken, Color Burn, Linear Burn |
| Lighten | Screen, Lighten, Color Dodge, Add |
| Contrast | Overlay, Hard Light, Soft Light |
| Difference | Exclusion |
| Component | Hue, Saturation, Color, Luminosity |

### Engine / API (planned)

- `enum class BlendMode` — unique cases for every row above (do not overload enum on colliding UI letters). Internal `Layer` already has `BlendMode::Normal` only.
- `setLayerBlendMode(layerId, mode)` / `layerBlendMode(layerId)` — **not public yet**.
- Composite (`SoftwareRenderer` for export / self-check; GPU `LayerCompositor` on present) — **src-over / Normal only** today; other modes [T0-12](ROADMAP.md#t0-12--layer-blending-modes-procreate-style).
- Self-check: known A/B fixtures for Multiply / Screen / Overlay at least.

### Out of scope (for now)

- Difference (hard), Subtract, Divide, and other Procreate extras not in the table.
- Per-channel blend, blend-if, knockout groups.
