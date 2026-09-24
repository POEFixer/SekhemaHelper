#include "Settings.h"
#include <nlohmann/json.hpp>   // resolves to lib/nlohmann/json.hpp via include dir
#include <fstream>

using nlohmann::json;

namespace sekhema {

WeightProfile* Settings::ActiveProfile() {
    for (auto& p : profiles)
        if (p.name == activeProfileName) return &p;
    return profiles.empty() ? nullptr : &profiles.front();
}

static json ColorToJson(const ImVec4& c) { return json::array({c.x, c.y, c.z, c.w}); }
static ImVec4 JsonToColor(const json& j, ImVec4 def) {
    if (j.is_array() && j.size() == 4)
        return ImVec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    return def;
}

static json ProfileToJson(const WeightProfile& p) {
    return json{
        {"name", p.name},
        {"roomTypeWeights", p.roomTypeWeights},
        {"afflictionWeights", p.afflictionWeights},
        {"rewardWeights", p.rewardWeights},
        {"avoidMerchantBelowWater", p.avoidMerchantBelowWater},
        {"avoidHonourAbovePct", p.avoidHonourAbovePct},
    };
}

static WeightProfile ProfileFromJson(const json& j) {
    WeightProfile p;
    p.name = j.value("name", std::string{});
    p.roomTypeWeights   = j.value("roomTypeWeights",   std::map<std::string,float>{});
    p.afflictionWeights = j.value("afflictionWeights", std::map<std::string,float>{});
    p.rewardWeights     = j.value("rewardWeights",     std::map<std::string,float>{});
    p.avoidMerchantBelowWater = j.value("avoidMerchantBelowWater", 120.0f);
    p.avoidHonourAbovePct     = j.value("avoidHonourAbovePct", 75.0f);
    return p;
}

void Settings::Save(const std::filesystem::path& directory) const {
    std::filesystem::path p = directory / "config" / "settings.json";
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);

    json profilesJson = json::array();
    for (const auto& pr : profiles) profilesJson.push_back(ProfileToJson(pr));

    json chestJson = json::array();
    for (const auto& c : chestTypes)
        chestJson.push_back(json{{"id", c.id}, {"show", c.show},
                                 {"highlight", c.highlight}, {"color", ColorToJson(c.color)}});

    json root = {
        {"drawBestPath", drawBestPath},
        {"frameThickness", frameThickness},
        {"bestPathColor", ColorToJson(bestPathColor)},
        {"dashboardVisible", dashboardVisible},
        {"dashboardAutoShow", dashboardAutoShow},
        {"dashboardPos", json::array({dashboardPos.x, dashboardPos.y})},
        {"toggleVk", toggleVk},
        {"timerOverlayEnabled", timerOverlayEnabled},
        {"timerOverlayPos", json::array({timerOverlayPos.x, timerOverlayPos.y})},
        {"timerShowFloorLine", timerShowFloorLine},
        {"timerShowRoomLine", timerShowRoomLine},
        {"timerDebugLog", timerDebugLog},
        {"activeProfileName", activeProfileName},
        {"profiles", profilesJson},
        {"showPortals", showPortals}, {"showLevers", showLevers},
        {"showCrystals", showCrystals}, {"showChests", showChests},
        {"portalColor", ColorToJson(portalColor)}, {"leverColor", ColorToJson(leverColor)},
        {"crystalColor", ColorToJson(crystalColor)},
        {"poiRadius", poiRadius},
        {"chestRadius", chestRadius},
        {"showChestLabels", showChestLabels},
        {"chestTypes", chestJson},
    };

    std::ofstream out(p);          // fs::path overload — Unicode-safe on MSVC
    if (out.is_open()) out << root.dump(2);
}

void Settings::Load(const std::filesystem::path& directory) {
    LoadFromDisk(directory);
    // Reconcile saved profiles with the current defaults + affliction catalog
    // (new weights / new game-patch afflictions appear, stale names drop).
    for (auto& p : profiles) MergeProfileDefaults(p);
}

void Settings::LoadFromDisk(const std::filesystem::path& directory) {
    std::filesystem::path p = directory / "config" / "settings.json";
    if (!std::filesystem::exists(p)) return;
    std::ifstream in(p);
    if (!in.is_open()) return;
    json root;
    try { in >> root; } catch (...) { return; }

    drawBestPath     = root.value("drawBestPath", drawBestPath);
    frameThickness   = root.value("frameThickness", frameThickness);
    bestPathColor    = JsonToColor(root.value("bestPathColor", json{}), bestPathColor);
    dashboardVisible = root.value("dashboardVisible", dashboardVisible);
    dashboardAutoShow= root.value("dashboardAutoShow", dashboardAutoShow);
    if (auto it = root.find("dashboardPos"); it != root.end() && it->is_array() && it->size()==2)
        dashboardPos = ImVec2((*it)[0].get<float>(), (*it)[1].get<float>());
    toggleVk         = root.value("toggleVk", toggleVk);
    timerOverlayEnabled = root.value("timerOverlayEnabled", timerOverlayEnabled);
    if (auto it = root.find("timerOverlayPos"); it != root.end() && it->is_array() && it->size()==2)
        timerOverlayPos = ImVec2((*it)[0].get<float>(), (*it)[1].get<float>());
    timerShowFloorLine = root.value("timerShowFloorLine", timerShowFloorLine);
    timerShowRoomLine  = root.value("timerShowRoomLine", timerShowRoomLine);
    timerDebugLog      = root.value("timerDebugLog", timerDebugLog);
    activeProfileName= root.value("activeProfileName", activeProfileName);

    if (auto it = root.find("profiles"); it != root.end() && it->is_array() && !it->empty()) {
        profiles.clear();
        for (const auto& pj : *it) profiles.push_back(ProfileFromJson(pj));
    }

    showPortals  = root.value("showPortals", showPortals);
    showLevers   = root.value("showLevers", showLevers);
    showCrystals = root.value("showCrystals", showCrystals);
    showChests   = root.value("showChests", showChests);
    portalColor  = JsonToColor(root.value("portalColor", json{}), portalColor);
    leverColor   = JsonToColor(root.value("leverColor", json{}), leverColor);
    crystalColor = JsonToColor(root.value("crystalColor", json{}), crystalColor);
    poiRadius    = root.value("poiRadius", poiRadius);
    chestRadius     = root.value("chestRadius", chestRadius);
    showChestLabels = root.value("showChestLabels", showChestLabels);

    if (auto it = root.find("chestTypes"); it != root.end() && it->is_array() && !it->empty()) {
        // Saved order wins; unknown ids are dropped, registry types missing from
        // the file (added in an update) are appended with their defaults.
        std::vector<ChestTypeSetting> loaded;
        for (const auto& cj : *it) {
            std::string id = cj.value("id", std::string{});
            const ChestTypeInfo* info = FindChestTypeInfo(id);
            if (!info) continue;
            ChestTypeSetting c;
            c.id        = id;
            c.show      = cj.value("show", info->defaultShow);
            c.highlight = cj.value("highlight", info->defaultHighlight);
            c.color     = JsonToColor(cj.value("color", json{}), info->defaultColor);
            loaded.push_back(std::move(c));
        }
        for (const auto& def : DefaultChestTypes()) {
            bool have = false;
            for (const auto& c : loaded) if (c.id == def.id) { have = true; break; }
            if (!have) loaded.push_back(def);
        }
        if (!loaded.empty()) chestTypes = std::move(loaded);
    } else if (auto lg = root.find("chestOrder"); lg != root.end() && lg->is_array()) {
        // Legacy config (pre per-type settings): reuse its priority order and
        // enabled flags as the highlight set; colors fall back to defaults.
        std::vector<ChestTypeSetting> migrated;
        for (const auto& cj : *lg) {
            std::string id = cj.value("id", std::string{});
            const ChestTypeInfo* info = FindChestTypeInfo(id);
            if (!info) continue;
            migrated.push_back({id, info->defaultShow, cj.value("enabled", false), info->defaultColor});
        }
        for (const auto& def : DefaultChestTypes()) {
            bool have = false;
            for (const auto& c : migrated) if (c.id == def.id) { have = true; break; }
            if (!have) migrated.push_back({def.id, def.show, false, def.color});
        }
        if (!migrated.empty()) chestTypes = std::move(migrated);
    }
}

} // namespace sekhema
