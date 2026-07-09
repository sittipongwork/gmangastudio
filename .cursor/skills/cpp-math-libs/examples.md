# GLM / Eigen — extra samples

Companion to [SKILL.md](SKILL.md). Read when implementing a concrete transform or solve.

## GLM — viewport present (good)

```cpp
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct PresentUniforms {
    float mvp[16];
};

PresentUniforms makePresentUniforms(float canvasW, float canvasH,
                                    float viewW, float viewH,
                                    float scale, float ox, float oy) {
    const float fit = std::min(viewW / canvasW, viewH / canvasH);
    const float s = fit * scale;
    // NDC orthographic for Metal (y-up). Adjust if your shader expects y-down.
    glm::mat4 proj = glm::ortho(0.f, viewW, viewH, 0.f, -1.f, 1.f);
    glm::mat4 model(1.f);
    model = glm::translate(model, glm::vec3(
        (viewW - canvasW * s) * 0.5f - ox * s,
        (viewH - canvasH * s) * 0.5f - oy * s,
        0.f));
    model = glm::scale(model, glm::vec3(s, s, 1.f));
    const glm::mat4 mvp = proj * model;
    PresentUniforms u{};
    std::memcpy(u.mvp, glm::value_ptr(mvp), sizeof(u.mvp));
    return u;
}
```

## GLM — bad: type soup in layer code

```cpp
// Don't replace Layer pixel indexing with glm::ivec2 everywhere — noise, no win.
glm::ivec2 idx{x, y};
size_t i = (size_t(idx.y) * width + size_t(idx.x)) * 4; // worse than plain ints
```

## Eigen — stroke sample fit (good, offline / endStroke)

```cpp
#include <Eigen/Dense>

// Fit y ≈ a*x + b for debug / SVG simplify — call once, not per dab.
void fitStrokeTrend(const std::vector<float>& xs, const std::vector<float>& ys,
                    float& a, float& b) {
    const int n = static_cast<int>(xs.size());
    Eigen::Matrix<float, Eigen::Dynamic, 2> A(n, 2);
    Eigen::VectorXf y = Eigen::Map<const Eigen::VectorXf>(ys.data(), n);
    for (int i = 0; i < n; ++i) {
        A(i, 0) = xs[static_cast<size_t>(i)];
        A(i, 1) = 1.f;
    }
    const Eigen::Vector2f sol = A.colPivHouseholderQr().solve(y);
    a = sol(0);
    b = sol(1);
}
```

## Eigen — bad: expression in dab loop

```cpp
void stampWrong(float* /*out*/, int n, float angle) {
    for (int i = 0; i < n; ++i) {
        Eigen::Matrix2f R; // OK once outside loop — BAD inside
        R = Eigen::Rotation2Df(angle).toRotationMatrix();
        // ...
    }
}
```

## Boundary conversion (good)

```cpp
// Single place to bridge if both libs exist later.
inline glm::vec2 toGlm(const Eigen::Vector2f& v) { return {v.x(), v.y()}; }
inline Eigen::Vector2f toEigen(const glm::vec2& v) { return {v.x, v.y}; }
```

Keep conversions out of hot loops; prefer one library per subsystem (render → GLM, fit → Eigen).
