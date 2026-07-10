//
//  BrushLibrary.cpp
//  IllusStudioFramework
//

#include "BrushLibrary.hpp"

#include "../math/Rect.hpp"
#include "../render/StrokeRasterizer.hpp"
#include "../strokes/StrokeSample.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace illus {
namespace {

float clampf(float v, float lo, float hi) {
    return std::clamp(v, lo, hi);
}

void nearestDownsample(
    const uint8_t* src,
    int32_t srcW,
    int32_t srcH,
    uint8_t* dst,
    int32_t dstW,
    int32_t dstH
) {
    for (int32_t y = 0; y < dstH; ++y) {
        const int32_t sy = std::min(srcH - 1, (y * srcH) / dstH);
        for (int32_t x = 0; x < dstW; ++x) {
            const int32_t sx = std::min(srcW - 1, (x * srcW) / dstW);
            const size_t si = (static_cast<size_t>(sy) * static_cast<size_t>(srcW) + static_cast<size_t>(sx)) * 4u;
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x)) * 4u;
            dst[di] = src[si];
            dst[di + 1] = src[si + 1];
            dst[di + 2] = src[si + 2];
            dst[di + 3] = src[si + 3];
        }
    }
}

/// Fit `tex` into dst (letterbox), transparent padding.
bool letterboxTexture(const BrushTexture& tex, int32_t outW, int32_t outH, uint8_t* outRGBA) {
    if (tex.width < 1 || tex.height < 1 || tex.rgba.empty()) return false;
    const float sx = static_cast<float>(outW) / static_cast<float>(tex.width);
    const float sy = static_cast<float>(outH) / static_cast<float>(tex.height);
    const float scale = std::min(sx, sy);
    const int32_t dw = std::max(1, static_cast<int32_t>(std::lround(tex.width * scale)));
    const int32_t dh = std::max(1, static_cast<int32_t>(std::lround(tex.height * scale)));
    const int32_t ox = (outW - dw) / 2;
    const int32_t oy = (outH - dh) / 2;
    for (int32_t y = 0; y < dh; ++y) {
        const int32_t syi = std::min(tex.height - 1, (y * tex.height) / dh);
        for (int32_t x = 0; x < dw; ++x) {
            const int32_t sxi = std::min(tex.width - 1, (x * tex.width) / dw);
            const size_t si =
                (static_cast<size_t>(syi) * static_cast<size_t>(tex.width) + static_cast<size_t>(sxi)) * 4u;
            const int32_t dx = ox + x;
            const int32_t dy = oy + y;
            if (dx < 0 || dy < 0 || dx >= outW || dy >= outH) continue;
            const size_t di =
                (static_cast<size_t>(dy) * static_cast<size_t>(outW) + static_cast<size_t>(dx)) * 4u;
            outRGBA[di] = tex.rgba[si];
            outRGBA[di + 1] = tex.rgba[si + 1];
            outRGBA[di + 2] = tex.rgba[si + 2];
            outRGBA[di + 3] = tex.rgba[si + 3];
        }
    }
    return true;
}

bool renderStrokeStrip(
    const BrushPreset& base,
    const BrushAssetStore& assets,
    int32_t outW,
    int32_t outH,
    uint8_t* outRGBA
) {
    constexpr int32_t kSrcW = 256;
    constexpr int32_t kSrcH = 64;
    std::vector<uint8_t> buf(static_cast<size_t>(kSrcW) * static_cast<size_t>(kSrcH) * 4u, 0);

    BrushPreset preset = base;
    preset.mode = BrushMode::Paint;
    preset.color = math::RGBA{255, 255, 255, 255};
    preset.opacity = 1.f;
    // Fit brush into strip height.
    preset.lineWidthPx = clampf(preset.lineWidthPx, 3.f, static_cast<float>(kSrcH) * 0.42f);
    // Match resolvedPreset: grain needs low flow; tip-only can go dense.
    if (preset.grainTextureId >= 0) {
        preset.spacing = std::min(std::max(preset.spacing, 0.04f), 0.14f);
        preset.flow = std::min(preset.flow, 0.32f);
    } else if (preset.tipTextureId >= 0) {
        preset.spacing = std::min(preset.spacing, 0.12f);
        preset.flow = std::max(preset.flow, 0.85f);
    }

    const float pad = 12.f;
    const float midY = static_cast<float>(kSrcH) * 0.55f;
    auto yAt = [&](float t) {
        return midY + std::sin(t * 6.2831853f) * static_cast<float>(kSrcH) * 0.18f;
    };

    float carry = 0.f;
    float strokeDist = 0.f;
    math::Rect dirty{};
    StrokeSample prev{pad, yAt(0.f), 1.f};
    {
        const BrushPreset dab = StrokeRasterizer::withStrokeDynamics(preset, 1.f, 0.f);
        StrokeRasterizer::stampDab(buf.data(), kSrcW, kSrcH, prev.x, prev.y, 1.f, dab, dirty, &assets);
    }
    constexpr int kSegs = 24;
    for (int i = 1; i <= kSegs; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegs);
        StrokeSample next{
            pad + t * (static_cast<float>(kSrcW) - 2.f * pad),
            yAt(t),
            1.f
        };
        StrokeRasterizer::stampSegment(
            buf.data(), kSrcW, kSrcH, prev, next, preset, carry, strokeDist, dirty, nullptr, &assets
        );
        prev = next;
    }

    nearestDownsample(buf.data(), kSrcW, kSrcH, outRGBA, outW, outH);
    return true;
}

} // namespace

BrushLibrary::BrushLibrary() {
    seedBuiltIns();
    // Defaults: Inking → ink.round; Drawing → erase.soft
    if (const BrushPreset* ink = findByName("ink.round")) lastPaintPresetId_ = ink->id;
    else if (!presets_.empty()) lastPaintPresetId_ = presets_[0].id;
    if (const BrushPreset* erase = findByName("erase.soft")) lastErasePresetId_ = erase->id;
    session_.presetId = lastPaintPresetId_;
    syncSessionFromPreset();
}

void BrushLibrary::seedBuiltIns() {
    auto addTo = [&](BrushSet& set, const char* name, BrushMode mode, float width, float smooth,
                     float hard, float opac, float flow, float spacing, float sizeP, float opacP) {
        BrushPreset p;
        p.id = nextPresetId_++;
        p.name = name;
        p.mode = mode;
        p.source = BrushSource::BuiltIn;
        p.lineWidthPx = width;
        p.lineSmooth = smooth;
        p.hardness = hard;
        p.opacity = opac;
        p.flow = flow;
        p.spacing = spacing;
        p.sizePressure = sizeP;
        p.opacityPressure = opacP;
        p.color = {20, 20, 20, 255};
        if (mode == BrushMode::Erase) p.color = {0, 0, 0, 255};
        set.presetIds.push_back(p.id);
        presets_.push_back(std::move(p));
    };

    auto makeSet = [&](const char* name) {
        BrushSet set;
        set.id = nextSetId_++;
        set.name = name;
        set.source = BrushSource::BuiltIn;
        return set;
    };

    // name, mode, width, smooth, hard, opac, flow, spacing, sizeP, opacP
    BrushSet sketching = makeSet("Sketching");
    addTo(sketching, "pencil.hard", BrushMode::Paint, 6.f, 0.05f, 0.95f, 1.f, 1.f, 0.12f, 1.f, 0.f);
    addTo(sketching, "pencil.soft", BrushMode::Paint, 10.f, 0.12f, 0.55f, 0.85f, 0.9f, 0.14f, 0.9f, 0.25f);
    addTo(sketching, "sketch.rough", BrushMode::Paint, 14.f, 0.08f, 0.7f, 0.7f, 0.85f, 0.15f, 0.85f, 0.15f);
    sets_.push_back(std::move(sketching));

    BrushSet inking = makeSet("Inking");
    addTo(inking, "ink.fine", BrushMode::Paint, 4.f, 0.08f, 0.98f, 1.f, 1.f, 0.1f, 0.6f, 0.f);
    addTo(inking, "ink.round", BrushMode::Paint, 16.f, 0.1f, 0.9f, 1.f, 1.f, 0.12f, 1.f, 0.f);
    addTo(inking, "ink.brush", BrushMode::Paint, 22.f, 0.15f, 0.75f, 1.f, 1.f, 0.12f, 1.f, 0.1f);
    sets_.push_back(std::move(inking));

    BrushSet drawing = makeSet("Drawing");
    addTo(drawing, "technical.pen", BrushMode::Paint, 3.f, 0.02f, 1.f, 1.f, 1.f, 0.1f, 0.f, 0.f);
    addTo(drawing, "marker", BrushMode::Paint, 18.f, 0.1f, 0.85f, 0.95f, 1.f, 0.12f, 0.5f, 0.f);
    addTo(drawing, "erase.soft", BrushMode::Erase, 24.f, 0.1f, 0.25f, 1.f, 1.f, 0.14f, 1.f, 0.f);
    addTo(drawing, "erase.hard", BrushMode::Erase, 16.f, 0.05f, 0.95f, 1.f, 1.f, 0.12f, 1.f, 0.f);
    sets_.push_back(std::move(drawing));

    BrushSet painting = makeSet("Painting");
    addTo(painting, "paint.round", BrushMode::Paint, 28.f, 0.15f, 0.45f, 0.9f, 0.85f, 0.12f, 0.9f, 0.2f);
    addTo(painting, "air.soft", BrushMode::Paint, 32.f, 0.2f, 0.2f, 0.6f, 0.5f, 0.15f, 0.8f, 0.4f);
    addTo(painting, "paint.wash", BrushMode::Paint, 40.f, 0.25f, 0.15f, 0.35f, 0.35f, 0.14f, 0.7f, 0.35f);
    sets_.push_back(std::move(painting));
}

const char* BrushLibrary::setName(int32_t setIndex) const {
    if (setIndex < 0 || setIndex >= setCount()) return "";
    return sets_[static_cast<size_t>(setIndex)].name.c_str();
}

BrushSource BrushLibrary::setSource(int32_t setIndex) const {
    if (setIndex < 0 || setIndex >= setCount()) return BrushSource::BuiltIn;
    return sets_[static_cast<size_t>(setIndex)].source;
}

int32_t BrushLibrary::setIdAt(int32_t setIndex) const {
    if (setIndex < 0 || setIndex >= setCount()) return -1;
    return sets_[static_cast<size_t>(setIndex)].id;
}

int32_t BrushLibrary::indexOfSetId(int32_t setId) const {
    for (int32_t i = 0; i < setCount(); ++i) {
        if (sets_[static_cast<size_t>(i)].id == setId) return i;
    }
    return -1;
}

int32_t BrushLibrary::presetCountInSet(int32_t setIndex) const {
    if (setIndex < 0 || setIndex >= setCount()) return 0;
    return static_cast<int32_t>(sets_[static_cast<size_t>(setIndex)].presetIds.size());
}

const char* BrushLibrary::presetName(int32_t index) const {
    if (index < 0 || index >= presetCount()) return "";
    return presets_[static_cast<size_t>(index)].name.c_str();
}

const char* BrushLibrary::presetNameInSet(int32_t setIndex, int32_t presetIndex) const {
    const int32_t id = presetIdInSet(setIndex, presetIndex);
    const BrushPreset* p = find(id);
    return p ? p->name.c_str() : "";
}

int32_t BrushLibrary::presetIdAt(int32_t index) const {
    if (index < 0 || index >= presetCount()) return -1;
    return presets_[static_cast<size_t>(index)].id;
}

int32_t BrushLibrary::presetIdInSet(int32_t setIndex, int32_t presetIndex) const {
    if (setIndex < 0 || setIndex >= setCount()) return -1;
    const auto& ids = sets_[static_cast<size_t>(setIndex)].presetIds;
    if (presetIndex < 0 || presetIndex >= static_cast<int32_t>(ids.size())) return -1;
    return ids[static_cast<size_t>(presetIndex)];
}

bool BrushLibrary::presetApproximated(int32_t setIndex, int32_t presetIndex) const {
    const BrushPreset* p = find(presetIdInSet(setIndex, presetIndex));
    return p ? p->approximated : false;
}

const BrushPreset* BrushLibrary::find(int32_t presetId) const {
    for (const auto& p : presets_) {
        if (p.id == presetId) return &p;
    }
    return nullptr;
}

BrushPreset* BrushLibrary::find(int32_t presetId) {
    for (auto& p : presets_) {
        if (p.id == presetId) return &p;
    }
    return nullptr;
}

const BrushPreset* BrushLibrary::findByName(const char* name) const {
    if (!name || !name[0]) return nullptr;
    for (const auto& p : presets_) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

bool BrushLibrary::setActivePreset(int32_t presetId) {
    const BrushPreset* p = find(presetId);
    if (!p) return false;
    session_.presetId = presetId;
    if (p->mode == BrushMode::Erase) {
        tool_ = ToolMode::Eraser;
        lastErasePresetId_ = presetId;
    } else {
        tool_ = ToolMode::Brush;
        lastPaintPresetId_ = presetId;
    }
    syncSessionFromPreset();
    return true;
}

void BrushLibrary::setTool(ToolMode mode) {
    tool_ = mode;
    if (mode == ToolMode::Eraser) {
        if (find(lastErasePresetId_)) session_.presetId = lastErasePresetId_;
        syncSessionFromPreset();
    } else if (mode == ToolMode::Brush) {
        if (find(lastPaintPresetId_)) session_.presetId = lastPaintPresetId_;
        syncSessionFromPreset();
    }
    // Pointer: keep last paint/erase presets; no stroke session change.
}

void BrushLibrary::syncSessionFromPreset() {
    session_.clearOverrides();
}

void BrushLibrary::resetSession() {
    syncSessionFromPreset();
}

BrushPreset BrushLibrary::resolvedPreset() const {
    const BrushPreset* base = find(session_.presetId);
    BrushPreset fallback;
    if (!base) base = &fallback;
    BrushPreset out = session_.resolve(*base);
    // Tool mode wins over preset mode when eraser tool selected with paint preset (and vice versa).
    if (tool_ == ToolMode::Eraser) out.mode = BrushMode::Erase;
    else if (out.mode == BrushMode::Erase && tool_ == ToolMode::Brush) {
        // keep erase if user picked erase preset while on brush? Prefer tool.
        out.mode = BrushMode::Paint;
    }
    out.lineWidthPx = std::max(0.5f, out.lineWidthPx);
    out.lineSmooth = clampf(out.lineSmooth, 0.f, 1.f);
    out.hardness = clampf(out.hardness, 0.f, 1.f);
    out.opacity = clampf(out.opacity, 0.f, 1.f);
    out.flow = clampf(out.flow, 0.f, 1.f);
    out.spacing = clampf(out.spacing, 0.01f, 2.f);
    out.sizePressure = clampf(out.sizePressure, 0.f, 1.f);
    out.opacityPressure = clampf(out.opacityPressure, 0.f, 1.f);
    out.minSize = clampf(out.minSize, 0.f, 1.f);
    out.taperSize = clampf(out.taperSize, 0.f, 1.f);
    out.taperOpacity = clampf(out.taperOpacity, 0.f, 1.f);
    out.taperPressure = clampf(out.taperPressure, 0.f, 1.f);
    out.grainScale = std::max(0.05f, out.grainScale);
    out.grainDepth = clampf(out.grainDepth, 0.f, 1.f);
    // Tip-only ink: dense solid. Grain/hatch: keep flow low so texture holes survive overlaps.
    if (out.grainTextureId >= 0) {
        // False-zero grainDepth from import would paint solid — treat as full punch.
        if (out.grainDepth < 0.05f) out.grainDepth = 1.f;
        out.spacing = std::min(std::max(out.spacing, 0.04f), 0.14f);
        out.flow = std::min(out.flow, 0.32f);
        out.hardness = std::max(out.hardness, 0.55f);
    } else if (out.tipTextureId >= 0) {
        out.spacing = std::min(out.spacing, 0.12f);
        out.flow = std::max(out.flow, 0.85f);
        out.hardness = std::max(out.hardness, 0.75f);
    } else {
        out.spacing = std::min(out.spacing, 0.15f);
    }
    return out;
}

int32_t BrushLibrary::saveSessionAsPreset(const char* name) {
    BrushPreset p = resolvedPreset();
    p.id = nextPresetId_++;
    p.name = (name && name[0]) ? name : "User Brush";
    p.source = BrushSource::User;

    // Ensure a User set exists.
    int32_t userSetIndex = -1;
    for (int32_t i = 0; i < setCount(); ++i) {
        if (sets_[static_cast<size_t>(i)].source == BrushSource::User) {
            userSetIndex = i;
            break;
        }
    }
    if (userSetIndex < 0) {
        BrushSet user;
        user.id = nextSetId_++;
        user.name = "User";
        user.source = BrushSource::User;
        sets_.push_back(std::move(user));
        userSetIndex = setCount() - 1;
    }
    sets_[static_cast<size_t>(userSetIndex)].presetIds.push_back(p.id);
    const int32_t id = p.id;
    if (p.mode == BrushMode::Erase) lastErasePresetId_ = id;
    else lastPaintPresetId_ = id;
    presets_.push_back(std::move(p));
    session_.presetId = id;
    syncSessionFromPreset();
    return id;
}

int32_t BrushLibrary::addImportedSet(const char* name, BrushSource source, std::vector<BrushPreset>& presets) {
    if (presets.empty()) return -1;
    BrushSet set;
    set.id = nextSetId_++;
    set.name = (name && name[0]) ? name : "Imported";
    set.source = source;
    for (BrushPreset& p : presets) {
        p.id = nextPresetId_++;
        if (p.name.empty()) p.name = "Brush";
        p.source = source;
        set.presetIds.push_back(p.id);
        if (p.mode == BrushMode::Erase) lastErasePresetId_ = p.id;
        else lastPaintPresetId_ = p.id;
        presets_.push_back(std::move(p));
    }
    const int32_t setId = set.id;
    sets_.push_back(std::move(set));
    return setId;
}

bool BrushLibrary::copyPresetPreviewRGBA(
    int32_t setIndex,
    int32_t presetIndex,
    int32_t outW,
    int32_t outH,
    uint8_t* outRGBA,
    int32_t outByteCount
) const {
    if (!outRGBA || outW < 1 || outH < 1) return false;
    const int32_t need = outW * outH * 4;
    if (outByteCount < need) return false;
    std::memset(outRGBA, 0, static_cast<size_t>(need));

    const int32_t id = presetIdInSet(setIndex, presetIndex);
    const BrushPreset* p = find(id);
    if (!p) return false;

    if (p->previewTextureId >= 0) {
        if (const BrushTexture* tex = assets_.find(p->previewTextureId)) {
            if (letterboxTexture(*tex, outW, outH, outRGBA)) return true;
        }
    }
    return renderStrokeStrip(*p, assets_, outW, outH, outRGBA);
}

} // namespace illus
