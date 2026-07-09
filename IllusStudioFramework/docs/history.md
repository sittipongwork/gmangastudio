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

- Design hooks land with hybrid strokes (vector `Stroke` is undo-friendly); full stack tracked in ROADMAP.
- Prefer inverse commands over bitmap snapshots per stroke.
- Paint undo vs timeline undo stay separate where possible (see [animation_timeline.md](animation_timeline.md)).

_(Add command types, coalescing, memory caps, timelapse export, etc. here.)_
