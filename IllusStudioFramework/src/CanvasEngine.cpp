//
//  CanvasEngine.cpp
//  IllusStudioFramework
//

#include "CanvasEngine.hpp"

#include <algorithm>
#include <cmath>

namespace illus {
namespace {

inline void blendOver(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 0) return;
    if (a == 255) {
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 255;
        return;
    }
    const float af = a / 255.f;
    const float inv = 1.f - af;
    dst[0] = static_cast<uint8_t>(r * af + dst[0] * inv);
    dst[1] = static_cast<uint8_t>(g * af + dst[1] * inv);
    dst[2] = static_cast<uint8_t>(b * af + dst[2] * inv);
    dst[3] = static_cast<uint8_t>(std::min(255.f, dst[3] + a * inv));
}

} // namespace

CanvasEngine::CanvasEngine(int32_t width, int32_t height)
    : width_(std::max(1, width))
    , height_(std::max(1, height))
    , pixels_(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u, 255)
{
    clear(255, 255, 255, 255);
}

void CanvasEngine::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (size_t i = 0; i < pixels_.size(); i += 4) {
        pixels_[i] = r;
        pixels_[i + 1] = g;
        pixels_[i + 2] = b;
        pixels_[i + 3] = a;
    }
}

void CanvasEngine::beginStroke(float x, float y, float pressure) {
    stroking_ = true;
    last_ = {x, y, pressure};
    stamp(x, y, pressure);
}

void CanvasEngine::continueStroke(float x, float y, float pressure) {
    if (!stroking_) {
        beginStroke(x, y, pressure);
        return;
    }
    Point next{x, y, pressure};
    stampLine(last_, next);
    last_ = next;
}

void CanvasEngine::endStroke() {
    stroking_ = false;
}

void CanvasEngine::stamp(float x, float y, float pressure) {
    const float radius = brushRadius_ * std::clamp(pressure, 0.05f, 1.f);
    const float radius2 = radius * radius;
    const int minX = std::max(0, static_cast<int>(std::floor(x - radius)));
    const int maxX = std::min(width_ - 1, static_cast<int>(std::ceil(x + radius)));
    const int minY = std::max(0, static_cast<int>(std::floor(y - radius)));
    const int maxY = std::min(height_ - 1, static_cast<int>(std::ceil(y + radius)));

    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float dx = (px + 0.5f) - x;
            const float dy = (py + 0.5f) - y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > radius2) continue;
            const float t = 1.f - std::sqrt(d2) / radius;
            const uint8_t a = static_cast<uint8_t>(std::clamp(t * 220.f, 0.f, 255.f));
            uint8_t* dst = &pixels_[(static_cast<size_t>(py) * static_cast<size_t>(width_) + static_cast<size_t>(px)) * 4u];
            blendOver(dst, 20, 20, 20, a);
        }
    }
}

void CanvasEngine::stampLine(const Point& a, const Point& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float step = std::max(0.5f, brushRadius_ * 0.35f);
    const int count = std::max(1, static_cast<int>(std::ceil(dist / step)));
    for (int i = 1; i <= count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count);
        stamp(a.x + dx * t, a.y + dy * t, a.pressure + (b.pressure - a.pressure) * t);
    }
}

bool CanvasEngine::selfCheck() {
    CanvasEngine c(32, 32);
    c.clear(255, 255, 255, 255);
    c.beginStroke(16, 16, 1);
    c.endStroke();
    const uint8_t* p = c.pixels();
    // Pixel (16,16): index = (y * width + x) * 4 — must not stay pure white after stamp.
    const size_t i = (16u * 32u + 16u) * 4u;
    return !(p[i] == 255 && p[i + 1] == 255 && p[i + 2] == 255);
}

} // namespace illus
