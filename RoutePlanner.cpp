#include "RoutePlanner.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>

namespace sekhema {

static const int OX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int OY[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const float OD[8] = { 1.0f, 1.41421356f, 1.0f, 1.41421356f,
                             1.0f, 1.41421356f, 1.0f, 1.41421356f };
static constexpr float kInf = std::numeric_limits<float>::infinity();

// One target's Dijkstra result over the region's sealed coarse grid, restricted
// to the player's component (inRoom) — fields stay room-sized on the huge
// shared floor map. dist[cell] = walk distance to the target; dir[cell] = 1..8
// step toward the target (0 = target/unreachable). Radar PathFinder's scheme.
struct Field {
    std::vector<float> dist;
    std::vector<uint8_t> dir;
    GridPoint target{};            // coarse, snapped
    bool valid = false;
};

// Spiral-snap a coarse point to the nearest in-room walkable cell.
static bool SnapToRoom(const RoomRegion& rg, GridPoint& p, int maxR = 6) {
    if (rg.CoarseInRoom(p)) return true;
    for (int r = 1; r <= maxR; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                GridPoint n{ p.x + dx, p.y + dy };
                if (rg.CoarseInRoom(n)) { p = n; return true; }
            }
    return false;
}

static Field BuildField(const RoomRegion& rg, GridPoint targetCoarse, int snapR) {
    Field f;
    f.target = targetCoarse;
    if (!SnapToRoom(rg, f.target, snapR)) return f;

    const int cells = rg.cw * rg.ch;
    f.dist.assign(static_cast<size_t>(cells), kInf);

    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    const int tIdx = f.target.y * rg.cw + f.target.x;
    f.dist[tIdx] = 0.0f;
    pq.push({ 0.0f, tIdx });

    while (!pq.empty()) {
        const float d = pq.top().first;
        const int uIdx = pq.top().second;
        pq.pop();
        if (d > f.dist[uIdx]) continue;
        const int ux = uIdx % rg.cw, uy = uIdx / rg.cw;
        for (int i = 0; i < 8; ++i) {
            const int nx = ux + OX[i], ny = uy + OY[i];
            if (static_cast<unsigned>(nx) >= static_cast<unsigned>(rg.cw) ||
                static_cast<unsigned>(ny) >= static_cast<unsigned>(rg.ch)) continue;
            const int nIdx = ny * rg.cw + nx;
            if (rg.inRoom[nIdx] == 0) continue;          // room component only
            const float nd = d + OD[i];
            if (nd < f.dist[nIdx]) { f.dist[nIdx] = nd; pq.push({ nd, nIdx }); }
        }
    }

    f.dir.assign(static_cast<size_t>(cells), 0);
    for (int y = 0; y < rg.ch; ++y)
        for (int x = 0; x < rg.cw; ++x) {
            const int idx = y * rg.cw + x;
            if (f.dist[idx] == kInf) continue;
            float best = f.dist[idx];
            int bestDir = 0;
            for (int i = 0; i < 8; ++i) {
                const int nx = x + OX[i], ny = y + OY[i];
                if (static_cast<unsigned>(nx) >= static_cast<unsigned>(rg.cw) ||
                    static_cast<unsigned>(ny) >= static_cast<unsigned>(rg.ch)) continue;
                const float nd = f.dist[ny * rg.cw + nx];
                if (nd < best) { best = nd; bestDir = i + 1; }
            }
            f.dir[idx] = static_cast<uint8_t>(bestDir);
        }
    f.valid = true;
    return f;
}

static float FieldDistAt(const RoomRegion& rg, const Field& f, GridPoint fromCoarse) {
    if (!f.valid) return kInf;
    GridPoint p = fromCoarse;
    if (!SnapToRoom(rg, p, 6)) return kInf;
    return f.dist[p.y * rg.cw + p.x];
}

// Walk `f.dir` from `fromCoarse` to the field's target, appending RAW points.
static bool WalkField(const RoomRegion& rg, const Field& f, GridPoint fromCoarse,
                      std::vector<GridPoint>& out) {
    if (!f.valid) return false;
    GridPoint cur = fromCoarse;
    if (!SnapToRoom(rg, cur, 6)) return false;
    if (f.dist[cur.y * rg.cw + cur.x] == kInf) return false;
    int guard = rg.cw * rg.ch + 8;
    while (!(cur.x == f.target.x && cur.y == f.target.y) && guard-- > 0) {
        const uint8_t d = f.dir[cur.y * rg.cw + cur.x];
        if (d == 0) break;
        cur = { cur.x + OX[d - 1], cur.y + OY[d - 1] };
        out.push_back(rg.ToRawCenter(cur));
    }
    return cur.x == f.target.x && cur.y == f.target.y;
}

PlannedRoute PlanCrystalRoute(const RoomRegion& rg, GridPoint playerRaw,
                              const std::vector<GridPoint>& crystalsRaw,
                              const GridPoint* door) {
    PlannedRoute out;
    if (crystalsRaw.empty()) return out;

    if (!rg.valid) {
        // No grid: straight fallback in the crystals' given order.
        out.polyline.push_back(playerRaw);
        for (const auto& c : crystalsRaw) { out.polyline.push_back(c); out.stops.push_back(c); }
        if (door) { out.hasDoor = true; out.doorPoint = *door; out.polyline.push_back(*door); }
        return out;
    }

    GridPoint playerC = rg.ToCoarse(static_cast<float>(playerRaw.x),
                                    static_cast<float>(playerRaw.y));
    if (!SnapToRoom(rg, playerC, 6)) {
        out.polyline.push_back(playerRaw);
        for (const auto& c : crystalsRaw) { out.polyline.push_back(c); out.stops.push_back(c); }
        return out;
    }

    // Build one field per reachable crystal (+ door). Crystals sit on props, so
    // snap generously (8 coarse cells); the door cell is sealed by RoomRegion,
    // so its snap lands on the near, in-room side of the doorway — exactly the
    // "walk up to the door" terminal we want.
    struct Target { GridPoint raw; Field field; };
    std::vector<Target> ts;
    ts.reserve(crystalsRaw.size());
    bool dropped = false;
    for (const auto& c : crystalsRaw) {
        Target t;
        t.raw = c;
        // Snap radius 4: enough for a crystal pedestal's unwalkable footprint,
        // small enough not to leak through a thin wall into another pocket.
        t.field = BuildField(rg, rg.ToCoarse(static_cast<float>(c.x), static_cast<float>(c.y)), 4);
        if (t.field.valid && FieldDistAt(rg, t.field, playerC) != kInf)
            ts.push_back(std::move(t));
        else
            dropped = true;
    }
    if (ts.empty()) {
        out.polyline.push_back(playerRaw);
        for (const auto& c : crystalsRaw) { out.polyline.push_back(c); out.stops.push_back(c); }
        return out;
    }

    Field doorField;
    if (door) {
        // Generous snap: the terminal may be a pedestal TILE anchor sitting
        // well BEHIND the sealed gate — land on the nearest in-room cell (the
        // gate's near side) so the route still walks up to the exit.
        doorField = BuildField(rg, rg.ToCoarse(static_cast<float>(door->x),
                                               static_cast<float>(door->y)), 16);
        if (doorField.valid && FieldDistAt(rg, doorField, playerC) != kInf) {
            out.hasDoor = true;
            out.doorPoint = *door;
        }
    }

    // Pairwise walk distances: d(i,j) = field_j.dist[target_i].
    const int n = static_cast<int>(ts.size());
    std::vector<float> dPlayer(n);                       // player -> i
    std::vector<std::vector<float>> dd(n, std::vector<float>(n, 0.0f));
    std::vector<float> dDoor(n, 0.0f);                   // i -> door
    for (int i = 0; i < n; ++i) {
        dPlayer[i] = FieldDistAt(rg, ts[i].field, playerC);
        for (int j = 0; j < n; ++j)
            if (i != j) dd[i][j] = FieldDistAt(rg, ts[j].field, ts[i].field.target);
        if (out.hasDoor) dDoor[i] = FieldDistAt(rg, doorField, ts[i].field.target);
    }

    // Visit order: exact Held-Karp (start = player, terminal = door when
    // present) for n <= 12; nearest-neighbour by field distance beyond that.
    std::vector<int> order;
    if (n <= 12) {
        const int FULL = 1 << n;
        std::vector<std::vector<float>> dp(FULL, std::vector<float>(n, kInf));
        std::vector<std::vector<int8_t>> par(FULL, std::vector<int8_t>(n, -1));
        for (int i = 0; i < n; ++i) dp[static_cast<size_t>(1) << i][i] = dPlayer[i];
        for (int mask = 1; mask < FULL; ++mask)
            for (int i = 0; i < n; ++i) {
                if (!(mask & (1 << i)) || dp[mask][i] == kInf) continue;
                for (int j = 0; j < n; ++j) {
                    if (mask & (1 << j)) continue;
                    const float nd = dp[mask][i] + dd[i][j];
                    const int nm = mask | (1 << j);
                    if (nd < dp[nm][j]) { dp[nm][j] = nd; par[nm][j] = static_cast<int8_t>(i); }
                }
            }
        int bestEnd = -1; float bestCost = kInf;
        for (int i = 0; i < n; ++i) {
            if (dp[FULL - 1][i] == kInf) continue;
            const float cost = dp[FULL - 1][i] + (out.hasDoor ? dDoor[i] : 0.0f);
            if (cost < bestCost) { bestCost = cost; bestEnd = i; }
        }
        if (bestEnd >= 0) {
            int mask = FULL - 1, cur = bestEnd;
            while (cur >= 0) {
                order.push_back(cur);
                const int p = par[mask][cur];
                mask &= ~(1 << cur);
                cur = p;
            }
            std::reverse(order.begin(), order.end());
        }
    }
    if (order.empty()) {                                  // greedy fallback
        std::vector<bool> used(n, false);
        float curBest; int curIdx; GridPoint curC = playerC;
        std::vector<float> dCur = dPlayer;
        for (int step = 0; step < n; ++step) {
            curBest = kInf; curIdx = -1;
            for (int i = 0; i < n; ++i)
                if (!used[i] && dCur[i] < curBest) { curBest = dCur[i]; curIdx = i; }
            if (curIdx < 0) break;
            used[curIdx] = true;
            order.push_back(curIdx);
            curC = ts[curIdx].field.target;
            for (int i = 0; i < n; ++i)
                if (!used[i]) dCur[i] = FieldDistAt(rg, ts[i].field, curC);
        }
    }

    // Assemble the polyline: player -> crystals in order (-> door), walking
    // each leg through the leg target's direction field.
    out.walkable = !dropped;
    out.polyline.push_back(rg.ToRawCenter(playerC));
    GridPoint cur = playerC;
    for (int oi : order) {
        if (!WalkField(rg, ts[oi].field, cur, out.polyline)) out.walkable = false;
        cur = ts[oi].field.target;
        out.stops.push_back(ts[oi].raw);
        out.polyline.push_back(ts[oi].raw);               // touch the actual crystal
        out.polyline.push_back(rg.ToRawCenter(cur));      // back on the walkable lattice
    }
    if (out.hasDoor) {
        if (!WalkField(rg, doorField, cur, out.polyline)) out.walkable = false;
        out.polyline.push_back(out.doorPoint);
    }
    return out;
}

} // namespace sekhema
