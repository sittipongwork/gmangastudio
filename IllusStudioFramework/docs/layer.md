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
| **Rules** | Strokes apply only to active layer; display = composite |
| **v1 out** | Layer groups, masks, full blend-mode catalog (start: Normal + Erase) |

---

## Detail

_(Add blend modes, groups/masks, merge semantics, stroke-list ownership per layer, etc. here.)_
