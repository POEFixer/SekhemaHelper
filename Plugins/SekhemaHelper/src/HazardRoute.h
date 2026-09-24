#pragma once
// HazardRoute.h — crystal Escape collection route: RoomRegion-bounded fields
// (radar PathFinder port) + exact door-terminal ordering (RoutePlanner). The
// route ends at the room's exit door (the completion door that opens once all
// crystals are active), so the last crystal visited is the one nearest it.
#include "EntityScanner.h"
#include "RoomRegion.h"
#include "RoutePlanner.h"
#include <vector>

namespace sekhema {

struct CrystalRoute {
    std::vector<GridPoint> polyline;  // walkable path player -> crystals (-> exit door)
    std::vector<GridPoint> stops;     // crystal positions in visit order (for numbering)
    bool walkable = false;            // false if any leg fell back to a straight segment
    bool hasDoor = false;             // route ends at the exit door
    GridPoint doorPoint{};            // exit-door position (end marker)
};

// Plan the route inside the auto-detected room. `doors` = all scanned trial
// doors (the exit is picked here: a CLOSED completion door touching the room,
// farthest from the room's entry point). Doors are SERVER entities that only
// appear near the player, so `exitHints` (static terrain markers — the
// KethAscendancy pedestal tgt at the airlock) provide the terminal before the
// door spawns; a visible door wins over a hint. entryGrid* = player position
// when the room was entered (disambiguates the entry seal from the exit).
CrystalRoute ComputeCrystalRoute(const std::vector<TrialMarker>& crystals,
                                 const std::vector<TrialDoor>& doors,
                                 const std::vector<GridPoint>& exitHints,
                                 const RoomRegion& region,
                                 int playerGridX, int playerGridY,
                                 int entryGridX, int entryGridY);

} // namespace sekhema
