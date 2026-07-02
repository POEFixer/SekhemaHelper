#include "RoomRegion.h"
#include <algorithm>

namespace sekhema {

// Raw 4-bit walkable read — game truth: even column = LOW nibble.
static inline bool RawWalkable(const uint8_t* grid, int w, int h, std::size_t sizeBytes,
                               int x, int y) {
    if (static_cast<unsigned>(x) >= static_cast<unsigned>(w) ||
        static_cast<unsigned>(y) >= static_cast<unsigned>(h)) return false;
    const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + x) / 2;
    if (idx >= sizeBytes) return false;
    const int shift = (x & 1) ? 4 : 0;
    return ((grid[idx] >> shift) & 0xF) != 0;
}

bool RoomRegion::Contains(float rawX, float rawY, int dilateCoarse) const {
    if (!valid) return false;
    GridPoint c = ToCoarse(rawX, rawY);
    if (CoarseInRoom(c)) return true;
    for (int r = 1; r <= dilateCoarse; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;   // ring only
                if (CoarseInRoom({ c.x + dx, c.y + dy })) return true;
            }
    return false;
}

RoomRegion BuildRoomRegion(const uint8_t* grid, int w, int h, std::size_t sizeBytes,
                           float playerX, float playerY,
                           const std::vector<DoorSeal>& doors,
                           int gridSize, int maxSpanRaw, int sealRadiusRaw) {
    RoomRegion rg;
    if (!grid || w <= 0 || h <= 0 || sizeBytes == 0 || gridSize <= 0) return rg;
    rg.gridSize = gridSize;

    const int px = static_cast<int>(playerX), py = static_cast<int>(playerY);
    int x0 = std::max(0, px - maxSpanRaw), y0 = std::max(0, py - maxSpanRaw);
    int x1 = std::min(w, px + maxSpanRaw), y1 = std::min(h, py + maxSpanRaw);
    if (x1 <= x0 || y1 <= y0) return rg;

    // Align the crop origin to the coarse lattice so coordinates stay stable.
    x0 -= x0 % gridSize; y0 -= y0 % gridSize;
    rg.cropX = x0; rg.cropY = y0;
    rg.cw = (x1 - x0 + gridSize - 1) / gridSize;
    rg.ch = (y1 - y0 + gridSize - 1) / gridSize;
    const int cells = rg.cw * rg.ch;
    if (cells <= 0) return rg;

    // Majority (>=50%) downsample — the radar PathFinder's rule. Hot loop
    // (runs on region rebuilds): per raw row, walk the packed bytes directly
    // instead of calling the bounds-checked per-cell reader millions of times.
    rg.walk.assign(static_cast<size_t>(cells), 0);
    std::vector<uint16_t> walkCnt(static_cast<size_t>(rg.cw), 0);
    std::vector<uint16_t> totCnt(static_cast<size_t>(rg.cw), 0);
    for (int cy = 0; cy < rg.ch; ++cy) {
        const int ry0 = y0 + cy * gridSize, ry1 = std::min(ry0 + gridSize, y1);
        std::fill(walkCnt.begin(), walkCnt.end(), static_cast<uint16_t>(0));
        std::fill(totCnt.begin(), totCnt.end(), static_cast<uint16_t>(0));
        for (int ry = ry0; ry < ry1 && ry < h; ++ry) {
            const std::size_t rowBase = static_cast<std::size_t>(ry) * w;
            for (int rx = x0; rx < x1 && rx < w; ++rx) {
                const std::size_t idx = (rowBase + rx) >> 1;
                if (idx >= sizeBytes) break;
                const int shift = (rx & 1) ? 4 : 0;   // even column = LOW nibble
                const int cx = (rx - x0) / gridSize;
                ++totCnt[cx];
                if (((grid[idx] >> shift) & 0xF) != 0) ++walkCnt[cx];
            }
        }
        uint8_t* rowOut = rg.walk.data() + static_cast<size_t>(cy) * rg.cw;
        for (int cx = 0; cx < rg.cw; ++cx)
            rowOut[cx] = (totCnt[cx] > 0 && walkCnt[cx] * 2 >= totCnt[cx]) ? 1 : 0;
    }

    // Seal every CLOSED door: zero a disc of coarse cells over its gap so the
    // flood cannot leak into the next pocket of the shared floor map.
    const int sealC = std::max(1, (sealRadiusRaw + gridSize - 1) / gridSize);
    for (const auto& d : doors) {
        if (!d.closed) continue;
        GridPoint c = rg.ToCoarse(static_cast<float>(d.pos.x), static_cast<float>(d.pos.y));
        for (int dy = -sealC; dy <= sealC; ++dy)
            for (int dx = -sealC; dx <= sealC; ++dx) {
                if (dx * dx + dy * dy > sealC * sealC) continue;
                GridPoint n{ c.x + dx, c.y + dy };
                if (rg.InBounds(n)) rg.walk[static_cast<size_t>(n.y) * rg.cw + n.x] = 0;
            }
    }

    // Snap the player cell to walkable (spiral, radius 5) and flood-fill.
    GridPoint start = rg.ToCoarse(playerX, playerY);
    if (!rg.CoarseWalkable(start)) {
        bool found = false;
        for (int r = 1; r <= 5 && !found; ++r)
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    GridPoint n{ start.x + dx, start.y + dy };
                    if (rg.CoarseWalkable(n)) { start = n; found = true; }
                }
        if (!found) return rg;   // player inside a fully sealed/unwalkable spot
    }

    rg.inRoom.assign(static_cast<size_t>(cells), 0);
    std::vector<int> stack;
    stack.reserve(1024);
    rg.inRoom[static_cast<size_t>(start.y) * rg.cw + start.x] = 1;
    stack.push_back(start.y * rg.cw + start.x);
    static const int OX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int OY[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    while (!stack.empty()) {
        const int idx = stack.back(); stack.pop_back();
        const int ux = idx % rg.cw, uy = idx / rg.cw;
        for (int i = 0; i < 8; ++i) {
            const int nx = ux + OX[i], ny = uy + OY[i];
            if (static_cast<unsigned>(nx) >= static_cast<unsigned>(rg.cw) ||
                static_cast<unsigned>(ny) >= static_cast<unsigned>(rg.ch)) continue;
            const int nIdx = ny * rg.cw + nx;
            if (rg.walk[nIdx] == 0 || rg.inRoom[nIdx] != 0) continue;
            rg.inRoom[nIdx] = 1;
            stack.push_back(nIdx);
        }
    }

    rg.valid = true;
    return rg;
}

} // namespace sekhema
