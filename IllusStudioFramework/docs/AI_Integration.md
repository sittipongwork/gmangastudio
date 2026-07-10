# AI integration

Catalog of features that use **AI** (generation / vision / assist) on top of IllusStudioCanvasEditor.  
Engine still owns layers, pixels, and import; UI + AI service own prompts, model calls, and result handoff.

**Tasks & status:** [ROADMAP.md](ROADMAP.md) (add AI tasks when scheduled)  
Related: [brush_drawing.md](brush_drawing.md) (image import / placement) · [layer.md](layer.md) · [README.md](../README.md) · [AGENTS.md](../../AGENTS.md)

---

## Feature index

| Feature | Status | Summary |
|---------|--------|---------|
| [Import reference image](#1-import-reference-image) | planned | Reference photo → AI line-art (+ flat color) → detect line/color → separate layers |

**Dependency:** engine image import (`importRGBA`, [T4-1](ROADMAP.md#t4--import--export)) is not implemented yet — AI handoff waits on that API. Schedule AI work in [ROADMAP.md](ROADMAP.md) when ready.

---

## 1. Import reference image

Turn a user photo / concept image into **coloring-ready line art** on the canvas, then split **line** and **color** onto separate layers so the artist can ink/color further.

### Flow

```text
UI: pick image + subject category
        │
        ▼
AI service: image-to-image (reference + category prompt)
        │
        ▼
Result: line-art (optionally with solid flat colors)
        │
        ▼
App: detect line vs color regions
        │
        ├─→ Layer "Line"   (ink / outlines)
        └─→ Layer "Color"  (flat fills; optional further splits by region)
        │
        ▼
Canvas: placed like normal import (move + 8-handle resize — see brush_drawing.md)
```

### Steps

1. **Import image** — user picks a reference (same picker path as raster import).
2. **Select what it is** — subject category drives the prompt template, e.g.:
   - Humanoid
   - Props
   - Tree & flower
   - _(extend as needed)_
3. **AI generate** — image-to-image with the category prompt; output is **line-art** suitable for coloring (bold outer outlines, plain clothes/props, no background/effects).
4. **Detect & separate layers** — app analyzes the AI result:
   - **Line layer** — dark / high-contrast outlines
   - **Color layer(s)** — solid flat fills (skin, hair, cloth, …) when the model returns colored flats

### Ownership

| Layer | Owns |
|-------|------|
| **UI** | Picker, category chips, progress, accept/reject AI result, layer naming |
| **AI service** | Model call, prompt assembly from category + reference, retries |
| **Engine** | Receive RGBA (or multi-buffer) → create layers / place transform; no model SDK inside C++ core |

**note:** Keep model SDKs and API keys in the app / a thin Swift service. Framework only accepts pixels + layer ops so self-check and headless paths stay free of network.

### Subject categories (v1)

| Id | Label | Prompt focus |
|----|-------|--------------|
| `humanoid` | Humanoid | Character silhouette, skin / hair / cloth flats, bold outer ink |
| `props` | Props | Object outline, plain surfaces, no busy patterns |
| `tree_flower` | Tree & flower | Organic silhouette, leaf/petal regions, clean outer boundary |

### Prompt sample — humanoid

Use as the default template for `humanoid` (tune per model; keep the coloring map explicit):

```text
Use image reference to style for ready to coloring, A decorative line art illustration, highly refined and detailed, The prompt explicitly calls for "ultra-bold, heavy, dark, and consistent outer boundary outlines" that define everything. This direct address ensures the model focuses on this aspect, Using "finished ink sketch ready for coloring" helps the model understand the level of finish and precision, no background no effect, the clothes and props must be plain (no patterns), coloring human orange, hair blue, cloth red and Use image reference to style for ready to coloring, A decorative line art illustration, highly refined and detailed, The prompt explicitly calls for "ultra-bold, heavy, dark, and consistent outer boundary outlines" that define everything. This direct address ensures the model focuses on this aspect, Using "finished ink sketch ready for coloring" helps the model understand the level of finish and precision, no background no effect, the clothes and props must be plain (no patterns). Coloring solid color by Human skin is orange, hair is blue, cloth and accessories is green
```

**Intent of the sample (normalize when wiring code):**

| Cue in prompt | Why |
|---------------|-----|
| Reference + “ready to coloring” | Image-to-image, not free invent |
| Ultra-bold, heavy, consistent outer outlines | Clean ink layer for detection |
| Finished ink sketch | Finish level / precision |
| No background, no effect | Easier matte / layer split |
| Clothes & props plain (no patterns) | Flat color regions |
| Solid color map (skin orange, hair blue, cloth green/red) | Deterministic flats for **color → layer** separation |

### Line / color detection (app)

After AI returns an image:

1. Threshold / morphology → **line mask** (dark strokes) → write to Line layer (or vectorize later).
2. Flood / region grow on non-line pixels → **flat color regions**; map known cue colors (orange/blue/green/…) to named layers when present, else one Color layer.
3. Place both (all) layers with the same initial `Placement` so move/resize stays in sync until the user unlinks them.

_(Detail algorithms, tolerances, and API once implementation starts.)_

### Relation to plain image import

- Non-AI import: [brush_drawing.md — Image import](brush_drawing.md#image-import-placed-raster) (move + 8-corner resize).
- AI path: same placement UX after layers are created; reference original may stay as a locked/hidden reference layer (optional).

### v1 out

- On-device model shipping inside the framework binary
- Guaranteed perfect region labels without cue colors
- Multi-character auto-rig / pose transfer

---

## Adding a new AI feature

1. Add a row to **Feature index** above.
2. Write a section: flow, ownership (UI / AI service / engine), prompts or inputs, handoff into `CanvasEditor`.
3. Schedule work in [ROADMAP.md](ROADMAP.md) when ready to build — do not track status only here.
