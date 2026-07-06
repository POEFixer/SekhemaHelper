#include "MapOverlay.h"
#include "Theme.h"
#include <imgui.h>

namespace sekhema {

void DrawMapOverlay(const SekhemaFloor& floor, const Settings& s,
                    const PluginSDK::Context* ctx, uintptr_t panel) {
    // structurePresent (not valid): with a rooms-hidden trial the graph exists but
    // no room is classified yet, so risk dots / best-path frames simply have
    // nothing to draw until rooms reveal — the walk stays a no-op meanwhile.
    if (!ctx || !panel || !floor.structurePresent) return;

    // Only draw when the floor-map panel is actually open. CE-verified: the panel's
    // own visibility bit clears when closed, while the room widgets keep theirs set
    // (so ComputeScreenRect would otherwise still place frames over the terrain).
    if (!ctx->Ui.IsVisible(panel)) return;

    // CE-verified: layers container = panel -> [0,0,1]; its child[layer].child[room]
    // is each on-screen room widget.
    static const int kLayersPath[] = {0, 0, 1};
    uintptr_t layersC = ctx->Ui.FollowPath(panel, kLayersPath, 3);
    if (!layersC) return;

    Theme th = MakeTheme();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    const ImU32 pathCol = ImGui::ColorConvertFloat4ToU32(s.bestPathColor);

    for (int li = 0; li < static_cast<int>(floor.layers.size()); ++li) {
        uintptr_t layerC = ctx->Ui.GetChildAt(layersC, li);
        if (!layerC) continue;
        const auto& rooms = floor.layers[li];
        for (int ri = 0; ri < static_cast<int>(rooms.size()); ++ri) {
            uintptr_t widget = ctx->Ui.GetChildAt(layerC, ri);
            if (!widget) continue;
            float x = 0, y = 0, w = 0, h = 0;
            if (!ctx->Ui.ComputeScreenRect(widget, x, y, w, h) || w <= 0.0f || h <= 0.0f)
                continue;

            const SekhemaRoom& room = rooms[ri];

            // Risk dot (top-left) for rooms that impose an affliction.
            if (!room.affliction.empty()) {
                ImVec2 c(x + 9.0f, y + 9.0f);
                dl->AddCircleFilled(c, 5.5f, RiskColor(th, room.risk));
                dl->AddCircle(c, 5.5f, IM_COL32(0, 0, 0, 170), 0, 1.5f);
            }

            // Best-path frame (skip the player's current room — that's where you are).
            bool isCurrent = (li == floor.playerLayer && ri == floor.playerRoom);
            if (s.drawBestPath && room.onBestPath && !isCurrent) {
                dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), pathCol, 8.0f, 0, s.frameThickness);
            }
        }
    }
}

} // namespace sekhema
