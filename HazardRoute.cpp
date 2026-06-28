#include "HazardRoute.h"

namespace sekhema {

CrystalRoute ComputeCrystalRoute(const std::vector<TrialMarker>& crystals,
                                 int px, int py, const PluginSDK::Context* ctx) {
    CrystalRoute route;
    if (!ctx || crystals.empty()) return route;

    PluginSDK::WalkableGridHandle grid = ctx->Terrain.GetWalkableGrid();
    const bool haveGrid = grid.Valid();
    route.walkable = haveGrid;

    // Greedy nearest-neighbour visit order (grid-space) from the player.
    std::vector<const TrialMarker*> rem;
    rem.reserve(crystals.size());
    for (const auto& c : crystals) rem.push_back(&c);
    int cx = px, cy = py;
    while (!rem.empty()) {
        size_t best = 0; long long bestD = 1LL << 62;
        for (size_t i = 0; i < rem.size(); ++i) {
            long long dx = static_cast<long long>(rem[i]->gridX) - cx;
            long long dy = static_cast<long long>(rem[i]->gridY) - cy;
            long long d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = i; }
        }
        cx = static_cast<int>(rem[best]->gridX);
        cy = static_cast<int>(rem[best]->gridY);
        route.stops.push_back({cx, cy});
        rem.erase(rem.begin() + static_cast<long>(best));
    }

    // Connect player -> stop[0] -> stop[1] -> ... with A* legs (straight fallback).
    auto leg = [&](GridPoint a, GridPoint b) {
        if (haveGrid) {
            auto p = FindWalkablePath(grid.Data(), grid.Width(), grid.Height(), grid.SizeBytes(),
                                      a.x, a.y, b.x, b.y);
            if (!p.empty()) {
                for (size_t i = (route.polyline.empty() ? 0 : 1); i < p.size(); ++i)
                    route.polyline.push_back(p[i]);
                return;
            }
            route.walkable = false;
        }
        if (route.polyline.empty()) route.polyline.push_back(a);
        route.polyline.push_back(b);
    };

    GridPoint prev{px, py};
    for (const auto& s : route.stops) { leg(prev, s); prev = s; }
    return route;
}

} // namespace sekhema
