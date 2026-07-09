//
//  ISCanvas.cpp
//  IllusStudioFramework — C bridge over CanvasEngine
//

#define ILLUSSTUDIOFRAMEWORK_EXPORTS 1

#include "../ISCanvas.h"
#include "CanvasEngine.hpp"

#include <new>

struct ISCanvas {
    illus::CanvasEngine engine;
    explicit ISCanvas(int32_t w, int32_t h) : engine(w, h) {}
};

ISCanvas* ISCanvasCreate(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) return nullptr;
    return new (std::nothrow) ISCanvas(width, height);
}

void ISCanvasDestroy(ISCanvas* canvas) {
    delete canvas;
}

void ISCanvasClear(ISCanvas* canvas, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!canvas) return;
    canvas->engine.clear(r, g, b, a);
}

void ISCanvasBeginStroke(ISCanvas* canvas, float x, float y, float pressure) {
    if (!canvas) return;
    canvas->engine.beginStroke(x, y, pressure);
}

void ISCanvasContinueStroke(ISCanvas* canvas, float x, float y, float pressure) {
    if (!canvas) return;
    canvas->engine.continueStroke(x, y, pressure);
}

void ISCanvasEndStroke(ISCanvas* canvas) {
    if (!canvas) return;
    canvas->engine.endStroke();
}

const uint8_t* ISCanvasGetPixels(const ISCanvas* canvas) {
    if (!canvas) return nullptr;
    return canvas->engine.pixels();
}

int32_t ISCanvasGetWidth(const ISCanvas* canvas) {
    return canvas ? canvas->engine.width() : 0;
}

int32_t ISCanvasGetHeight(const ISCanvas* canvas) {
    return canvas ? canvas->engine.height() : 0;
}

const char* ISCanvasGetVersion(void) {
    return "IllusStudioFramework 0.1.0";
}

int32_t ISCanvasSelfCheck(void) {
    return illus::CanvasEngine::selfCheck() ? 1 : 0;
}
