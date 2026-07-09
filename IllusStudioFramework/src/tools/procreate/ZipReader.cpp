//
//  ZipReader.cpp
//  IllusStudioFramework — store + raw-deflate ZIP extract
//

#include "ZipReader.hpp"

#include <zlib.h>

#include <algorithm>
#include <cstring>

namespace illus {
namespace {

uint16_t u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

bool inflateRaw(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& dst, size_t expected) {
    if (!src || srcLen == 0) return false;
    dst.assign(expected > 0 ? expected : srcLen * 4u, 0);

    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    strm.avail_in = static_cast<uInt>(srcLen);
    // Negative windowBits = raw DEFLATE (ZIP method 8).
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;

    strm.next_out = dst.data();
    strm.avail_out = static_cast<uInt>(dst.size());
    int rc = inflate(&strm, Z_FINISH);
    if (rc == Z_BUF_ERROR && expected == 0) {
        const size_t produced = strm.total_out;
        dst.resize(std::max(dst.size() * 4u, produced + srcLen * 8u));
        strm.next_out = dst.data() + produced;
        strm.avail_out = static_cast<uInt>(dst.size() - produced);
        rc = inflate(&strm, Z_FINISH);
    }
    const size_t written = strm.total_out;
    inflateEnd(&strm);
    if (rc != Z_STREAM_END && rc != Z_OK) return false;
    dst.resize(written);
    return written > 0;
}

} // namespace

bool zipExtractAll(const uint8_t* data, int32_t size, std::vector<ZipEntry>& out) {
    out.clear();
    if (!data || size < 30) return false;

    int32_t off = 0;
    while (off + 30 <= size) {
        if (u32(data + off) != 0x04034b50u) break;
        const uint16_t method = u16(data + off + 8);
        const uint32_t compSize = u32(data + off + 18);
        const uint32_t uncompSize = u32(data + off + 22);
        const uint16_t nameLen = u16(data + off + 26);
        const uint16_t extraLen = u16(data + off + 28);
        const int32_t nameStart = off + 30;
        if (nameStart + nameLen + extraLen > size) return false;

        std::string path(reinterpret_cast<const char*>(data + nameStart), nameLen);
        const int32_t dataStart = nameStart + nameLen + extraLen;
        if (compSize > 0 && dataStart + static_cast<int32_t>(compSize) > size) return false;

        if (!path.empty() && path.back() == '/') {
            off = dataStart + static_cast<int32_t>(compSize);
            continue;
        }

        ZipEntry entry;
        entry.path = std::move(path);
        if (method == 0) {
            entry.data.assign(data + dataStart, data + dataStart + compSize);
        } else if (method == 8) {
            if (!inflateRaw(data + dataStart, compSize, entry.data, uncompSize)) {
                off = dataStart + static_cast<int32_t>(compSize);
                continue;
            }
        } else {
            off = dataStart + static_cast<int32_t>(compSize);
            continue;
        }
        out.push_back(std::move(entry));
        off = dataStart + static_cast<int32_t>(compSize);
    }
    return !out.empty();
}

} // namespace illus
