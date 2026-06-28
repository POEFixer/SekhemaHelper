#pragma once
#include "WeightProfiles.h"
#include <imgui.h>          // ImVec2/ImVec4 POD types only
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sekhema {

struct Settings {
    // display
    bool   drawBestPath     = true;
    float  frameThickness   = 2.5f;
    ImVec4 bestPathColor    = {0.20f, 0.85f, 1.00f, 1.00f};
    bool   dashboardVisible = true;
    bool   dashboardAutoShow= true;
    ImVec2 dashboardPos     = {40.0f, 120.0f};
    int    toggleVk         = 0x75; // VK_F6

    // profiles
    std::string activeProfileName = "Default";
    std::vector<WeightProfile> profiles = DefaultProfiles();

    // overlays (fields now; consumed in Phase 4)
    bool   showPortals = true, showLevers = true, showCrystals = true, showChests = true;
    ImVec4 portalColor = {0.85f,0.45f,1.0f,1.0f}, leverColor = {1.0f,0.8f,0.2f,1.0f};
    ImVec4 crystalColor= {0.3f,1.0f,0.9f,1.0f},   chestColor  = {1.0f,0.85f,0.3f,1.0f};
    float  poiRadius = 9.0f;
    // Only mark trial objects within this grid distance of the player (all Sekhema
    // floors share one big map, so this keeps markers/route to the current room).
    float  roomRadius = 300.0f;
    // chest content priority: ordered (top=best) + enabled flag
    std::vector<std::pair<std::string,bool>> chestOrder = {
        {"GrandSpectrum", true}, {"RadiusJewels", true}, {"LargeRelic", true},
        {"Jewels", true}, {"Currency", false}, {"MediumRelic", false},
        {"SmallRelic", false}, {"Maps", false}, {"Generic", false},
    };

    WeightProfile* ActiveProfile();

    void Load(const std::filesystem::path& directory);
    void Save(const std::filesystem::path& directory) const;
};

} // namespace sekhema
