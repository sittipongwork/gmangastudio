//
//  BrushLibrary.cpp
//  IllusStudioFramework
//

#include "BrushLibrary.hpp"

#include <algorithm>
#include <cmath>

namespace illus {
namespace {

float clampf(float v, float lo, float hi) {
    return std::clamp(v, lo, hi);
}

} // namespace

BrushLibrary::BrushLibrary() {
    seedBuiltIns();
    lastPaintPresetId_ = presets_[0].id; // ink.round
    lastErasePresetId_ = presets_[2].id; // erase.soft
    session_.presetId = lastPaintPresetId_;
    syncSessionFromPreset();
}

void BrushLibrary::seedBuiltIns() {
    BrushSet builtIn;
    builtIn.id = nextSetId_++;
    builtIn.name = "Built-in";
    builtIn.source = BrushSource::BuiltIn;

    auto add = [&](const char* name, BrushMode mode, float width, float smooth, float hard,
                   float opac, float flow, float spacing, float sizeP, float opacP, math::RGBA color) {
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
        p.color = color;
        builtIn.presetIds.push_back(p.id);
        presets_.push_back(std::move(p));
    };

    // ink.round, air.soft, erase.soft, erase.hard
    add("ink.round", BrushMode::Paint, 16.f, 0.1f, 0.9f, 1.f, 1.f, 0.2f, 1.f, 0.f, {20, 20, 20, 255});
    add("air.soft", BrushMode::Paint, 32.f, 0.2f, 0.2f, 0.6f, 0.5f, 0.35f, 0.8f, 0.4f, {20, 20, 20, 255});
    add("erase.soft", BrushMode::Erase, 24.f, 0.1f, 0.25f, 1.f, 1.f, 0.25f, 1.f, 0.f, {0, 0, 0, 255});
    add("erase.hard", BrushMode::Erase, 16.f, 0.05f, 0.95f, 1.f, 1.f, 0.2f, 1.f, 0.f, {0, 0, 0, 255});

    sets_.push_back(std::move(builtIn));
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
    // ponytail: until T1-7-3b tip/grain, imported tip presets stamp as solid round —
    // Procreate spacing/flow/softness otherwise produce scalloped muddy dabs.
    if (out.tipTextureId >= 0) {
        out.spacing = std::min(out.spacing, 0.10f);
        out.flow = std::max(out.flow, 0.95f);
        out.hardness = std::max(out.hardness, 0.88f);
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

} // namespace illus
