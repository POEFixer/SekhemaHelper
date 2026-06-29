#include "LargeMapMarkers.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace sekhema {

static void Marker(ImDrawList* dl, ImVec2 p, ImU32 col, float r, const char* label, bool ring) {
    dl->AddCircleFilled(p, r, col);
    dl->AddCircle(p, r, IM_COL32(0, 0, 0, 180), 0, 1.5f);
    if (ring) dl->AddCircle(p, r + 3.0f, IM_COL32(255, 255, 255, 230), 0, 2.0f);
    if (label && label[0]) {
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImVec2 tp(p.x - ts.x * 0.5f, p.y + r + 1.0f);
        dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 200), label);
        dl->AddText(tp, IM_COL32(255, 255, 255, 235), label);
    }
}

// Draw all markers projected onto one map (large or mini).
static void DrawOn(ImDrawList* dl, bool large, const TrialEntities& ents, const CrystalRoute& route,
                   const Settings& s, const SekhemaResources& res, const PluginSDK::Context* ctx) {
    const float pr = large ? s.poiRadius : s.poiRadius * 0.6f;
    const bool labels = large;

    auto projG = [&](int gx, int gy, ImVec2& out) -> bool {
        float wz = ctx->Terrain.GetTerrainHeight(gx, gy);
        float sx = 0, sy = 0;
        bool ok = large ? ctx->Render.GridToLargeMap((float)gx, (float)gy, wz, sx, sy)
                        : ctx->Render.GridToMiniMap((float)gx, (float)gy, wz, sx, sy);
        if (!ok) return false; out = ImVec2(sx, sy); return true;
    };
    auto projM = [&](const TrialMarker& m, ImVec2& out) -> bool {
        float sx = 0, sy = 0;
        bool ok = large ? ctx->Render.GridToLargeMap(m.gridX, m.gridY, m.worldZ, sx, sy)
                        : ctx->Render.GridToMiniMap(m.gridX, m.gridY, m.worldZ, sx, sy);
        if (!ok) return false; out = ImVec2(sx, sy); return true;
    };

    // Crystals: A* route polyline (decimated) + numbered stops.
    if (s.showCrystals && !route.stops.empty()) {
        ImU32 col = ImGui::ColorConvertFloat4ToU32(s.crystalColor);
        // Draw EVERY polyline point — when A* falls back to straight legs the polyline
        // is just [player, stop0, stop1, ...], so skipping points would drop crystals
        // from the line (it would connect only every other one).
        ImVec2 prevp; bool haveprev = false;
        for (size_t i = 0; i < route.polyline.size(); ++i) {
            ImVec2 sp;
            if (projG(route.polyline[i].x, route.polyline[i].y, sp)) {
                if (haveprev) dl->AddLine(prevp, sp, col, 2.0f);
                prevp = sp; haveprev = true;
            } else {
                haveprev = false;
            }
        }
        for (size_t i = 0; i < route.stops.size(); ++i) {
            ImVec2 sp;
            if (projG(route.stops[i].x, route.stops[i].y, sp)) {
                char num[8]; std::snprintf(num, sizeof(num), "%zu", i + 1);
                Marker(dl, sp, col, pr, num, false);
            }
        }
    }

    // Chests: per tier, prioritise by content order, highlight top-key-budget.
    if (s.showChests && !ents.chests.empty()) {
        ImU32 col = ImGui::ColorConvertFloat4ToU32(s.chestColor);
        int keys[4] = { 0, res.keysBronze, res.keysSilver, res.keysGold };
        auto rank = [&](const std::string& content) -> int {
            for (size_t i = 0; i < s.chestOrder.size(); ++i)
                if (s.chestOrder[i].second && !s.chestOrder[i].first.empty()
                    && content.find(s.chestOrder[i].first) != std::string::npos)
                    return static_cast<int>(i);
            return 1000;
        };
        for (int tier = 1; tier <= 3; ++tier) {
            std::vector<const TrialMarker*> tc;
            for (const auto& c : ents.chests) if (c.chestTier == tier) tc.push_back(&c);
            std::sort(tc.begin(), tc.end(), [&](const TrialMarker* a, const TrialMarker* b){
                return rank(a->label) < rank(b->label); });
            int budget = keys[tier];
            for (size_t i = 0; i < tc.size(); ++i) {
                ImVec2 sp;
                if (!projM(*tc[i], sp)) continue;
                bool top = (static_cast<int>(i) < budget) && rank(tc[i]->label) < 1000;
                Marker(dl, sp, col, pr, labels ? tc[i]->label.c_str() : nullptr, top);
            }
        }
    }

    if (s.showPortals)
        for (const auto& m : ents.portals) {
            ImVec2 sp;
            if (projM(m, sp)) Marker(dl, sp, ImGui::ColorConvertFloat4ToU32(s.portalColor),
                                     pr, labels ? "Portal" : nullptr, false);
        }
    if (s.showLevers)
        for (const auto& m : ents.levers) {
            ImVec2 sp;
            if (projM(m, sp)) Marker(dl, sp, ImGui::ColorConvertFloat4ToU32(s.leverColor),
                                     pr, labels ? "Lever" : nullptr, false);
        }
}

void DrawLargeMapMarkers(const TrialEntities& ents, const CrystalRoute& route,
                         const Settings& s, const SekhemaResources& res,
                         const PluginSDK::Context* ctx) {
    if (!ctx) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    if (ctx->Render.GetLargeMapTransform().IsVisible) DrawOn(dl, true,  ents, route, s, res, ctx);
    if (ctx->Render.GetMiniMapTransform().IsVisible)  DrawOn(dl, false, ents, route, s, res, ctx);
}

} // namespace sekhema
