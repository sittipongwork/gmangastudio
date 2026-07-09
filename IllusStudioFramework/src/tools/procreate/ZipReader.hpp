//
//  ZipReader.hpp
//  IllusStudioFramework — minimal ZIP (store + deflate) for Procreate packs (T1-7)
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace illus {

struct ZipEntry {
    std::string path; // forward-slash, may include folders
    std::vector<uint8_t> data;
};

/// Extract all files from a ZIP buffer. Returns false on hard failure (bad header).
/// ponytail: no encryption / zip64 / multi-disk — enough for .brush / .brushset.
bool zipExtractAll(const uint8_t* data, int32_t size, std::vector<ZipEntry>& out);

} // namespace illus
