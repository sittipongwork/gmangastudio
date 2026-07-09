# Layer management

Feature spec for the layer stack (order, opacity, visibility, merge, etc.). Expand this file as the design grows.

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [canvas_document.md](canvas_document.md) · [brush_drawing.md](brush_drawing.md) · [AGENTS.md](../../AGENTS.md)

---

## Summary

| | |
|--|--|
| **Inputs** | Add/delete/duplicate/reorder; set opacity/visibility/blend; set active layer |
| **State** | Ordered `Layer` list (top = front); each layer has RGBA buffer (or tiles later), id, name, opacity, visible, blend |
| **API** | `addLayer` / `removeLayer` / `setActiveLayer` / opacity+visibility / `duplicateLayer` / `moveLayer` / `mergeLayerDown` |
| **Rules** | New project seeds **Background Layer** (back) + **Layer 1** (front, active). `addLayer` inserts at front as **Layer N**. Strokes apply only to active layer; display = composite |
| **v1 out** | Layer groups, masks, full blend-mode catalog (start: Normal + Erase) |

---

## Detail

- Index **0 = front** (top of Layers panel); last index = back.
- Background Layer: `isBackground == true`; cannot remove / duplicate / move off the back. Owns the page white fill — hide it to reveal transparent composite (view clear color).
- Page underlay (`PageSettings.background`) is transparent; `setBackground` / `clearAll` write the fill onto Background Layer.
- `addLayer(nullptr)` / empty / `"Layer"` → auto-name `Layer 1`, `Layer 2`, …
- UI thumbs: `copyLayerThumbnailRGBA` (document aspect; refresh after paint / layer edits).

_(Add blend modes, groups/masks, merge semantics, stroke-list ownership per layer, etc. here.)_
