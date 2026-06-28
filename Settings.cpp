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
    for (const auto& c : chestOrder) chestJson.push_back(json{{"id", c.first}, {"enabled", c.second}});

    json root = {
        {"drawBestPath", drawBestPath},
        {"frameThickness", frameThickness},
        {"bestPathColor", ColorToJson(bestPathColor)},
        {"dashboardVisible", dashboardVisible},
        {"dashboardAutoShow", dashboardAutoShow},
        {"dashboardPos", json::array({dashboardPos.x, dashboardPos.y})},
        {"toggleVk", toggleVk},
        {"activeProfileName", activeProfileName},
        {"profiles", profilesJson},
        {"showPortals", showPortals}, {"showLevers", showLevers},
        {"showCrystals", showCrystals}, {"showChests", showChests},
        {"portalColor", ColorToJson(portalColor)}, {"leverColor", ColorToJson(leverColor)},
        {"crystalColor", ColorToJson(crystalColor)}, {"chestColor", ColorToJson(chestColor)},
        {"poiRadius", poiRadius},
        {"roomRadius", roomRadius},
        {"chestOrder", chestJson},
    };

    std::ofstream out(p);          // fs::path overload — Unicode-safe on MSVC
    if (out.is_open()) out << root.dump(2);
}

void Settings::Load(const std::filesystem::path& directory) {
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
    chestColor   = JsonToColor(root.value("chestColor", json{}), chestColor);
    poiRadius    = root.value("poiRadius", poiRadius);
    roomRadius   = root.value("roomRadius", roomRadius);

    if (auto it = root.find("chestOrder"); it != root.end() && it->is_array() && !it->empty()) {
        chestOrder.clear();
        for (const auto& cj : *it)
            chestOrder.emplace_back(cj.value("id", std::string{}), cj.value("enabled", true));
    }
}

} // namespace sekhema
