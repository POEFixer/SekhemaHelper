#pragma once
// WalkablePathfinder.h — A* over the game's 4-bit walkable grid. Pure (raw data),
// so it's standalone-testable; HazardRoute wraps the SDK WalkableGridHandle.
#include <cstdint>
#include <cstddef>
#include <vector>

namespace sekhema {

struct GridPoint { int x = 0; int y = 0; };

// Non-zero nibble == walkable (even col = low nibble, odd col = high nibble).
bool IsWalkable(const uint8_t* grid, int w, int h, std::size_t sizeBytes, int x, int y);

// 8-directional A* with corner-cut prevention. Returns the cell path start..goal
// (inclusive) or empty on failure / unwalkable endpoints / node-cap exceeded.
std::vector<GridPoint> FindWalkablePath(const uint8_t* grid, int w, int h, std::size_t sizeBytes,
                                        int sx, int sy, int gx, int gy, int maxNodes = 40000);

} // namespace sekhema
