//
//  ProcreateBrushMap.mm
//  IllusStudioFramework — NSPropertyList / NSKeyedUnarchiver best-effort flatten
//

#import "ProcreateBrushMap.hpp"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <cctype>
#include <cmath>

/// Procreate archives encode `SilicaBrush` / `ValkyrieBrush` — stub so NSKeyedUnarchiver can decode.
@interface IllusCurveStub : NSObject <NSCoding>
@property (nonatomic, assign) float y0;
@property (nonatomic, assign) float y1;
@property (nonatomic, assign) BOOL valid;
@end
@implementation IllusCurveStub
- (instancetype)initWithCoder:(NSCoder*)coder {
    self = [super init];
    if (!self) return nil;
    _y0 = 0.f;
    _y1 = 1.f;
    _valid = NO;
    id pts = nil;
    @try { pts = [coder decodeObjectForKey:@"points"]; } @catch (NSException*) {}
    if ([pts isKindOfClass:[NSArray class]] && [(NSArray*)pts count] >= 2) {
        NSArray* arr = (NSArray*)pts;
        auto yAt = ^float(id obj) {
            NSString* s = [obj isKindOfClass:[NSString class]]
                ? (NSString*)obj
                : [obj description];
            if (!s) return 0.f;
            // "{x, y}" from NSPoint / NSValue description
            NSScanner* sc = [NSScanner scannerWithString:s];
            float x = 0, y = 0;
            [sc scanUpToString:@"{" intoString:nil];
            [sc scanString:@"{" intoString:nil];
            [sc scanFloat:&x];
            [sc scanString:@"," intoString:nil];
            [sc scanFloat:&y];
            return y;
        };
        _y0 = yAt(arr.firstObject);
        _y1 = yAt(arr.lastObject);
        _valid = YES;
    }
    return self;
}
- (void)encodeWithCoder:(NSCoder*)coder { (void)coder; }
@end

@interface IllusSilicaBrushStub : NSObject <NSCoding>
@property (nonatomic, strong) NSMutableDictionary<NSString*, id>* kv;
@end
@implementation IllusSilicaBrushStub
- (instancetype)initWithCoder:(NSCoder*)coder {
    self = [super init];
    if (!self) return nil;
    _kv = [NSMutableDictionary dictionary];
    // Strings are objects; sizes/opacities are typed floats — decodeObjectForKey on those
    // logs "value for key (…) is not an object" (and will error later).
    static NSString* const kStringKeys[] = { @"name", @"authorName", @"title", nil };
    static NSString* const kNumberKeys[] = {
        @"taperSize", @"pencilTaperSize", @"taperOpacity", @"taperPressure", @"pencilTaperOpacity",
        @"maxSize", @"minSize", @"size", @"diameter", @"paintSize", @"paintedSize", @"tipSize",
        @"streamline", @"stabilization", @"smoothing",
        @"hardness", @"hard", @"softness", @"soft",
        @"opacity", @"maxOpacity", @"paintOpacity",
        @"flow", @"accumulation", @"density",
        @"spacing", @"stampSpacing", @"plotSpacing",
        @"sizePressure", @"pressureSize", @"sizeDynamics", @"dynamicsPressureSize",
        @"opacityPressure", @"pressureOpacity", @"opacityDynamics", @"dynamicsPressureOpacity",
        @"angle", @"rotation", @"azimuth", @"shapeRotation",
        @"roundness", @"aspect", @"ellipticity", @"shapeRoundness",
        @"oriented", @"shapeOrientation",
        @"shapeInverted", @"textureScale", @"grainScale", @"grainDepth", @"grainDepthMinimum",
        nil
    };
    for (NSInteger i = 0; kStringKeys[i]; ++i) {
        id v = [coder decodeObjectForKey:kStringKeys[i]];
        if ([v isKindOfClass:[NSString class]]) _kv[kStringKeys[i]] = v;
    }
    for (NSInteger i = 0; kNumberKeys[i]; ++i) {
        if (![coder containsValueForKey:kNumberKeys[i]]) continue;
        _kv[kNumberKeys[i]] = @([coder decodeDoubleForKey:kNumberKeys[i]]);
    }
    // Pressure curves → endpoint Y (linear gain approximation).
    static NSString* const kCurveKeys[] = {
        @"dynamicsPressureSizeCurve_", @"dynamicsPressureSizeCurve",
        @"dynamicsPressureOpacityCurve_", @"dynamicsPressureOpacityCurve",
        nil
    };
    for (NSInteger i = 0; kCurveKeys[i]; ++i) {
        id v = nil;
        @try { v = [coder decodeObjectForKey:kCurveKeys[i]]; } @catch (NSException*) {}
        if ([v isKindOfClass:[IllusCurveStub class]] && ((IllusCurveStub*)v).valid) {
            IllusCurveStub* c = (IllusCurveStub*)v;
            NSString* base = kCurveKeys[i];
            if ([base containsString:@"Size"]) {
                _kv[@"sizeCurveY0"] = @(c.y0);
                _kv[@"sizeCurveY1"] = @(c.y1);
            } else {
                _kv[@"opacityCurveY0"] = @(c.y0);
                _kv[@"opacityCurveY1"] = @(c.y1);
            }
        }
    }
    return self;
}
- (void)encodeWithCoder:(NSCoder*)coder {
    (void)coder;
}
@end

namespace illus {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void ingestNumber(ProcreateArchiveFlat& flat, const std::string& key, float v) {
    if (key.empty()) return;
    const std::string k = lower(key);
    // First wins — stub decode runs before raw bplist walk; plist $objects otherwise
    // stomps maxSize/paintSize with unrelated floats (imported brushes → 1px rounds).
    if (flat.numbers.find(k) != flat.numbers.end()) return;
    flat.numbers[k] = v;
}

void ingestString(ProcreateArchiveFlat& flat, const std::string& key, const std::string& v) {
    if (key.empty() || v.empty()) return;
    const std::string k = lower(key);
    if (flat.strings.find(k) != flat.strings.end()) return;
    flat.strings[k] = v;
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
        NSUInteger n = MIN(arr.count, (NSUInteger)4);
        for (NSUInteger i = 0; i < n; ++i) {
            walk(arr[i], prefix + "[" + std::to_string(i) + "]", flat, depth + 1);
        }
        return;
    }
    if ([obj isKindOfClass:[NSNumber class]]) {
        ingestNumber(flat, prefix, [(NSNumber*)obj floatValue]);
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

const std::string* findStr(const ProcreateArchiveFlat& flat, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const auto it = flat.strings.find(lower(k));
        if (it != flat.strings.end() && !it->second.empty()) return &it->second;
    }
    return nullptr;
}

float norm01(float v) {
    if (v > 1.f && v <= 100.f) return std::clamp(v / 100.f, 0.f, 1.f);
    return std::clamp(v, 0.f, 1.f);
}

void ingestStub(IllusSilicaBrushStub* stub, ProcreateArchiveFlat& flat) {
    if (!stub.kv) return;
    for (NSString* key in stub.kv) {
        id v = stub.kv[key];
        const std::string k = [key UTF8String] ?: "";
        if ([v isKindOfClass:[NSString class]]) {
            ingestString(flat, k, std::string([v UTF8String] ?: ""));
        } else if ([v isKindOfClass:[NSNumber class]]) {
            ingestNumber(flat, k, [v floatValue]);
        }
    }
}

} // namespace

ProcreateArchiveFlat decodeProcreateArchive(const uint8_t* data, int32_t size) {
    ProcreateArchiveFlat flat;
    if (!data || size < 8) return flat;

    @autoreleasepool {
        static dispatch_once_t once;
        dispatch_once(&once, ^{
            [NSKeyedUnarchiver setClass:[IllusSilicaBrushStub class] forClassName:@"SilicaBrush"];
            [NSKeyedUnarchiver setClass:[IllusSilicaBrushStub class] forClassName:@"ValkyrieBrush"];
            [NSKeyedUnarchiver setClass:[IllusCurveStub class] forClassName:@"ValkyrieMagnitudinalCurve"];
        });

        NSData* nsData = [NSData dataWithBytes:data length:static_cast<NSUInteger>(size)];
        id root = nil;
        NSError* err = nil;

        if (@available(macOS 10.13, iOS 11, *)) {
            NSKeyedUnarchiver* un = [[NSKeyedUnarchiver alloc] initForReadingFromData:nsData error:&err];
            if (un) {
                un.requiresSecureCoding = NO;
                root = [un decodeObjectForKey:NSKeyedArchiveRootObjectKey];
                if (!root) root = [un decodeTopLevelObjectAndReturnError:&err];
                [un finishDecoding];
            }
        }

        if ([root isKindOfClass:[IllusSilicaBrushStub class]]) {
            ingestStub((IllusSilicaBrushStub*)root, flat);
        } else if (root) {
            walk(root, "", flat, 0);
        }

        // Also walk raw bplist ($objects) — picks up strings/numbers the stub missed.
        id plist = [NSPropertyListSerialization propertyListWithData:nsData
                                                              options:NSPropertyListImmutable
                                                               format:nil
                                                                error:nil];
        if (plist) walk(plist, "", flat, 0);
    }
    return flat;
}

void mapProcreateToPreset(const ProcreateArchiveFlat& flat, BrushPreset& preset, bool& approximated) {
    int hits = 0;
    bool hit = false;

    if (const std::string* name = findStr(flat, {"name", "title", "brushname", "localizedname"})) {
        if (name->size() >= 2) {
            preset.name = *name;
            ++hits;
        }
    }

    hit = false;
    // Prefer maxSize (Procreate brush units). Never treat paintSize (0..1 slider) as pixels.
    float size = findNum(flat, {"maxsize", "diameter", "tipsize"}, hit);
    if (!hit) {
        float maybe = findNum(flat, {"size"}, hit);
        if (hit && maybe >= 0.05f) size = maybe;
        else hit = false;
    }
    if (hit) {
        if (size > 200.f) size = 40.f + std::fmod(size, 160.f);
        // Procreate maxSize is often normalized (~0.2–3), not px — scale those up.
        float px = size;
        if (px > 0.f && px < 8.f) px *= 40.f; // 0.2→8, 1→40, 2.1→84
        preset.lineWidthPx = std::clamp(px, 1.f, 256.f);
        ++hits;
    }

    hit = false;
    float minSize = findNum(flat, {"minsize"}, hit);
    if (hit && minSize >= 0.f) {
        preset.minSize = std::clamp(minSize, 0.f, 1.f);
        ++hits;
    }

    // Pressure size curve endpoints → minSize + enable sizePressure.
    hit = false;
    float curveY0 = findNum(flat, {"sizecurvey0"}, hit);
    bool hitY1 = false;
    float curveY1 = findNum(flat, {"sizecurvey1"}, hitY1);
    if (hit && hitY1) {
        preset.minSize = std::clamp(curveY0, 0.f, 1.f);
        const float span = std::abs(curveY1 - curveY0);
        preset.sizePressure = span > 0.05f ? 1.f : 0.f;
        ++hits;
    }

    hit = false;
    float dynSize = findNum(flat, {"dynamicspressuresize", "sizepressure", "pressureSize", "sizeDynamics"}, hit);
    if (hit) {
        // 0 = pressure size off in Procreate.
        if (dynSize <= 0.f) preset.sizePressure = 0.f;
        else preset.sizePressure = norm01(dynSize);
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
    float opac = findNum(flat, {"opacity", "maxopacity", "paintopacity"}, hit);
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
    float spacing = findNum(flat, {"plotspacing", "spacing", "stampSpacing"}, hit);
    if (hit) {
        // 0.009 = already a fraction of diameter; 0.7–2 ≈ percent-ish; >2 = percent.
        if (spacing > 2.f) spacing = spacing / 100.f;
        else if (spacing > 0.25f) spacing = spacing * 0.1f;
        preset.spacing = std::clamp(spacing, 0.02f, 0.20f);
        ++hits;
    }

    hit = false;
    float opacP = findNum(flat, {"dynamicspressureopacity", "opacitypressure", "pressureOpacity", "opacityDynamics"}, hit);
    if (hit) {
        preset.opacityPressure = norm01(opacP);
        ++hits;
    }
    hit = false;
    float opacCurveY0 = findNum(flat, {"opacitycurvey0"}, hit);
    bool hitOpY1 = false;
    float opacCurveY1 = findNum(flat, {"opacitycurvey1"}, hitOpY1);
    if (hit && hitOpY1) {
        const float span = std::abs(opacCurveY1 - opacCurveY0);
        if (span > 0.05f) preset.opacityPressure = std::max(preset.opacityPressure, span);
        ++hits;
    }

    hit = false;
    float taper = findNum(flat, {"tapersize", "penciltapersize"}, hit);
    if (hit) {
        preset.taperSize = norm01(taper);
        ++hits;
    }
    // Prefer the stronger of taperSize / pencilTaperSize if both present.
    hit = false;
    float pencilTaper = findNum(flat, {"penciltapersize"}, hit);
    if (hit) preset.taperSize = std::max(preset.taperSize, norm01(pencilTaper));

    hit = false;
    float taperOp = findNum(flat, {"taperopacity", "penciltaperopacity"}, hit);
    if (hit) {
        preset.taperOpacity = norm01(taperOp);
        ++hits;
    }

    hit = false;
    float taperPr = findNum(flat, {"taperpressure"}, hit);
    if (hit) {
        preset.taperPressure = norm01(taperPr);
        ++hits;
    }

    hit = false;
    float angle = findNum(flat, {"shaperotation", "angle", "rotation", "azimuth"}, hit);
    if (hit) {
        preset.angleDeg = angle;
        ++hits;
    }

    hit = false;
    float roundness = findNum(flat, {"shaperoundness", "roundness", "aspect", "ellipticity"}, hit);
    if (hit) {
        preset.roundness = norm01(roundness);
        ++hits;
    }

    hit = false;
    float oriented = findNum(flat, {"oriented", "shapeorientation"}, hit);
    if (hit && oriented > 0.5f) {
        preset.orientTip = true;
        ++hits;
    }

    hit = false;
    float inverted = findNum(flat, {"shapeinverted"}, hit);
    if (hit && inverted > 0.5f) {
        preset.shapeInverted = true;
        ++hits;
    }

    hit = false;
    float gscale = findNum(flat, {"texturescale", "grainscale"}, hit);
    if (hit && gscale > 0.f) {
        preset.grainScale = std::clamp(gscale, 0.05f, 16.f);
        ++hits;
    }

    hit = false;
    float gdepth = findNum(flat, {"graindepth", "texturedepth"}, hit);
    // decodeDouble often yields 0 for missing/object-typed grainDepth; 0 would disable grain.
    // Keep preset default (1) unless we got a real positive depth.
    if (hit && gdepth > 1e-4f) {
        preset.grainDepth = norm01(gdepth);
        ++hits;
    }

    approximated = hits < 2;
}

void parseBrushsetPlistBytes(
    const uint8_t* data,
    int32_t size,
    std::string& outName,
    std::vector<std::string>& outBrushOrder
) {
    outName.clear();
    outBrushOrder.clear();
    if (!data || size < 8) return;
    @autoreleasepool {
        NSData* nsData = [NSData dataWithBytes:data length:static_cast<NSUInteger>(size)];
        id root = [NSPropertyListSerialization propertyListWithData:nsData
                                                             options:NSPropertyListImmutable
                                                              format:nil
                                                               error:nil];
        if (![root isKindOfClass:[NSDictionary class]]) return;
        NSDictionary* dict = (NSDictionary*)root;
        id name = dict[@"name"];
        if ([name isKindOfClass:[NSString class]]) {
            outName = [(NSString*)name UTF8String] ?: "";
        }
        id brushes = dict[@"brushes"];
        if ([brushes isKindOfClass:[NSArray class]]) {
            for (id item in (NSArray*)brushes) {
                if ([item isKindOfClass:[NSString class]]) {
                    outBrushOrder.push_back(std::string([(NSString*)item UTF8String] ?: ""));
                }
            }
        }
    }
}

} // namespace illus
