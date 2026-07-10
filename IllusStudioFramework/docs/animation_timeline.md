# Animation & timeline

Feature spec for frames / cels, playhead, onion-skin, and timeline ops.

**Tasks & status:** [ROADMAP.md](ROADMAP.md)  
Related: [README.md](../README.md) · [canvas_document.md](canvas_document.md) · [layer.md](layer.md) · [history.md](history.md) · [AGENTS.md](../../AGENTS.md)

---

## Summary

| | |
|--|--|
| **Inputs** | Add/remove frame (cel), set fps, playhead, onion-skin range |
| **State** | Timeline: ordered frames; each frame references layer snapshot or cel stack; playhead index |
| **API** | Timeline methods on `CanvasEditor` (planned); paint still goes through tools on current frame’s layers |
| **Rules** | **Paint undo** vs **timeline undo** stay separate where possible (document in API comments) |
| **v1 out** | Audio scrub, complex tweening, GIF/MP4 export polish |

---

## Detail

**No code started** — `src/animation/` absent; `CanvasEditor.hpp` has zero timeline methods. Tracked as [T5](ROADMAP.md#t5--animation--timeline).

_(Add cel model, onion-skin compositing, fps / playback, export of animated sequences, etc. here.)_
