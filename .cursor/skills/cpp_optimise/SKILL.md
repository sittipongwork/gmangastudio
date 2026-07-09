---
name: cpp_optimise
description: >-
  C++ memory and allocation optimization for IllusStudioFramework (canvas,
  layers, Metal present). Use when profiling, fixing leaks, reducing
  allocations, improving frame time, or when the user mentions memory,
  Instruments, Address Sanitizer, dirty-rect, or cpp_optimise.
---

# C++ Optimise — Memory

Apply when changing `IllusStudioFramework/` C++ (especially `layers/`, `render/`, `IllusStudioCanvasEditor`, `MetalRenderer`). Stay ponytail: measure first, smallest fix that holds 120fps.

## Memory

Canvas documents are large (e.g. 1920×1080×4 ≈ 8MB per full RGBA buffer). Layer count multiplies that. Prefer:

- **Reuse buffers** — `composite_`, `belowCache_`, Metal texture; resize only on page size change
- **Dirty rects** — never full-frame blend/upload while stroking unless structure changed
- **Below-cache** — composite layers under active once per stroke session; don't rebuild every dab
- **No per-event heap** — avoid `std::vector` grow/shrink, `new[]`, or temporary full-frame copies on the stroke/present path
- **Shared ownership only at API edge** — `CanvasEditor` may use `shared_ptr` for Swift; hot path stays contiguous `uint8_t` storage
- **Metal** — dirty `replaceRegion` only; don't recreate `MTLTexture` every frame

Budget mindset: stroke path cost should be **O(dirty pixels × layers above active)**, not **O(canvas × all layers)**.

## Core Memory Management Skills

When writing or reviewing engine code:

1. **Own one clear owner** — layer pixels owned by `Layer`; composite owned by editor; Metal texture owned by `MetalRenderer`
2. **RAII** — no bare `new`/`delete` on hot paths; prefer `std::vector` / unique ownership inside impl
3. **Reserve / assign once** — `pixels.assign(n, 0)` on create/resize; clear with `fill` or `clearTransparent`, don't reallocate
4. **Move, don't copy** — never copy full layer buffers unless duplicating a layer intentionally
5. **Integer blend on hot path** — avoid float per-pixel in inner loops (`math/Blend.hpp`)
6. **Skip empty work** — invisible / zero-opacity / zero-alpha pixels early-out
7. **Don't cache wrong** — invalidate below-cache on layer add/remove/reorder/active change/opacity/visibility/background; keep it across strokes on the same active layer
8. **Public API stays thin** — no `std::vector` in `CanvasEditor.hpp`; no accidental copies across Swift–C++ boundary

Checklist before merging a perf-related change:

- [ ] No full-canvas composite on every pointer move
- [ ] No full-texture upload on every pointer move (dirty rect OK)
- [ ] No new heap allocation in `stamp` / `continueStroke` / `presentMetalTexture`
- [ ] Layer add doesn't leave stale caches (markDirty)

## Profiling & Debugging Skills

**macOS / Xcode (preferred for this app)**

| Goal | Tool |
|------|------|
| CPU hotspots while drawing | Instruments → Time Profiler (filter `IllusStudio` / `SoftwareRenderer` / `stamp`) |
| Allocations / retain spikes | Instruments → Allocations |
| Leaks | Instruments → Leaks; or Address Sanitizer |
| Metal | Metal System Trace / GPU Frame Capture |
| Frame pacing (120Hz) | Quartz Debug / os_signpost; watch MTKView present vs stroke work |

**Sanitizers (Debug)**

- Address Sanitizer (ASan) — use-after-free, buffer overflow on layer/composite buffers
- Undefined Behavior Sanitizer (UBSan) — optional for blend/index math

**How to profile a drawing regression**

1. Reproduce: many layers + continuous stroke at 1920×1080
2. Time Profiler: confirm time in full composite vs dirty-rect path
3. Allocations: confirm zero persistent growth during stroke; spikes only on layer add/resize
4. If GPU-bound: check texture recreate vs `replaceRegion`; drawable size vs canvas size
5. Fix the shared hot function once (editor/renderer), not each Swift call site

**Quick local checks**

- `CanvasEditor::selfCheck()` / `IllusStudioCanvasEditor::selfCheck()` still pass after memory changes
- Add a tiny assert/demo only if new non-trivial caching logic lands (ponytail)

## Memory Leaks & Allocations

**Common leak / growth sources in this codebase**

| Symptom | Likely cause | Fix direction |
|---------|--------------|---------------|
| RSS grows every stroke | Composite/temp buffer reallocated or copied each event | Reuse `composite_` / dirty rect only |
| RSS grows every Add Layer | Expected (new layer buffer); check for duplicate full caches | One buffer per layer; don't clone composite |
| Metal device/texture leak | `newTexture` without `release` on resize/destroy | `MetalRenderer` destructor / replace path |
| Swift side growth | Holding extra `Data`/`CGImage` copies | Prefer Metal present path only |
| Use-after-free | Returning pointers into reallocated `vector` | Don't hold `compositePixels()` across mutations |

**Allocation rules**

- **Forbidden on stroke/present hot path:** `push_back` that may reallocate full frames, `std::vector` temporaries of canvas size, logging that formats huge buffers
- **Allowed:** stack locals, fixed brush stamp loops, dirty-rect unions, Metal `replaceRegion`
- **Layer duplicate:** one intentional full copy; then stop

**Leak hunt recipe**

1. Run under ASan or Instruments Leaks
2. Add/remove layers in a loop; stroke; clear — RSS should return near baseline after clear/remove (minus allocator fragmentation)
3. Confirm `MetalRenderer` releases texture/device/queue
4. Confirm `CanvasEditor` / `IllusStudioCanvasEditor` destruction frees layer vectors

## Out of scope unless asked

- Premature SIMD / Accelerate without a profile
- New third-party allocators or memory pools
- Full GPU layer composite (future); keep CPU composite correct first
