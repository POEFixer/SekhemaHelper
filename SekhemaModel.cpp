#include "SekhemaModel.h"
#include "MemReader.h"
#include "MemoryLayout.h"
#include "RoomClassifier.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace sekhema {
using namespace layout;

// Try one floorObj: pick the FloorData base by the flag, fall back to the other
// base; accept iff the Layers vector yields a layer count in [1, MaxLayers].
static bool TryResolveBase(const Mem& mem, uintptr_t floorObj,
                           uintptr_t& floorData, int& layerCount) {
    if (!floorObj) return false;
    uint8_t flag = mem.Read<uint8_t>(floorObj + FloorObj_Flag);
    floorData = floorObj + (flag != 0 ? FloorData_OffActive : FloorData_OffAlt);
    layerCount = VecCount(mem.ReadVec(floorData + FloorData_Layers), LayerStride);
    if (layerCount > 0 && layerCount <= MaxLayers) return true;
    floorData = floorObj + (flag != 0 ? FloorData_OffAlt : FloorData_OffActive);
    layerCount = VecCount(mem.ReadVec(floorData + FloorData_Layers), LayerStride);
    return layerCount > 0 && layerCount <= MaxLayers;
}

// Resolve FloorData from the panel (panel+0x3B8 -> floorObj), retrying from the
// panel's UI parent (panel+0xB8) if the direct read doesn't validate.
static bool ResolveFloor(const Mem& mem, uintptr_t panelAddr,
                         uintptr_t& floorData, int& layerCount) {
    if (!panelAddr) return false;
    if (TryResolveBase(mem, mem.Ptr(panelAddr + MapElement_FloorObjPtr), floorData, layerCount))
        return true;
    uintptr_t parent = mem.Ptr(panelAddr + UiElement_ParentPtr);
    if (parent && TryResolveBase(mem, mem.Ptr(parent + MapElement_FloorObjPtr),
                                 floorData, layerCount))
        return true;
    return false;
}

SekhemaFloor SekhemaReader::Read(uintptr_t panelAddr, const PluginSDK::Context* ctx) {
    SekhemaFloor floor;
    Mem mem(ctx);
    if (!mem.Valid()) return floor;

    uintptr_t floorData = 0; int layerCount = 0;
    if (!ResolveFloor(mem, panelAddr, floorData, layerCount)) return floor;

    // 1) Build the layered room graph (inline structs).
    StdVec layersVec = mem.ReadVec(floorData + FloorData_Layers);
    floor.layers.resize(static_cast<size_t>(layerCount));
    for (int li = 0; li < layerCount; ++li) {
        uintptr_t layerAddr = layersVec.First + static_cast<uintptr_t>(li) * LayerStride;
        StdVec roomsVec = mem.ReadVec(layerAddr + Layer_Rooms);
        int roomCount = VecCount(roomsVec, RoomStride);
        if (roomCount > MaxRooms) roomCount = MaxRooms;
        for (int ri = 0; ri < roomCount; ++ri) {
            uintptr_t roomAddr = roomsVec.First + static_cast<uintptr_t>(ri) * RoomStride;
            SekhemaRoom room;
            // connections: std::vector<uint8_t> First/Last -> next-layer indices.
            uintptr_t cf = mem.Ptr(roomAddr + Room_ConnFirst);
            uintptr_t cl = mem.Ptr(roomAddr + Room_ConnLast);
            long long cnt = static_cast<long long>(cl) - static_cast<long long>(cf);
            if (cf && cnt > 0 && cnt <= MaxConn) {
                std::vector<uint8_t> bytes = mem.ReadBytes(cf, static_cast<int>(cnt));
                for (uint8_t b : bytes) room.connections.push_back(static_cast<int>(b));
            }
            floor.layers[li].push_back(std::move(room));
        }
    }

    // 2) Choices + Counter -> player position + chosen marks.
    uint8_t counter = mem.Read<uint8_t>(floorData + FloorData_Counter);
    int choicesMade = counter & 7;
    for (int li = 0; li < layerCount; ++li) {
        uint8_t ch = mem.Read<uint8_t>(floorData + FloorData_Choices + li);
        if (ch != 0xFF && ch < static_cast<int>(floor.layers[li].size()))
            floor.layers[li][ch].isChosen = true;
    }
    floor.playerLayer = -1;
    floor.playerRoom  = -1;
    if (choicesMade > 0 && (choicesMade - 1) < layerCount) {
        floor.playerLayer = choicesMade - 1;
        uint8_t ch = mem.Read<uint8_t>(floorData + FloorData_Choices + floor.playerLayer);
        floor.playerRoom = (ch == 0xFF) ? -1 : static_cast<int>(ch);
    }

    // 3) Content vector -> classify each (layer, room).
    StdVec contentVec = mem.ReadVec(floorData + FloorData_Content);
    int contentCount = VecCount(contentVec, Content_Stride);
    if (contentCount > MaxContent) contentCount = MaxContent;
    for (int i = 0; i < contentCount; ++i) {
        uintptr_t e = contentVec.First + static_cast<uintptr_t>(i) * Content_Stride;
        int layer = mem.Read<uint8_t>(e + Content_Layer);
        int idx   = mem.Read<uint8_t>(e + Content_RoomIdx);
        if (layer < 0 || layer >= static_cast<int>(floor.layers.size())) continue;
        if (idx < 0 || idx >= static_cast<int>(floor.layers[layer].size())) continue;
        SekhemaRoom& room = floor.layers[layer][idx];
        for (int k = 0; k < Content_FkCount; ++k) {
            uintptr_t rowPtr   = mem.Ptr(e + Content_FkRow0   + k * Content_FkStride);
            uintptr_t tablePtr = mem.Ptr(e + Content_FkTable0 + k * Content_FkStride);
            if (rowPtr && tablePtr) ClassifyFk(room, rowPtr, tablePtr, mem, &floor.floorTileset);
        }
    }

    // A real Sekhema floor always has classified content (SanctumRooms entries).
    // Requiring >=1 classified room rejects coincidental +0x3B8 false positives
    // that happen to resolve a [1,64] layer vector but aren't a trial floor.
    int classified = 0;
    for (const auto& layer : floor.layers)
        for (const auto& room : layer)
            if (!room.roomType.empty() || !room.affliction.empty() || !room.reward.empty())
                ++classified;

    floor.valid = (classified > 0);
    return floor;
}

} // namespace sekhema
