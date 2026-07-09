//
//  ProcreateBrushImporter.hpp
//  IllusStudioFramework — .brush / .brushset / .brushlibrary import (T1-7)
//

#pragma once

#include "../../CanvasEditor.hpp" // BrushPackageKind
#include "../BrushAssetStore.hpp"
#include "../BrushLibrary.hpp"

#include <cstdint>

namespace illus {

struct BrushImportResult {
    int32_t setId = -1;       // primary / last set id
    int32_t brushCount = 0;
    int32_t setCount = 0;
    bool ok = false;
};

BrushImportResult importBrushPackageBytes(
    BrushLibrary& library,
    BrushAssetStore& assets,
    const uint8_t* data,
    int32_t size,
    BrushPackageKind kind,
    const char* suggestedName
);

BrushImportResult importBrushPackagePath(
    BrushLibrary& library,
    BrushAssetStore& assets,
    const char* path,
    BrushPackageKind kind
);

} // namespace illus
