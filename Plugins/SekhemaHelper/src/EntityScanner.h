#pragma once
// EntityScanner.h — classify the trial's important objects from the entity list.
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "RunTracker.h"   // DoorKind
#include <cstdint>
#include <string>
#include <vector>

namespace sekhema {

enum class MarkerType { Crystal, Chest, Portal, Lever };

struct TrialMarker {
    MarkerType  type   = MarkerType::Crystal;
    float       gridX  = 0, gridY = 0, worldZ = 0;
    bool        active = true;   // crystal uncollected / portal-lever not yet used
    int         chestTier = 0;   // 1=Bronze 2=Silver 3=Gold; 0=untiered (pots/urns)
    int         quality   = 0;   // 0=none/base, 2=Superior, 3=Prime (path digit)
    std::string label;           // chest content-type id (ChestTypes.h), else empty
    uint32_t    entityId = 0;
};

// Trial door (selector / completion / floor-transition) with its live
// StateMachine "open" state — the run-tracker's precise room-boundary signal.
struct TrialDoor {
    uint32_t    entityId = 0;
    DoorKind    kind = DoorKind::Selector;
    bool        open = false;
    float       gridX = 0, gridY = 0;
    const char* debugName = "";
};

// Current-floor boss entity sighting (path-matched per floor).
struct TrialBoss {
    uint32_t entityId = 0;
    bool     alive = false;
    int      hp = 0, maxHp = 0;
};

struct TrialEntities {
    std::vector<TrialMarker> crystals;
    std::vector<TrialMarker> chests;
    std::vector<TrialMarker> portals;
    std::vector<TrialMarker> levers;
    std::vector<TrialDoor>   doors;
    std::vector<TrialBoss>   bosses;
};

// Enumerate the entity snapshot and classify Sekhema crystals / chests / portals
// / levers / doors / floor bosses. Collected/opened/used objects are dropped
// (doors and bosses are always reported with their live state). Objects farther
// than `maxDist` grid units from the player are dropped too (all Sekhema floors
// share one big map, so this keeps results to the current room).
// `floorNum` (1..4) selects which boss paths to match; 0 disables boss matching.
TrialEntities ScanTrialEntities(const PluginSDK::Context* ctx,
                                float playerGridX, float playerGridY, float maxDist,
                                int floorNum);

} // namespace sekhema
