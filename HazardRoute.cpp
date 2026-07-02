#include "HazardRoute.h"

namespace sekhema {

CrystalRoute ComputeCrystalRoute(const std::vector<TrialMarker>& crystals,
                                 const std::vector<TrialDoor>& doors,
                                 const std::vector<GridPoint>& exitHints,
                                 const RoomRegion& region,
                                 int px, int py, int ex, int ey) {
    CrystalRoute route;
    if (crystals.empty()) return route;

    std::vector<GridPoint> points;
    points.reserve(crystals.size());
    for (const auto& c : crystals)
        points.push_back({ static_cast<int>(c.gridX), static_cast<int>(c.gridY) });

    // Exit door = a CLOSED completion door on this room's boundary, farthest
    // from the entry point (the entry seal is the other closed one, right where
    // the player came in).
    const TrialDoor* exit = nullptr;
    long long exitD = -1;
    for (const auto& d : doors) {
        if (d.kind != DoorKind::Completion || d.open) continue;
        if (region.valid && !region.Contains(d.gridX, d.gridY, 4)) continue;
        const long long dx = static_cast<long long>(d.gridX) - ex;
        const long long dy = static_cast<long long>(d.gridY) - ey;
        const long long dist = dx * dx + dy * dy;
        if (dist > exitD) { exitD = dist; exit = &d; }
    }

    GridPoint doorPt{};
    const GridPoint* doorArg = nullptr;
    if (exit) {
        doorPt = { static_cast<int>(exit->gridX), static_cast<int>(exit->gridY) };
        doorArg = &doorPt;
    } else {
        // Door entity not spawned yet (server-side, proximity-gated): fall back
        // to the static pedestal marker nearest the room — pick the candidate
        // within reach of the room, farthest from the entry point. The planner
        // snaps the terminal to the nearest in-room cell, so a hint sitting
        // BEHIND the closed gate still ends the route at the gate's near side.
        // Escape rooms are large (spans well over 500 cells) and the hint tile
        // anchor sits past the sealed gate — generous caps, the region gate
        // (dilated) still cuts other rooms' pedestals.
        long long hintD = -1;
        for (const auto& hpt : exitHints) {
            const long long pdx = static_cast<long long>(hpt.x) - px;
            const long long pdy = static_cast<long long>(hpt.y) - py;
            if (pdx * pdx + pdy * pdy > 900LL * 900LL) continue;   // other rooms' props
            if (region.valid && !region.Contains(static_cast<float>(hpt.x),
                                                 static_cast<float>(hpt.y), 20))
                continue;   // not near this room at all
            const long long dx = static_cast<long long>(hpt.x) - ex;
            const long long dy = static_cast<long long>(hpt.y) - ey;
            const long long dist = dx * dx + dy * dy;
            if (dist > hintD) { hintD = dist; doorPt = hpt; }
        }
        if (hintD >= 0) doorArg = &doorPt;
    }

    PlannedRoute planned = PlanCrystalRoute(region, { px, py }, points, doorArg);
    route.polyline = std::move(planned.polyline);
    route.stops    = std::move(planned.stops);
    route.walkable = planned.walkable;
    route.hasDoor  = planned.hasDoor;
    route.doorPoint= planned.doorPoint;
    return route;
}

} // namespace sekhema
