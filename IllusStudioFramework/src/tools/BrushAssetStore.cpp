//
//  BrushAssetStore.cpp
//  IllusStudioFramework
//

#include "BrushAssetStore.hpp"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <cstring>

namespace illus {

int32_t BrushAssetStore::addRGBA(int32_t width, int32_t height, const uint8_t* rgba, int32_t byteCount) {
    if (!rgba || width < 1 || height < 1) return -1;
    const int32_t need = width * height * 4;
    if (byteCount < need) return -1;
    BrushTexture t;
    t.id = nextId_++;
    t.width = width;
    t.height = height;
    t.rgba.assign(rgba, rgba + need);
    const int32_t id = t.id;
    textures_[id] = std::move(t);
    return id;
}

int32_t BrushAssetStore::addImageBytes(const uint8_t* data, int32_t size, const char* /*label*/) {
    if (!data || size < 8) return -1;

    CFDataRef cfData = CFDataCreate(kCFAllocatorDefault, data, static_cast<CFIndex>(size));
    if (!cfData) return -1;
    CGImageSourceRef src = CGImageSourceCreateWithData(cfData, nullptr);
    CFRelease(cfData);
    if (!src) return -1;

    CGImageRef image = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!image) return -1;

    const size_t w = CGImageGetWidth(image);
    const size_t h = CGImageGetHeight(image);
    if (w < 1 || h < 1 || w > 4096 || h > 4096) {
        CGImageRelease(image);
        return -1;
    }

    std::vector<uint8_t> rgba(w * h * 4u, 0);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        rgba.data(),
        w,
        h,
        8,
        w * 4u,
        space,
        static_cast<CGBitmapInfo>(
            static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) |
            static_cast<uint32_t>(kCGBitmapByteOrder32Big)
        )
    );
    CGColorSpaceRelease(space);
    if (!ctx) {
        CGImageRelease(image);
        return -1;
    }
    CGContextDrawImage(ctx, CGRectMake(0, 0, static_cast<CGFloat>(w), static_cast<CGFloat>(h)), image);
    CGContextRelease(ctx);
    CGImageRelease(image);

    // Un-premultiply for stamp math (straight alpha).
    for (size_t i = 0; i < w * h; ++i) {
        uint8_t* p = rgba.data() + i * 4u;
        const uint8_t a = p[3];
        if (a == 0) {
            p[0] = p[1] = p[2] = 0;
        } else if (a < 255) {
            p[0] = static_cast<uint8_t>(std::min(255, (p[0] * 255 + a / 2) / a));
            p[1] = static_cast<uint8_t>(std::min(255, (p[1] * 255 + a / 2) / a));
            p[2] = static_cast<uint8_t>(std::min(255, (p[2] * 255 + a / 2) / a));
        }
    }

    // Procreate Shape/Grain: grayscale RGB + opaque A after CG. Stamp coverage uses
    // max(RGB,A) — without this, dark grain/tip holes read as full coverage (solid strokes).
    // Skip flat RGB (fixture tips) so near-black + A=255 still stamps via alpha.
    {
        bool alphaFlatOpaque = true;
        double sum = 0, sum2 = 0;
        const size_t n = w * h;
        for (size_t i = 0; i < n; ++i) {
            const uint8_t* p = rgba.data() + i * 4u;
            if (p[3] < 255) {
                alphaFlatOpaque = false;
                break;
            }
            const double lum = std::max({p[0], p[1], p[2]});
            sum += lum;
            sum2 += lum * lum;
        }
        if (alphaFlatOpaque && n > 0) {
            const double mean = sum / static_cast<double>(n);
            const double var = sum2 / static_cast<double>(n) - mean * mean;
            if (var > 64.0) { // std > 8
                for (size_t i = 0; i < n; ++i) {
                    uint8_t* p = rgba.data() + i * 4u;
                    p[3] = std::max({p[0], p[1], p[2]});
                }
            }
        }
    }

    return addRGBA(static_cast<int32_t>(w), static_cast<int32_t>(h), rgba.data(), static_cast<int32_t>(rgba.size()));
}

const BrushTexture* BrushAssetStore::find(int32_t textureId) const {
    const auto it = textures_.find(textureId);
    return it == textures_.end() ? nullptr : &it->second;
}

void BrushAssetStore::clear() {
    textures_.clear();
}

} // namespace illus
