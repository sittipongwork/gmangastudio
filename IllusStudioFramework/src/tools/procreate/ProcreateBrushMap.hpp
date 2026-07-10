//
//  ProcreateBrushMap.hpp
//  IllusStudioFramework — best-effort Brush.archive → BrushPreset (T1-7-2)
//

#pragma once

#include "../BrushPreset.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace illus {

/// Flat string→string / string→number map from a best-effort archive decode.
struct ProcreateArchiveFlat {
    std::unordered_map<std::string, float> numbers;
    std::unordered_map<std::string, std::string> strings;
    bool approximated = true; // true until we map at least one known key
};

/// Decode Brush.archive bytes (bplist / NSKeyedArchiver) into a flat map.
/// Never throws; empty map on failure.
ProcreateArchiveFlat decodeProcreateArchive(const uint8_t* data, int32_t size);

/// Apply known synonyms onto `preset`. Sets `approximated` if few keys matched.
void mapProcreateToPreset(const ProcreateArchiveFlat& flat, BrushPreset& preset, bool& approximated);

/// Parse brushset.plist bytes → set display name + ordered brush UUID list.
void parseBrushsetPlistBytes(
    const uint8_t* data,
    int32_t size,
    std::string& outName,
    std::vector<std::string>& outBrushOrder
);

} // namespace illus
