//
//  ProcreateBrushMap.mm
//  IllusStudioFramework — NSPropertyList / NSKeyedUnarchiver best-effort flatten
//

#import "ProcreateBrushMap.hpp"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace illus {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void ingestNumber(ProcreateArchiveFlat& flat, const std::string& key, float v) {
    if (key.empty()) return;
    flat.numbers[lower(key)] = v;
}

void ingestString(ProcreateArchiveFlat& flat, const std::string& key, const std::string& v) {
    if (key.empty()) return;
    flat.strings[lower(key)] = v;
}

void walk(id obj, const std::string& prefix, ProcreateArchiveFlat& flat, int depth);

void walkDict(NSDictionary* dict, const std::string& prefix, ProcreateArchiveFlat& flat, int depth) {
    if (depth > 8 || !dict) return;
    for (id key in dict) {
        if (![key isKindOfClass:[NSString class]]) continue;
        const std::string k = prefix.empty()
            ? std::string([key UTF8String] ?: "")
            : prefix + "." + std::string([key UTF8String] ?: "");
        walk(dict[key], k, flat, depth + 1);
    }
}

void walk(id obj, const std::string& prefix, ProcreateArchiveFlat& flat, int depth) {
    if (!obj || depth > 8) return;
    if ([obj isKindOfClass:[NSDictionary class]]) {
        walkDict((NSDictionary*)obj, prefix, flat, depth);
        return;
    }
    if ([obj isKindOfClass:[NSArray class]]) {
        NSArray* arr = (NSArray*)obj;
        // Sample first few numeric points as pressure endpoints.
        NSUInteger n = MIN(arr.count, (NSUInteger)4);
        for (NSUInteger i = 0; i < n; ++i) {
            walk(arr[i], prefix + "[" + std::to_string(i) + "]", flat, depth + 1);
        }
        return;
    }
    if ([obj isKindOfClass:[NSNumber class]]) {
        ingestNumber(flat, prefix, [(NSNumber*)obj floatValue]);
        // Also store leaf name (last path component) for synonym lookup.
        const auto dot = prefix.rfind('.');
        if (dot != std::string::npos) {
            ingestNumber(flat, prefix.substr(dot + 1), [(NSNumber*)obj floatValue]);
        }
        return;
    }
    if ([obj isKindOfClass:[NSString class]]) {
        ingestString(flat, prefix, std::string([(NSString*)obj UTF8String] ?: ""));
        return;
    }
}

float findNum(const ProcreateArchiveFlat& flat, std::initializer_list<const char*> keys, bool& hit) {
    for (const char* k : keys) {
        const auto it = flat.numbers.find(lower(k));
        if (it != flat.numbers.end()) {
            hit = true;
            return it->second;
        }
    }
    return 0.f;
}

float norm01(float v) {
    if (v > 1.f && v <= 100.f) return std::clamp(v / 100.f, 0.f, 1.f);
    return std::clamp(v, 0.f, 1.f);
}

} // namespace

ProcreateArchiveFlat decodeProcreateArchive(const uint8_t* data, int32_t size) {
    ProcreateArchiveFlat flat;
    if (!data || size < 8) return flat;

    @autoreleasepool {
        NSData* nsData = [NSData dataWithBytes:data length:static_cast<NSUInteger>(size)];
        id root = nil;
        NSError* err = nil;

        // Prefer keyed unarchive (Procreate Brush.archive).
        if (@available(macOS 10.13, iOS 11, *)) {
            NSKeyedUnarchiver* un = [[NSKeyedUnarchiver alloc] initForReadingFromData:nsData error:&err];
            if (un) {
                un.requiresSecureCoding = NO;
                root = [un decodeObjectForKey:NSKeyedArchiveRootObjectKey];
                if (!root) {
                    // Some archives use "$top" / custom keys — try top-level decodeObject.
                    root = [un decodeTopLevelObjectAndReturnError:&err];
                }
                [un finishDecoding];
            }
        }
        if (!root) {
            root = [NSPropertyListSerialization propertyListWithData:nsData
                                                              options:NSPropertyListImmutable
                                                               format:nil
                                                                error:&err];
        }
        if (root) walk(root, "", flat, 0);
    }
    return flat;
}

void mapProcreateToPreset(const ProcreateArchiveFlat& flat, BrushPreset& preset, bool& approximated) {
    int hits = 0;
    bool hit = false;

    hit = false;
    float size = findNum(flat, {"maxsize", "size", "diameter", "paintSize", "paintedSize", "tipSize"}, hit);
    if (hit) {
        // Procreate sizes are often large; clamp to a usable canvas range.
        if (size > 200.f) size = 40.f + std::fmod(size, 160.f);
        preset.lineWidthPx = std::clamp(size, 1.f, 256.f);
        ++hits;
    }

    hit = false;
    float smooth = findNum(flat, {"streamline", "stabilization", "smoothing", "lineSmooth"}, hit);
    if (hit) {
        preset.lineSmooth = norm01(smooth);
        ++hits;
    }

    hit = false;
    float hard = findNum(flat, {"hardness", "hard"}, hit);
    if (hit) {
        preset.hardness = norm01(hard);
        ++hits;
    } else {
        hit = false;
        float soft = findNum(flat, {"softness", "soft"}, hit);
        if (hit) {
            preset.hardness = 1.f - norm01(soft);
            ++hits;
        }
    }

    hit = false;
    float opac = findNum(flat, {"opacity", "maxopacity"}, hit);
    if (hit) {
        preset.opacity = norm01(opac);
        ++hits;
    }

    hit = false;
    float flow = findNum(flat, {"flow", "accumulation", "density"}, hit);
    if (hit) {
        preset.flow = norm01(flow);
        ++hits;
    }

    hit = false;
    float spacing = findNum(flat, {"spacing", "stampSpacing"}, hit);
    if (hit) {
        // Often percent of size (1..100) or 0..1.
        if (spacing > 1.f) spacing = spacing / 100.f;
        // Cap: large Procreate spacing → scalloped dab islands in our stamp path.
        preset.spacing = std::clamp(spacing, 0.04f, 0.12f);
        ++hits;
    }

    hit = false;
    float sizeP = findNum(flat, {"sizepressure", "pressureSize", "sizeDynamics"}, hit);
    if (hit) {
        preset.sizePressure = norm01(sizeP);
        ++hits;
    }

    hit = false;
    float opacP = findNum(flat, {"opacitypressure", "pressureOpacity", "opacityDynamics"}, hit);
    if (hit) {
        preset.opacityPressure = norm01(opacP);
        ++hits;
    }

    hit = false;
    float angle = findNum(flat, {"angle", "rotation", "azimuth"}, hit);
    if (hit) {
        preset.angleDeg = angle;
        ++hits;
    }

    hit = false;
    float roundness = findNum(flat, {"roundness", "aspect", "ellipticity"}, hit);
    if (hit) {
        preset.roundness = norm01(roundness);
        ++hits;
    }

    approximated = hits < 2;
}

} // namespace illus
