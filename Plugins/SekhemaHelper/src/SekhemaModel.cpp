#include "SekhemaModel.h"
#include "RoomClassifier.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace sekhema {

SekhemaFloor SekhemaReader::Read(uintptr_t panelAddr, const PluginSDK::Context* ctx) {
    SekhemaFloor floor;
    if (!ctx || !panelAddr) return floor;

    // Floor header via the host Sekhema service: FloorData resolution (direct
    // panel pointer -> UI-parent fallback) plus the per-run dynamic state
    // (choices/counter), captured atomically in one call. {valid=false} covers
    // off-Trial nodes AND transient mid-free ticks that used to yield a torn
    // skeleton — the caller's structural debounce absorbs the latter.
    const PluginSDK::SekhemaFloor hdr = ctx->Sekhema.GetFloor(panelAddr);
    if (!hdr.valid) return floor;
    // roomCounts/choices carry exactly one entry per host-validated layer.
    const int layerCount = static_cast<int>(hdr.roomCounts.size());
    if (layerCount <= 0) return floor;

    // 1) Build the layered room graph (immutable per floor, so the separate
    //    Rooms() call cannot tear against the header).
    floor.layers.resize(static_cast<size_t>(layerCount));
    for (const PluginSDK::SekhemaRoom& r : ctx->Sekhema.Rooms(panelAddr)) {
        if (r.layer < 0 || r.layer >= layerCount) continue;
        SekhemaRoom room;
        room.connections = r.connections;   // next-layer indices
        floor.layers[static_cast<size_t>(r.layer)].push_back(std::move(room));
    }

    // 2) Choices + Counter -> player position + chosen marks.
    const int choicesMade = hdr.counter & 7;
    for (int li = 0; li < layerCount; ++li) {
        uint8_t ch = hdr.choices[static_cast<size_t>(li)];
        if (ch != 0xFF && ch < static_cast<int>(floor.layers[li].size()))
            floor.layers[li][ch].isChosen = true;
    }
    floor.playerLayer = -1;
    floor.playerRoom  = -1;
    if (choicesMade > 0 && (choicesMade - 1) < layerCount) {
        floor.playerLayer = choicesMade - 1;
        uint8_t ch = hdr.choices[static_cast<size_t>(floor.playerLayer)];
        floor.playerRoom = (ch == 0xFF) ? -1 : static_cast<int>(ch);
    }

    // 3) Content entries -> classify each (layer, room). Target indices are
    //    delivered as read — bounds-check against the graph before joining.
    for (const PluginSDK::SekhemaContentEntry& e : ctx->Sekhema.Content(panelAddr)) {
        if (e.layer < 0 || e.layer >= static_cast<int>(floor.layers.size())) continue;
        if (e.roomIndex < 0 ||
            e.roomIndex >= static_cast<int>(floor.layers[e.layer].size())) continue;
        SekhemaRoom& room = floor.layers[e.layer][e.roomIndex];
        for (const PluginSDK::SekhemaContentFk& fk : e.fks)
            ClassifyFk(room, fk.tablePath, fk.rowId, fk.rowName, &floor.floorTileset);
    }

    // A real Sekhema floor always has classified content (SanctumRooms entries).
    // Requiring >=1 classified room rejects coincidental floor-pointer false
    // positives that happen to resolve a [1,64] layer vector but aren't a trial
    // floor.
    int classified = 0;
    for (const auto& layer : floor.layers)
        for (const auto& room : layer)
            if (!room.roomType.empty() || !room.affliction.empty() || !room.reward.empty())
                ++classified;

    // structurePresent: the trial-map graph resolved (>=2 layers: entrance -> ...
    // -> boss). Kept distinct from `valid` so a floor whose room CONTENTS are
    // hidden (relic "The Burden of Leadership": "Rooms are unknown on the Trial
    // Map") is still recognized as a live trial — the plugin renders the
    // topology/resources/route and fills room identities in as they get revealed,
    // instead of concluding "no trial here" and permanently standing down for the
    // whole floor (a floor is one zone). `valid` still gates room-content advice.
    floor.structurePresent = (layerCount >= 2);
    floor.classifiedRooms  = classified;
    floor.valid            = (classified > 0);
    return floor;
}

} // namespace sekhema
