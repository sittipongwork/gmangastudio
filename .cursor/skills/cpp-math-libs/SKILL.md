---
name: cpp-math-libs
description: >-
  Choose and use optimized C++ math libraries (GLM for graphics transforms,
  Eigen for linear algebra) in IllusStudioFramework / gmangastudio. Use when
  adding GLM or Eigen, writing vec/mat/transform code, reviewing math deps,
  or when the user mentions GLM, Eigen, matrix, quaternion, or cpp-math-libs.
---

# C++ Optimized Math Libraries (GLM + Eigen)

Apply when introducing or writing math that goes beyond the small helpers in `IllusStudioFramework/src/math/` (`Rect`, `Blend`). Stay **ponytail**: do not add a dependency for a 2D scale+offset.

## Decision ladder (pick first that holds)

1. **Already in-tree?** Reuse `math/Rect.hpp`, `Viewport`, scalar float — no new lib.
2. **Axis-aligned present / zoom / pan @ 120Hz?** → **Scalar** (`presentNdcRect` or UI from cached viewport). **Not GLM.**
3. **Real mat4 / quat (canvas rotate, MVP, tip orientation matrix)?** → **GLM**.
4. **Solve / SVD / least-squares curve fit (export / offline)?** → **Eigen** — **lazy**, never under stroke mutex.
5. **Neither?** Keep a tiny POD (`struct Vec2 { float x,y; }`) in `math/` — do not pull GLM for one `dot`.

| Need | Prefer | Avoid |
|------|--------|--------|
| Present NDC (fit × scale + pan) | Scalar | GLM every frame |
| `mat4` / quat for rotate / MVP / tip matrix | GLM | Eigen; hand-rolled mat4 |
| Least-squares Bézier / PCA (export) | Eigen lazy | Fit on every `endStroke` |
| Per-pixel dab / blend | Scalars / POD | Eigen, GLM |
| Swift–C++ public API | Plain `float` / `int32_t` | GLM/Eigen in `CanvasEditor.hpp` |

**Keep vs remove:** Keep vendored GLM + Eigen while SVG cubics and/or canvas rotate remain on the roadmap. Removing them does not fix idle 120Hz present CPU. See [ROADMAP TX-7](../../IllusStudioFramework/docs/ROADMAP.md#tx-7--math-libraries-glm--eigen).

**Vendoring:** `IllusStudioFramework/third_party/glm/` or `…/eigen/`; use **SYSTEM_HEADER_SEARCH_PATHS** (`-isystem`) so vendor doc-comments stay quiet.

## GLM — good vs bad

**Good:** column-major transforms once per frame / stroke; pass `value_ptr` to Metal; keep hot loops scalar.

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Build view matrix once (present / gizmo), not per dab.
glm::mat4 canvasToNdc(float scale, float ox, float oy, float viewW, float viewH) {
    glm::mat4 m(1.f);
    m = glm::translate(m, glm::vec3(ox, oy, 0.f));
    m = glm::scale(m, glm::vec3(scale, scale, 1.f));
    return m;
}

void uploadMVP(MTL::Buffer* buf, const glm::mat4& mvp) {
    // Metal wants column-major float[16] — GLM matches.
    std::memcpy(buf->contents(), glm::value_ptr(mvp), sizeof(float) * 16);
}
```

```cpp
// Tip rotation: one angle → 2×2, or use glm::vec2 rotate once per dab (OK).
glm::vec2 rotate(glm::vec2 p, float rad) {
    const float c = std::cos(rad), s = std::sin(rad);
    return {p.x * c - p.y * s, p.x * s + p.y * c};
}
```

**Bad:** GLM in pixel loops; unnecessary `dmat`; including all of GLM; leaking types across the public API.

```cpp
// BAD — mat4 multiply inside every pixel of a dab stamp
for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
        glm::mat4 M = glm::rotate(glm::mat4(1), angle, {0,0,1}); // rebuild every pixel
        glm::vec4 p = M * glm::vec4(x, y, 0, 1);
        // ...
    }
}

// BAD — pull heavy headers into CanvasEditor.hpp (Swift interop + compile cost)
// #include <glm/glm.hpp>
// glm::vec2 viewToCanvas(...);

// BAD — double precision on a 2D canvas path with no need
glm::dvec2 p{x, y};
```

**GLM habits for this repo**

- Prefer `#include <glm/vec2.hpp>` / specific headers over `glm/ext.hpp` when possible.
- Define `GLM_FORCE_SILENT_WARNINGS` only if needed; don't enable experimental extensions casually.
- Match Metal: column-major, `float` (not `double`) for present uniforms.
- Convert at the boundary: `Viewport` stays `{scale, offsetX, offsetY}`; build `mat4` only in render/present code.

## Eigen — good vs bad

**Good:** one-shot solves / fits; fixed-size types on stack; `NoAlias` / `.eval()` when reusing LHS.

```cpp
#include <Eigen/Dense>

// Bézier / polyline fit: small dense solve once on endStroke — OK.
bool fitLineLeastSquares(const float* xy, int n, float& a, float& b) {
    if (n < 2) return false;
    Eigen::MatrixXf A(n, 2);
    Eigen::VectorXf y(n);
    for (int i = 0; i < n; ++i) {
        A(i, 0) = xy[i * 2];
        A(i, 1) = 1.f;
        y(i) = xy[i * 2 + 1];
    }
    Eigen::Vector2f sol = A.colPivHouseholderQr().solve(y);
    a = sol(0);
    b = sol(1);
    return true;
}
```

```cpp
// Fixed-size: no heap, good for small geometry helpers.
Eigen::Matrix2f R;
R << std::cos(t), -std::sin(t),
     std::sin(t),  std::cos(t);
Eigen::Vector2f p(x, y);
p = R * p;
```

**Bad:** Eigen on the 120Hz dab/present path; auto expression chains that allocate; dynamic matrices for 2–3 floats.

```cpp
// BAD — dynamic MatrixXf for a 2-vector (heap + slow)
Eigen::MatrixXf v(2, 1);
v(0) = x; v(1) = y;

// BAD — per-dab QR in continueStroke
void continueStroke(...) {
    Eigen::MatrixXf A(samples.size(), 4);
    // rebuild and solve every pointer move — kills 120Hz
}

// BAD — aliasing bug (read/write same matrix without eval)
M = M * A;           // may be wrong
M.noalias() = M * A; // still careful — prefer
M = (M * A).eval();  // or use a temporary

// BAD — expose Eigen::VectorXf in public CanvasEditor API
```

**Eigen habits for this repo**

- Use on **T2-7** curve fit / offline tools / tests — not in `stampDab` / `presentMetalTexture`.
- Prefer `Matrix2f` / `Vector2f` / `Matrix4f` over `MatrixXf` when size is known.
- `#include <Eigen/Dense>` only in `.cpp` that need it; keep headers free of Eigen when possible (compile times).
- Don't mix GLM `mat4` and Eigen `Matrix4f` without an explicit conversion helper in one place.

## Benchmark — measure before putting libs on hot paths

Runnable microbench (not in the Xcode target):

```bash
clang++ -O2 -std=c++20 -DNDEBUG \
  -I IllusStudioFramework/src \
  -isystem IllusStudioFramework/third_party/eigen \
  -isystem IllusStudioFramework/third_party/glm \
  IllusStudioFramework/src/math/Bezier.cpp \
  IllusStudioFramework/src/math/PresentTransform.cpp \
  IllusStudioFramework/tools/tx7_math_bench.cpp \
  -o /tmp/tx7_math_bench && /tmp/tx7_math_bench
```

Compare **`-O2 -DNDEBUG`** (ship) vs **`-O0`** (typical Xcode Debug). Numbers below are from one Apple Silicon run; re-run locally.

| Path | Release `-O2` | Debug `-O0` |
|------|---------------|-------------|
| Present NDC **scalar** | ~7 ns/op | ~34 ns/op |
| Present NDC **GLM mat4** | ~6 ns/op (~same) | ~770 ns/op (**~23×** vs scalar) |
| endStroke **skip fit** | ~5 ns | ~14 ns |
| endStroke **Eigen fit** (50…1000 samples) | ~24…59 µs | ~0.7…2.6 **ms** |

**Takeaway:** Keep libs; use **scalar present** + **lazy Eigen**. In Debug, GLM-on-present and Eigen-on-pen-up caused Activity Monitor spikes. Do not remove third_party “for CPU” while T4 SVG / rotate still planned.

## Project integration checklist

When adding either library:

- [ ] Justify with the ladder above (comment `ponytail:` if “only for X”)
- [ ] Vendor under `third_party/`; add HEADER_SEARCH_PATHS like metal-cpp
- [ ] No GLM/Eigen in `CanvasEditor.hpp` / Swift-facing surface
- [ ] Hot path (stroke, composite, Metal upload) stays allocation-free
- [ ] One self-check or assert if non-trivial math lands (ponytail)

## Related

- Memory / frame budget: [cpp_optimise](../cpp_optimise/SKILL.md)
- YAGNI / smallest diff: [ponytail](../ponytail/SKILL.md)
- More samples: [examples.md](examples.md)
