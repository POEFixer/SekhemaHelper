#pragma once
// RoomRegion.h — automatic current-room detection over the walkable grid.
//
// A Sekhema floor is one huge map whose rooms are closed pockets separated by
// walls and (Triggerable-blockage) doors. The player's room is therefore the
// connected component of walkable cells around the player once every CLOSED
// door's gap is sealed. No manual radius needed: walls and door state bound
// the flood themselves; when a door opens the region naturally grows through
// it.
//
// Pure (raw 4-bit grid in, POD out) so it is standalone-testable. Nibble
// packing follows the game truth used by the host radar RENDERER and
// GameClient_Map: even column = LOW nibble, odd column = HIGH nibble.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sekhema {

struct GridPoint { int x = 0; int y = 0; };

struct DoorSeal {
    GridPoint pos;        // raw grid coords
    bool closed = false;  // only closed doors seal
};

// Coarse (downsampled) view of the crop around the player + the player's
// flood component. All public helpers take RAW grid coordinates.
class RoomRegion {
public:
    bool valid = false;
    int gridSize = 3;                 // raw cells per coarse cell
    int cropX = 0, cropY = 0;         // raw origin of the crop
    int cw = 0, ch = 0;               // coarse dimensions
    std::vector<uint8_t> walk;        // coarse walkability AFTER door seals
    std::vector<uint8_t> inRoom;      // player's component (subset of walk)

    GridPoint ToCoarse(float rawX, float rawY) const {
        return { (static_cast<int>(rawX) - cropX) / gridSize,
                 (static_cast<int>(rawY) - cropY) / gridSize };
    }
    GridPoint ToRawCenter(GridPoint c) const {
        return { cropX + c.x * gridSize + gridSize / 2,
                 cropY + c.y * gridSize + gridSize / 2 };
    }
    bool InBounds(GridPoint c) const {
        return static_cast<unsigned>(c.x) < static_cast<unsigned>(cw) &&
               static_cast<unsigned>(c.y) < static_cast<unsigned>(ch);
    }
    bool CoarseWalkable(GridPoint c) const {
        return InBounds(c) && walk[c.y * cw + c.x] != 0;
    }
    bool CoarseInRoom(GridPoint c) const {
        return InBounds(c) && inRoom[c.y * cw + c.x] != 0;
    }

    // Is a raw-grid position part of the current room? `dilateCoarse` extends
    // membership to cells NEAR the component so objects standing on unwalkable
    // props (crystal pedestals, chest platforms) still count.
    bool Contains(float rawX, float rawY, int dilateCoarse = 3) const;
};

// Build the region: crop [player ± maxSpanRaw], majority-downsample, zero the
// coarse cells within `sealRadiusRaw` of every CLOSED door, then flood-fill
// (8-neighbour) from the player's cell (snapped to walkable within 5 cells).
RoomRegion BuildRoomRegion(const uint8_t* grid, int w, int h, std::size_t sizeBytes,
                           float playerX, float playerY,
                           const std::vector<DoorSeal>& doors,
                           int gridSize = 3, int maxSpanRaw = 800,
                           int sealRadiusRaw = 9);

} // namespace sekhema
