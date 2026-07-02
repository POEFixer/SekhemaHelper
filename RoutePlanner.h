#pragma once
// RoutePlanner.h — obstacle-aware crystal collection route (radar PathFinder
// port: per-target Dijkstra distance/direction fields over the RoomRegion's
// coarse grid) + exact visit ordering (Held-Karp) with the room's exit door as
// the fixed terminal, so the LAST crystal is the one nearest the door.
//
// Pure (RoomRegion + points in, polyline out) — standalone-testable.
#include "RoomRegion.h"
#include <vector>

namespace sekhema {

struct PlannedRoute {
    std::vector<GridPoint> polyline;   // raw grid coords: player -> crystals[..] (-> door)
    std::vector<GridPoint> stops;      // crystal raw positions in visit order
    bool walkable = false;             // false when any leg fell back to a straight line
    bool hasDoor = false;              // door terminal was used
    GridPoint doorPoint{};             // raw door position (for the end marker)
};

// Plan the route inside `region` (its sealed coarse grid bounds all walking).
// `door` (nullable) fixes the route's end; unreachable crystals are skipped
// (walkable=false then). Exact ordering up to 12 crystals, greedy beyond.
PlannedRoute PlanCrystalRoute(const RoomRegion& region,
                              GridPoint playerRaw,
                              const std::vector<GridPoint>& crystalsRaw,
                              const GridPoint* door);

} // namespace sekhema
