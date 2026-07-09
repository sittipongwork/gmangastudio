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
#include <sstream>
#include <unordered_map>
#include <unordered_set>

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

std::string parentDir(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
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

struct BrushFolder {
    std::string name;
    const ZipEntry* archive = nullptr;
    const ZipEntry* tip = nullptr;
    const ZipEntry* grain = nullptr;
    const ZipEntry* preview = nullptr;
};

void collectFolders(const std::vector<ZipEntry>& entries, std::unordered_map<std::string, BrushFolder>& folders) {
    for (const ZipEntry& e : entries) {
        const std::string base = basename(e.path);
        const std::string dir = parentDir(e.path);
        if (lower(base) == "brush.archive") {
            BrushFolder& f = folders[dir.empty() ? base : dir];
            if (f.name.empty()) f.name = dir.empty() ? "Imported Brush" : basename(dir);
            f.archive = &e;
            continue;
        }
        if (!isPngName(base)) continue;
        // Group by parent folder; root-level PNGs use empty key only with archive.
        BrushFolder& f = folders[dir];
        if (f.name.empty()) f.name = dir.empty() ? "Imported Brush" : basename(dir);
        if (looksLikeTip(base) && !f.tip) f.tip = &e;
        else if (looksLikeGrain(base) && !f.grain) f.grain = &e;
        else if (looksLikePreview(base) && !f.preview) f.preview = &e;
        else if (!f.tip) f.tip = &e; // first image as tip fallback
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

    // Nested .brushset inside .brushlibrary: re-import each zip entry that is itself a zip.
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
        // Also import any Brush.archive folders at top level.
        std::unordered_map<std::string, BrushFolder> folders;
        collectFolders(entries, folders);
        if (!folders.empty()) {
            std::string setName = (suggestedName && suggestedName[0]) ? suggestedName : "Imported Library";
            // strip extension
            const auto dot = setName.find_last_of('.');
            if (dot != std::string::npos) setName = setName.substr(0, dot);
            std::vector<BrushPreset> presets;
            for (auto& kv : folders) {
                BrushFolder& folder = kv.second;
                if (!folder.archive && !folder.tip) continue;
                BrushPreset p;
                p.name = folder.name;
                p.source = BrushSource::ImportedProcreate;
                p.mode = BrushMode::Paint;
                p.approximated = true;
                if (folder.archive) {
                    auto flat = decodeProcreateArchive(
                        folder.archive->data.data(), static_cast<int32_t>(folder.archive->data.size())
                    );
                    mapProcreateToPreset(flat, p, p.approximated);
                }
                if (folder.tip) {
                    p.tipTextureId = assets.addImageBytes(
                        folder.tip->data.data(), static_cast<int32_t>(folder.tip->data.size()), folder.tip->path.c_str()
                    );
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
                presets.push_back(std::move(p));
            }
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

    std::string setName = (suggestedName && suggestedName[0]) ? suggestedName : "Imported";
    {
        const auto dot = setName.find_last_of('.');
        if (dot != std::string::npos) setName = setName.substr(0, dot);
    }
    if (kind == BrushPackageKind::Brush && folders.size() == 1) {
        setName = folders.begin()->second.name;
    }

    std::vector<BrushPreset> presets;
    for (auto& kv : folders) {
        BrushFolder& folder = kv.second;
        if (!folder.archive && !folder.tip) continue;

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
        }
        if (folder.tip) {
            p.tipTextureId = assets.addImageBytes(
                folder.tip->data.data(), static_cast<int32_t>(folder.tip->data.size()), folder.tip->path.c_str()
            );
            if (p.tipTextureId < 0) p.approximated = true;
        }
        if (folder.grain) {
            p.grainTextureId = assets.addImageBytes(
                folder.grain->data.data(), static_cast<int32_t>(folder.grain->data.size()), folder.grain->path.c_str()
            );
        }
        if (folder.preview) {
            p.previewTextureId = assets.addImageBytes(
                folder.preview->data.data(),
                static_cast<int32_t>(folder.preview->data.size()),
                folder.preview->path.c_str()
            );
        }
        presets.push_back(std::move(p));
    }

    if (presets.empty()) return result;
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
