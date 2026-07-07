#include "RoomClassifier.h"

namespace sekhema {

void ClassifyFk(SekhemaRoom& room, const std::string& tablePath,
                const std::string& rowId, const std::string& rowName,
                std::string* outFloorTileset) {
    if (tablePath.empty()) return;

    if (HasSubI(tablePath, "SanctumPersistentEffects")) {
        // room-imposed affliction display name (row Name field)
        if (!rowName.empty()) room.affliction = rowName;
    } else if (HasSubI(tablePath, "SanctumRooms")) {
        if (rowId.empty()) return;
        if (outFloorTileset && outFloorTileset->empty()) {
            auto parts = SplitOn(rowId, '_');
            if (parts.size() >= 2 && !parts[0].empty()) *outFloorTileset = parts[0];
        }
        if (HasSubI(rowId, "Treasure")) {
            std::string rw = MapReward(rowId);
            if (!rw.empty()) room.reward = rw;
        } else {
            std::string t = ExtractRoomType(rowId);
            if (!t.empty()) room.roomType = t;
        }
    }
    // tables other than these two are ignored.
}

} // namespace sekhema
