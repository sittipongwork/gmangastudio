//
//  Blend.hpp
//  IllusStudioFramework — integer straight-alpha blend helpers
//

#pragma once

#include "Rect.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace illus::math {

struct RGBA {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

inline void fillRGBA(uint8_t* dst, size_t pixelCount, RGBA c) {
    if (pixelCount == 0) return;
    // Fast path: all channels equal (e.g. white/black clear).
    if (c.r == c.g && c.g == c.b && c.b == c.a) {
        std::memset(dst, c.r, pixelCount * 4u);
        return;
    }
    for (size_t i = 0; i < pixelCount; ++i) {
        dst[i * 4 + 0] = c.r;
        dst[i * 4 + 1] = c.g;
        dst[i * 4 + 2] = c.b;
        dst[i * 4 + 3] = c.a;
    }
}

inline void fillRect(uint8_t* dst, int32_t stridePixels, Rect rect, RGBA c) {
    if (rect.empty()) return;
    for (int32_t y = 0; y < rect.h; ++y) {
        uint8_t* row = dst + (static_cast<size_t>(rect.y + y) * static_cast<size_t>(stridePixels) + static_cast<size_t>(rect.x)) * 4u;
        for (int32_t x = 0; x < rect.w; ++x) {
            row[0] = c.r;
            row[1] = c.g;
            row[2] = c.b;
            row[3] = c.a;
            row += 4;
        }
    }
}

inline void copyRect(uint8_t* dst, const uint8_t* src, int32_t stridePixels, Rect rect) {
    if (rect.empty()) return;
    const size_t rowBytes = static_cast<size_t>(rect.w) * 4u;
    for (int32_t y = 0; y < rect.h; ++y) {
        const size_t off = (static_cast<size_t>(rect.y + y) * static_cast<size_t>(stridePixels) + static_cast<size_t>(rect.x)) * 4u;
        std::memcpy(dst + off, src + off, rowBytes);
    }
}

/// Straight-alpha src-over (integer).
/// out_a = sa + da*(1-sa); out_rgb = (src*sa + dst*da*(1-sa)) / out_a
/// (Do not write premultiplied RGB — LayerCompositor premultiplies at present.)
inline void blendSrcOver(uint8_t* dst, uint8_t sr, uint8_t sg, uint8_t sb, uint8_t sa) {
    if (sa == 0) return;
    if (sa == 255) {
        dst[0] = sr;
        dst[1] = sg;
        dst[2] = sb;
        dst[3] = 255;
        return;
    }
    const uint32_t da = dst[3];
    if (da == 0) {
        dst[0] = sr;
        dst[1] = sg;
        dst[2] = sb;
        dst[3] = sa;
        return;
    }
    // Heal bogus "black with alpha" leftovers — but not real black paint (sr=sg=sb=0).
    if ((dst[0] | dst[1] | dst[2]) == 0 && (sr | sg | sb) != 0) {
        dst[0] = sr;
        dst[1] = sg;
        dst[2] = sb;
        dst[3] = sa;
        return;
    }
    const uint32_t inv = 255u - sa;
    const uint32_t outA = sa + (da * inv + 127u) / 255u;
    if (outA == 0) {
        dst[0] = dst[1] = dst[2] = dst[3] = 0;
        return;
    }
    dst[0] = static_cast<uint8_t>((sr * sa + (dst[0] * da * inv + 127u) / 255u + outA / 2u) / outA);
    dst[1] = static_cast<uint8_t>((sg * sa + (dst[1] * da * inv + 127u) / 255u + outA / 2u) / outA);
    dst[2] = static_cast<uint8_t>((sb * sa + (dst[2] * da * inv + 127u) / 255u + outA / 2u) / outA);
    dst[3] = static_cast<uint8_t>(outA);
}

/// Brush stamp / paint merge: keep brush RGB, accumulate coverage.
/// Same-color overlaps stay vivid (no muddy dark cores from src-over drift).
inline void blendPaintOver(uint8_t* dst, uint8_t sr, uint8_t sg, uint8_t sb, uint8_t sa) {
    if (sa == 0) return;
    const uint32_t da = dst[3];
    if (da == 0) {
        dst[0] = sr;
        dst[1] = sg;
        dst[2] = sb;
        dst[3] = sa;
        return;
    }
    // Heal bogus black+alpha; real black brush (src 0,0,0) falls through to accumulate.
    if ((dst[0] | dst[1] | dst[2]) == 0 && (sr | sg | sb) != 0) {
        dst[0] = sr;
        dst[1] = sg;
        dst[2] = sb;
        dst[3] = sa;
        return;
    }
    // Near-same color (incl. 1–2 LSB drift from prior merges) → coverage only.
    const int dr = int(dst[0]) - int(sr);
    const int dg = int(dst[1]) - int(sg);
    const int db = int(dst[2]) - int(sb);
    if (dr * dr + dg * dg + db * db <= 30 * 30) {
        const uint32_t inv = 255u - sa;
        const uint32_t outA = sa + (da * inv + 127u) / 255u;
        dst[0] = sr;
        dst[1] = sg;
        dst[2] = sb;
        dst[3] = static_cast<uint8_t>(std::min<uint32_t>(255u, outA));
        return;
    }
    blendSrcOver(dst, sr, sg, sb, sa);
}

/// Dest-out: punch coverage `sa` from dst alpha (keep straight RGB).
inline void blendDestOut(uint8_t* dst, uint8_t sa) {
    if (sa == 0) return;
    if (sa == 255) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = 0;
        return;
    }
    const uint32_t inv = 255u - sa;
    dst[3] = static_cast<uint8_t>((dst[3] * inv + 127u) / 255u);
    if (dst[3] == 0) {
        dst[0] = dst[1] = dst[2] = 0;
    }
}

inline void blendSrcOverOpacity(uint8_t* dst, const uint8_t* src, uint8_t opacity8) {
    if (src[3] == 0 || opacity8 == 0) return;
    const uint8_t sa = static_cast<uint8_t>((src[3] * opacity8 + 127u) / 255u);
    blendSrcOver(dst, src[0], src[1], src[2], sa);
}

inline void blendPaintOverOpacity(uint8_t* dst, const uint8_t* src, uint8_t opacity8) {
    if (src[3] == 0 || opacity8 == 0) return;
    const uint8_t sa = static_cast<uint8_t>((src[3] * opacity8 + 127u) / 255u);
    blendPaintOver(dst, src[0], src[1], src[2], sa);
}

/// Blend src layer rect over dst (same dimensions / stride).
inline void blendLayerRect(uint8_t* dst, const uint8_t* src, int32_t stridePixels, Rect rect, float opacity) {
    if (rect.empty() || opacity <= 0.f) return;
    const uint8_t opacity8 = static_cast<uint8_t>(std::clamp(opacity, 0.f, 1.f) * 255.f + 0.5f);
    if (opacity8 == 0) return;

    for (int32_t y = 0; y < rect.h; ++y) {
        const size_t row = (static_cast<size_t>(rect.y + y) * static_cast<size_t>(stridePixels) + static_cast<size_t>(rect.x)) * 4u;
        uint8_t* d = dst + row;
        const uint8_t* s = src + row;
        for (int32_t x = 0; x < rect.w; ++x) {
            blendSrcOverOpacity(d, s, opacity8);
            d += 4;
            s += 4;
        }
    }
}

/// Paint merge (overlay → layer): coverage accumulate, preserve brush color.
inline void blendPaintLayerRect(uint8_t* dst, const uint8_t* src, int32_t stridePixels, Rect rect, float opacity) {
    if (rect.empty() || opacity <= 0.f) return;
    const uint8_t opacity8 = static_cast<uint8_t>(std::clamp(opacity, 0.f, 1.f) * 255.f + 0.5f);
    if (opacity8 == 0) return;

    for (int32_t y = 0; y < rect.h; ++y) {
        const size_t row = (static_cast<size_t>(rect.y + y) * static_cast<size_t>(stridePixels) + static_cast<size_t>(rect.x)) * 4u;
        uint8_t* d = dst + row;
        const uint8_t* s = src + row;
        for (int32_t x = 0; x < rect.w; ++x) {
            blendPaintOverOpacity(d, s, opacity8);
            d += 4;
            s += 4;
        }
    }
}

} // namespace illus::math
