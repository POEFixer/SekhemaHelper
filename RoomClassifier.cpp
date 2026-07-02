#include "RoomClassifier.h"
#include "MemReader.h"
#include "MemoryLayout.h"

namespace sekhema {

void ClassifyFk(SekhemaRoom& room, uintptr_t rowPtr, uintptr_t tablePtr, const Mem& mem,
                std::string* outFloorTileset) {
    if (!rowPtr || !tablePtr) return;

    // table path string: table+0x08 -> ptr -> raw UTF-16 (max 96 chars).
    std::string tpath = mem.ReadWide(mem.Ptr(tablePtr + layout::DatTable_PathPtr), 96);
    if (tpath.empty()) return;

    if (HasSubI(tpath, "SanctumPersistentEffects")) {
        // room-imposed affliction display name @ row+0x28
        std::string name = mem.ReadWide(mem.Ptr(rowPtr + layout::DatRow_NamePtr), 48);
        if (!name.empty()) room.affliction = name;
    } else if (HasSubI(tpath, "SanctumRooms")) {
        // room id @ row+0x00
        std::string id = mem.ReadWide(mem.Ptr(rowPtr + layout::DatRow_IdPtr), 64);
        if (id.empty()) return;
        if (outFloorTileset && outFloorTileset->empty()) {
            auto parts = SplitOn(id, '_');
            if (parts.size() >= 2 && !parts[0].empty()) *outFloorTileset = parts[0];
        }
        if (HasSubI(id, "Treasure")) {
            std::string rw = MapReward(id);
            if (!rw.empty()) room.reward = rw;
        } else {
            std::string t = ExtractRoomType(id);
            if (!t.empty()) room.roomType = t;
        }
    }
    // tables other than these two are ignored.
}

} // namespace sekhema
