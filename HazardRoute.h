#pragma once
// HazardRoute.h — crystal Escape collection route (greedy order + A* legs).
#include "EntityScanner.h"
#include "WalkablePathfinder.h"
#include "sdk/PluginSDK.h"
#include <vector>

namespace sekhema {

struct CrystalRoute {
    std::vector<GridPoint> polyline;  // walkable path player -> all crystals (grid coords)
    std::vector<GridPoint> stops;     // crystal positions in visit order (for numbering)
    bool walkable = false;            // false if any leg fell back to a straight segment
};

// Order the crystals greedily from the player and connect them with A* legs over
// the walkable grid. Computed off the render thread cadence (it's heavy) and
// cached; the renderer just projects the grid points each frame.
CrystalRoute ComputeCrystalRoute(const std::vector<TrialMarker>& crystals,
                                 int playerGridX, int playerGridY,
                                 const PluginSDK::Context* ctx);

} // namespace sekhema
