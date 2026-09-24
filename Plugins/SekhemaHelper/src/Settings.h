#pragma once
#include "WeightProfiles.h"
#include "ChestTypes.h"
#include <imgui.h>          // ImVec2/ImVec4 POD types only
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sekhema {

// Per chest-content-type overlay settings. Every chest of a highlight-checked
// type gets the white ring (drawn on top of circles and labels).
struct ChestTypeSetting {
    std::string id;                              // ChestTypeInfo::id
    bool        show      = true;                // draw on the map at all
    bool        highlight = false;               // white ring on all chests of the type
    ImVec4      color{1.0f, 0.85f, 0.3f, 1.0f};  // circle fill
};

inline std::vector<ChestTypeSetting> DefaultChestTypes() {
    std::vector<ChestTypeSetting> v;
    size_t n = 0;
    const ChestTypeInfo* reg = ChestTypeRegistry(n);
    v.reserve(n);
    for (size_t i = 0; i < n; ++i)
        v.push_back({reg[i].id, reg[i].defaultShow, reg[i].defaultHighlight, reg[i].defaultColor});
    return v;
}

struct Settings {
    // display
    bool   drawBestPath     = true;
    float  frameThickness   = 2.5f;
    ImVec4 bestPathColor    = {0.20f, 0.85f, 1.00f, 1.00f};
    bool   dashboardVisible = true;
    bool   dashboardAutoShow= true;
    ImVec2 dashboardPos     = {40.0f, 120.0f};
    int    toggleVk         = 0x75; // VK_F6

    // timers (run / floor / room overlay + tracker diagnostics)
    bool   timerOverlayEnabled = true;
    ImVec2 timerOverlayPos     = {40.0f, 40.0f};
    bool   timerShowFloorLine  = true;
    bool   timerShowRoomLine   = true;
    bool   timerDebugLog       = false;

    // profiles
    std::string activeProfileName = "Default";
    std::vector<WeightProfile> profiles = DefaultProfiles();

    // overlays
    bool   showPortals = true, showLevers = true, showCrystals = true, showChests = true;
    ImVec4 portalColor = {0.85f,0.45f,1.0f,1.0f}, leverColor = {1.0f,0.8f,0.2f,1.0f};
    ImVec4 crystalColor= {0.3f,1.0f,0.9f,1.0f};
    float  poiRadius = 9.0f;        // crystals / portals / levers
    // chests: compact circles + tier-colored labels, per-type colors below
    float  chestRadius     = 6.0f;
    bool   showChestLabels = true;
    std::vector<ChestTypeSetting> chestTypes = DefaultChestTypes();

    WeightProfile* ActiveProfile();

    void Load(const std::filesystem::path& directory);   // LoadFromDisk + profile merge
    void Save(const std::filesystem::path& directory) const;

private:
    void LoadFromDisk(const std::filesystem::path& directory);
};

} // namespace sekhema
