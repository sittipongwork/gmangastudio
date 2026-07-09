//
//  CanvasEngine.hpp
//  IllusStudioFramework — core canvas (C++, private)
//

#pragma once

#include <cstdint>
#include <vector>

namespace illus {

struct Point {
    float x = 0;
    float y = 0;
    float pressure = 1;
};

class CanvasEngine {
public:
    CanvasEngine(int32_t width, int32_t height);

    void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void beginStroke(float x, float y, float pressure);
    void continueStroke(float x, float y, float pressure);
    void endStroke();

    const uint8_t* pixels() const { return pixels_.data(); }
    int32_t width() const { return width_; }
    int32_t height() const { return height_; }

    /// ponytail: one runnable check for stamp/blend math; expand when tools grow.
    static bool selfCheck();

private:
    void stamp(float x, float y, float pressure);
    void stampLine(const Point& a, const Point& b);

    int32_t width_ = 0;
    int32_t height_ = 0;
    std::vector<uint8_t> pixels_;
    bool stroking_ = false;
    Point last_{};
    float brushRadius_ = 8.f;
};

} // namespace illus
