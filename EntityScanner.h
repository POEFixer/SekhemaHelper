#pragma once
// EntityScanner.h — classify the trial's important objects from the entity list.
#include "sdk/PluginSDK.h"
#include <cstdint>
#include <string>
#include <vector>

namespace sekhema {

enum class MarkerType { Crystal, Chest, Portal, Lever };

struct TrialMarker {
    MarkerType  type   = MarkerType::Crystal;
    float       gridX  = 0, gridY = 0, worldZ = 0;
    bool        active = true;   // crystal uncollected / portal-lever not yet used
    int         chestTier = 0;   // 1=Bronze 2=Silver 3=Gold
    std::string label;           // chest content (else empty)
    uint32_t    entityId = 0;
};

struct TrialEntities {
    std::vector<TrialMarker> crystals;
    std::vector<TrialMarker> chests;
    std::vector<TrialMarker> portals;
    std::vector<TrialMarker> levers;
};

// Enumerate the entity snapshot and classify Sekhema crystals / chests / portals
// / levers. Collected/opened/used objects are dropped. Objects farther than
// `maxDist` grid units from the player are dropped too (all Sekhema floors share
// one big map, so this keeps results to the current room).
TrialEntities ScanTrialEntities(const PluginSDK::Context* ctx,
                                float playerGridX, float playerGridY, float maxDist);

} // namespace sekhema
