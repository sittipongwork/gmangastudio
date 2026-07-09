//
//  ISCanvas.h
//  IllusStudioFramework — C bridge (Swift-importable)
//

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus)
#define IS_EXTERN extern "C"
#else
#define IS_EXTERN extern
#endif

#if defined(ILLUSSTUDIOFRAMEWORK_EXPORTS)
#define IS_API __attribute__((visibility("default")))
#else
#define IS_API
#endif

typedef struct ISCanvas ISCanvas;

/// Create an RGBA canvas. Returns NULL on failure.
IS_API ISCanvas* ISCanvasCreate(int32_t width, int32_t height);

IS_API void ISCanvasDestroy(ISCanvas* canvas);

IS_API void ISCanvasClear(ISCanvas* canvas, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

IS_API void ISCanvasBeginStroke(ISCanvas* canvas, float x, float y, float pressure);
IS_API void ISCanvasContinueStroke(ISCanvas* canvas, float x, float y, float pressure);
IS_API void ISCanvasEndStroke(ISCanvas* canvas);

/// Pointer valid until next mutating call or destroy. RGBA8, row-major, width*height*4 bytes.
IS_API const uint8_t* ISCanvasGetPixels(const ISCanvas* canvas);
IS_API int32_t ISCanvasGetWidth(const ISCanvas* canvas);
IS_API int32_t ISCanvasGetHeight(const ISCanvas* canvas);

IS_API const char* ISCanvasGetVersion(void);

/// Returns 1 if internal self-check passes, 0 otherwise.
IS_API int32_t ISCanvasSelfCheck(void);

#ifdef __cplusplus
}
#endif
