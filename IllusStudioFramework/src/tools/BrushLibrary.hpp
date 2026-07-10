//
//  BrushLibrary.hpp
//  IllusStudioFramework — built-in presets + user saves
//

#pragma once

#include "BrushAssetStore.hpp"
#include "BrushPreset.hpp"
#include "BrushSession.hpp"

#include "../../CanvasEditor.hpp" // ToolMode (public)

#include <cstdint>
#include <string>
#include <vector>

namespace illus {

struct BrushSet {
    int32_t id = 0;
    std::string name;
    BrushSource source = BrushSource::BuiltIn;
    std::vector<int32_t> presetIds;
};

class BrushLibrary {
public:
    BrushLibrary();

    int32_t setCount() const { return static_cast<int32_t>(sets_.size()); }
    const char* setName(int32_t setIndex) const;
    BrushSource setSource(int32_t setIndex) const;
    int32_t setIdAt(int32_t setIndex) const;
    int32_t indexOfSetId(int32_t setId) const;
    int32_t presetCountInSet(int32_t setIndex) const;
    int32_t presetCount() const { return static_cast<int32_t>(presets_.size()); }
    const char* presetName(int32_t index) const;
    const char* presetNameInSet(int32_t setIndex, int32_t presetIndex) const;
    int32_t presetIdAt(int32_t index) const;
    int32_t presetIdInSet(int32_t setIndex, int32_t presetIndex) const;
    bool presetApproximated(int32_t setIndex, int32_t presetIndex) const;

    const BrushPreset* find(int32_t presetId) const;
    BrushPreset* find(int32_t presetId);

    bool setActivePreset(int32_t presetId);
    int32_t activePresetId() const { return session_.presetId; }

    void setTool(ToolMode mode);
    ToolMode tool() const { return tool_; }

    BrushSession& session() { return session_; }
    const BrushSession& session() const { return session_; }

    BrushAssetStore& assets() { return assets_; }
    const BrushAssetStore& assets() const { return assets_; }

    void resetSession();
    /// Snapshot library ⊕ session for beginStroke.
    BrushPreset resolvedPreset() const;

    /// Append user preset from current session; returns new id or -1.
    int32_t saveSessionAsPreset(const char* name);

    /// Append imported set; assigns ids; returns set id or -1.
    int32_t addImportedSet(const char* name, BrushSource source, std::vector<BrushPreset>& presets);

    /// Brush list chip: QuickLook preview if present, else tip/grain stroke strip.
    bool copyPresetPreviewRGBA(
        int32_t setIndex,
        int32_t presetIndex,
        int32_t outW,
        int32_t outH,
        uint8_t* outRGBA,
        int32_t outByteCount
    ) const;

private:
    void seedBuiltIns();
    void syncSessionFromPreset();
    const BrushPreset* findByName(const char* name) const;
    int32_t lastPaintPresetId_ = 0;
    int32_t lastErasePresetId_ = 0;
    ToolMode tool_ = ToolMode::Brush;
    BrushSession session_;
    BrushAssetStore assets_;
    std::vector<BrushSet> sets_;
    std::vector<BrushPreset> presets_;
    int32_t nextPresetId_ = 1;
    int32_t nextSetId_ = 1;
};

} // namespace illus
