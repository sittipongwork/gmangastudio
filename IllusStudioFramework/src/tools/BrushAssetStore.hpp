//
//  BrushAssetStore.hpp
//  IllusStudioFramework — tip / grain / preview RGBA textures (T1-7)
//

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace illus {

struct BrushTexture {
    int32_t id = 0;
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint8_t> rgba; // w*h*4
};

class BrushAssetStore {
public:
    /// Decode PNG/JPEG bytes via ImageIO → RGBA8. Returns texture id or -1.
    int32_t addImageBytes(const uint8_t* data, int32_t size, const char* label = nullptr);
    /// Raw RGBA (takes ownership of copy).
    int32_t addRGBA(int32_t width, int32_t height, const uint8_t* rgba, int32_t byteCount);

    const BrushTexture* find(int32_t textureId) const;
    void clear();

private:
    int32_t nextId_ = 1;
    std::unordered_map<int32_t, BrushTexture> textures_;
};

} // namespace illus
