# History (undo / redo / timelapse)

Feature spec for command history and timelapse recording.

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [brush_drawing.md](brush_drawing.md) · [layer.md](layer.md) · [AGENTS.md](../../AGENTS.md)

---

## Summary

| | |
|--|--|
| **Inputs** | Undo/redo requests; automatic push on committed ops |
| **State** | Command stack (`StrokeCommand`, `LayerCommand`, …); separate append-only **timelapse op log** with timestamps |
| **API** | `undo` / `redo` / `canUndo` / `canRedo` (planned) |
| **Timelapse** | Record ops, not full framebuffer every stroke; replay composites when exporting |
| **v1 out** | Branching history; cloud sync of op logs |

---

## Detail

- Vector `Stroke` lists per layer are **in place** (T1) — undo-friendly storage is ready.
- Full command stack **not started** (`src/history/` absent). No `undo` / `redo` / `canUndo` / `canRedo` on `CanvasEditor.hpp`.
- Prefer inverse commands over bitmap snapshots per stroke.
- Paint undo vs timeline undo stay separate where possible (see [animation_timeline.md](animation_timeline.md)).
- Self-check (when T3 lands): undo restores prior active-layer / stroke-list hash.

_(Add command types, coalescing, memory caps, timelapse export, etc. here.)_
