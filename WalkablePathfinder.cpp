#include "WalkablePathfinder.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace sekhema {

bool IsWalkable(const uint8_t* grid, int w, int h, std::size_t sizeBytes, int x, int y) {
    if (!grid || x < 0 || y < 0 || x >= w || y >= h) return false;
    int rowStride = w / 2;
    std::size_t idx = static_cast<std::size_t>(y) * rowStride + static_cast<std::size_t>(x / 2);
    if (idx >= sizeBytes) return false;
    uint8_t cell = (x & 1) ? (grid[idx] >> 4) : (grid[idx] & 0x0F);
    return cell != 0;
}

std::vector<GridPoint> FindWalkablePath(const uint8_t* grid, int w, int h, std::size_t sizeBytes,
                                        int sx, int sy, int gx, int gy, int maxNodes) {
    std::vector<GridPoint> out;
    if (!grid || w <= 0 || h <= 0) return out;
    if (!IsWalkable(grid, w, h, sizeBytes, sx, sy)) return out;
    if (!IsWalkable(grid, w, h, sizeBytes, gx, gy)) return out;

    auto id = [w](int x, int y) -> long long { return static_cast<long long>(y) * w + x; };
    auto heur = [&](int x, int y) -> float {
        int dx = std::abs(x - gx), dy = std::abs(y - gy);
        return static_cast<float>(std::max(dx, dy)) + 0.41421356f * std::min(dx, dy); // octile
    };

    struct Node { float f; int x, y; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };
    std::priority_queue<Node, std::vector<Node>, Cmp> open;
    std::unordered_map<long long, float> g;
    std::unordered_map<long long, long long> came;

    g[id(sx, sy)] = 0.0f;
    open.push({heur(sx, sy), sx, sy});

    const int dxs[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dys[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    int expanded = 0;
    bool found = false;

    while (!open.empty() && expanded < maxNodes) {
        Node cur = open.top(); open.pop();
        ++expanded;
        if (cur.x == gx && cur.y == gy) { found = true; break; }
        long long cid = id(cur.x, cur.y);
        auto git = g.find(cid);
        float cg = (git == g.end()) ? 1e30f : git->second;

        for (int k = 0; k < 8; ++k) {
            int nx = cur.x + dxs[k], ny = cur.y + dys[k];
            if (!IsWalkable(grid, w, h, sizeBytes, nx, ny)) continue;
            bool diag = (k >= 4);
            if (diag &&
                (!IsWalkable(grid, w, h, sizeBytes, cur.x + dxs[k], cur.y) ||
                 !IsWalkable(grid, w, h, sizeBytes, cur.x, cur.y + dys[k])))
                continue; // no cutting wall corners
            float ng = cg + (diag ? 1.41421356f : 1.0f);
            long long nid = id(nx, ny);
            auto it = g.find(nid);
            if (it == g.end() || ng < it->second) {
                g[nid] = ng;
                came[nid] = cid;
                open.push({ng + heur(nx, ny), nx, ny});
            }
        }
    }
    if (!found) return out;

    long long cur = id(gx, gy);
    const long long startId = id(sx, sy);
    std::vector<GridPoint> rev;
    while (true) {
        rev.push_back({static_cast<int>(cur % w), static_cast<int>(cur / w)});
        if (cur == startId) break;
        auto it = came.find(cur);
        if (it == came.end()) break;
        cur = it->second;
    }
    out.assign(rev.rbegin(), rev.rend());
    return out;
}

} // namespace sekhema
