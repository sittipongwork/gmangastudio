//
//  ProcreateBrushImporter.cpp
//  IllusStudioFramework
//

#include "ProcreateBrushImporter.hpp"

#include "ProcreateBrushMap.hpp"
#include "ZipReader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace illus {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string basename(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string brushRoot(const std::string& path) {
    // UUID/… → UUID; files at zip root keep full basename key.
    const auto slash = path.find('/');
    return slash == std::string::npos ? path : path.substr(0, slash);
}

bool endsWith(const std::string& s, const char* suffix) {
    const std::string suf = suffix;
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

bool isPngName(const std::string& name) {
    const std::string n = lower(name);
    return endsWith(n, ".png") || endsWith(n, ".jpg") || endsWith(n, ".jpeg");
}

bool looksLikeTip(const std::string& name) {
    const std::string n = lower(name);
    return n.find("shape") != std::string::npos || n.find("tip") != std::string::npos
        || n == "shape.png" || n.find("stamp") != std::string::npos;
}

bool looksLikeGrain(const std::string& name) {
    const std::string n = lower(name);
    return n.find("grain") != std::string::npos || n.find("texture") != std::string::npos;
}

bool looksLikePreview(const std::string& name) {
    const std::string n = lower(name);
    return n.find("preview") != std::string::npos || n.find("quicklook") != std::string::npos
        || n.find("thumbnail") != std::string::npos;
}

bool looksLikeAuthorOrSignature(const std::string& path) {
    const std::string n = lower(path);
    return n.find("authorpicture") != std::string::npos || n.find("signature") != std::string::npos
        || n.find("/author/") != std::string::npos;
}

bool looksLikeUuid(const std::string& s) {
    // 8-4-4-4-12 hex with dashes
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

struct BrushFolder {
    std::string name; // folder id (often UUID); replaced by archive name when mapped
    const ZipEntry* archive = nullptr;
    const ZipEntry* tip = nullptr;
    const ZipEntry* grain = nullptr;
    const ZipEntry* preview = nullptr;
};

void collectFolders(const std::vector<ZipEntry>& entries, std::unordered_map<std::string, BrushFolder>& folders) {
    // Pass 1: only folders that contain Brush.archive are brushes (skip AuthorPicture orphans).
    for (const ZipEntry& e : entries) {
        if (lower(basename(e.path)) != "brush.archive") continue;
        const std::string root = brushRoot(e.path);
        BrushFolder& f = folders[root];
        f.name = root.empty() ? "Imported Brush" : root;
        f.archive = &e;
    }
    // Pass 2: attach tip/grain/preview under those roots only.
    for (const ZipEntry& e : entries) {
        const std::string base = basename(e.path);
        if (!isPngName(base)) continue;
        if (looksLikeAuthorOrSignature(e.path)) continue;
        const std::string root = brushRoot(e.path);
        auto it = folders.find(root);
        if (it == folders.end()) continue;
        BrushFolder& f = it->second;
        if (looksLikeTip(base) && !f.tip) f.tip = &e;
        else if (looksLikeGrain(base) && !f.grain) f.grain = &e;
        else if (looksLikePreview(base) && !f.preview) f.preview = &e;
        else if (!f.tip && lower(base) == "shape.png") f.tip = &e;
    }
}

BrushPackageKind sniffKind(const char* name, BrushPackageKind kind) {
    if (kind != BrushPackageKind::Auto) return kind;
    if (!name) return BrushPackageKind::BrushSet;
    const std::string n = lower(name);
    if (endsWith(n, ".brushlibrary")) return BrushPackageKind::BrushLibrary;
    if (endsWith(n, ".brush")) return BrushPackageKind::Brush;
    return BrushPackageKind::BrushSet;
}

struct BrushsetMeta {
    std::string name;
    std::vector<std::string> brushOrder; // UUID folder names
};

BrushsetMeta parseBrushsetPlist(const std::vector<ZipEntry>& entries) {
    BrushsetMeta meta;
    for (const ZipEntry& e : entries) {
        if (lower(basename(e.path)) != "brushset.plist") continue;
        parseBrushsetPlistBytes(
            e.data.data(),
            static_cast<int32_t>(e.data.size()),
            meta.name,
            meta.brushOrder
        );
        break;
    }
    return meta;
}

std::string stripExtension(std::string name) {
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

BrushPreset makePresetFromFolder(BrushFolder& folder, BrushAssetStore& assets) {
    BrushPreset p;
    p.name = folder.name.empty() ? "Brush" : folder.name;
    p.source = BrushSource::ImportedProcreate;
    p.mode = BrushMode::Paint;
    p.approximated = true;

    if (folder.archive) {
        auto flat = decodeProcreateArchive(
            folder.archive->data.data(), static_cast<int32_t>(folder.archive->data.size())
        );
        mapProcreateToPreset(flat, p, p.approximated);
        // If map left a UUID name, keep folder id only as last resort.
        if (looksLikeUuid(p.name) && !looksLikeUuid(folder.name) && !folder.name.empty()) {
            p.name = folder.name;
        }
    }
    if (folder.tip) {
        p.tipTextureId = assets.addImageBytes(
            folder.tip->data.data(), static_cast<int32_t>(folder.tip->data.size()), folder.tip->path.c_str()
        );
        if (p.tipTextureId < 0) p.approximated = true;
    }
    if (folder.grain) {
        p.grainTextureId = assets.addImageBytes(
            folder.grain->data.data(),
            static_cast<int32_t>(folder.grain->data.size()),
            folder.grain->path.c_str()
        );
    }
    if (folder.preview) {
        p.previewTextureId = assets.addImageBytes(
            folder.preview->data.data(),
            static_cast<int32_t>(folder.preview->data.size()),
            folder.preview->path.c_str()
        );
    }
    return p;
}

std::vector<BrushPreset> presetsFromFolders(
    std::unordered_map<std::string, BrushFolder>& folders,
    const BrushsetMeta& meta,
    BrushAssetStore& assets
) {
    std::vector<BrushPreset> presets;
    std::unordered_map<std::string, bool> used;

    auto addRoot = [&](const std::string& root) {
        auto it = folders.find(root);
        if (it == folders.end() || used[root]) return;
        if (!it->second.archive) return;
        used[root] = true;
        presets.push_back(makePresetFromFolder(it->second, assets));
    };

    for (const std::string& id : meta.brushOrder) addRoot(id);
    for (auto& kv : folders) addRoot(kv.first);
    return presets;
}

} // namespace

BrushImportResult importBrushPackageBytes(
    BrushLibrary& library,
    BrushAssetStore& assets,
    const uint8_t* data,
    int32_t size,
    BrushPackageKind kind,
    const char* suggestedName
) {
    BrushImportResult result;
    if (!data || size < 30) return result;

    std::vector<ZipEntry> entries;
    if (!zipExtractAll(data, size, entries)) return result;

    kind = sniffKind(suggestedName, kind);

    if (kind == BrushPackageKind::BrushLibrary) {
        int32_t total = 0;
        int32_t sets = 0;
        int32_t lastSet = -1;
        for (const ZipEntry& e : entries) {
            const std::string n = lower(e.path);
            if (!(endsWith(n, ".brushset") || endsWith(n, ".brush") || endsWith(n, ".zip"))) continue;
            if (e.data.size() < 30) continue;
            auto sub = importBrushPackageBytes(
                library,
                assets,
                e.data.data(),
                static_cast<int32_t>(e.data.size()),
                endsWith(n, ".brush") ? BrushPackageKind::Brush : BrushPackageKind::BrushSet,
                basename(e.path).c_str()
            );
            if (sub.ok) {
                total += sub.brushCount;
                sets += std::max(1, sub.setCount);
                lastSet = sub.setId;
            }
        }
        std::unordered_map<std::string, BrushFolder> folders;
        collectFolders(entries, folders);
        if (!folders.empty()) {
            BrushsetMeta meta = parseBrushsetPlist(entries);
            std::string setName = !meta.name.empty()
                ? meta.name
                : ((suggestedName && suggestedName[0]) ? stripExtension(suggestedName) : "Imported Library");
            auto presets = presetsFromFolders(folders, meta, assets);
            if (!presets.empty()) {
                lastSet = library.addImportedSet(setName.c_str(), BrushSource::ImportedProcreate, presets);
                total += static_cast<int32_t>(presets.size());
                ++sets;
            }
        }
        result.ok = total > 0;
        result.brushCount = total;
        result.setCount = sets;
        result.setId = lastSet;
        return result;
    }

    std::unordered_map<std::string, BrushFolder> folders;
    collectFolders(entries, folders);
    if (folders.empty()) return result;

    BrushsetMeta meta = parseBrushsetPlist(entries);
    std::string setName = !meta.name.empty()
        ? meta.name
        : ((suggestedName && suggestedName[0]) ? stripExtension(suggestedName) : "Imported");
    if (kind == BrushPackageKind::Brush && folders.size() == 1) {
        // Single .brush: prefer archive name after map.
        setName = folders.begin()->second.name;
    }

    auto presets = presetsFromFolders(folders, meta, assets);
    if (presets.empty()) return result;
    if (kind == BrushPackageKind::Brush && presets.size() == 1 && !presets[0].name.empty()) {
        setName = presets[0].name;
    }

    result.setId = library.addImportedSet(setName.c_str(), BrushSource::ImportedProcreate, presets);
    result.brushCount = static_cast<int32_t>(presets.size());
    result.setCount = 1;
    result.ok = result.setId >= 0;
    return result;
}

BrushImportResult importBrushPackagePath(
    BrushLibrary& library,
    BrushAssetStore& assets,
    const char* path,
    BrushPackageKind kind
) {
    BrushImportResult result;
    if (!path || !path[0]) return result;
    std::ifstream in(path, std::ios::binary);
    if (!in) return result;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return result;
    const char* name = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return importBrushPackageBytes(
        library, assets, bytes.data(), static_cast<int32_t>(bytes.size()), kind, name
    );
}

} // namespace illus
