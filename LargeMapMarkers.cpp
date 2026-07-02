#include "LargeMapMarkers.h"
#include "ChestTypes.h"
#include <imgui.h>
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

// Chest label color follows the cache tier (rarity): Bronze / Silver / Gold;
// grey for the untiered pots/urns.
static ImU32 TierLabelColor(int tier) {
    switch (tier) {
        case 1:  return IM_COL32(226, 148, 92, 255);
        case 2:  return IM_COL32(218, 218, 226, 255);
        case 3:  return IM_COL32(255, 213, 65, 255);
        default: return IM_COL32(168, 168, 168, 255);
    }
}

// Draw all markers projected onto one map (large or mini).
static void DrawOn(ImDrawList* dl, bool large, float playerZ,
                   const TrialEntities& ents, const CrystalRoute& route,
                   const Settings& s, const PluginSDK::Context* ctx) {
    const float pr = large ? s.poiRadius : s.poiRadius * 0.6f;
    const bool labels = large;

    // GridToLargeMap/GridToMiniMap expect Z RELATIVE to the player — the host
    // radar projects with (entityZ - playerZ). Absolute heights made markers on
    // raised platforms drift away from their true map spot.
    auto projG = [&](int gx, int gy, ImVec2& out) -> bool {
        float dz = ctx->Terrain.GetTerrainHeight(gx, gy) - playerZ;
        float sx = 0, sy = 0;
        bool ok = large ? ctx->Render.GridToLargeMap((float)gx, (float)gy, dz, sx, sy)
                        : ctx->Render.GridToMiniMap((float)gx, (float)gy, dz, sx, sy);
        if (!ok) return false; out = ImVec2(sx, sy); return true;
    };
    auto projM = [&](const TrialMarker& m, ImVec2& out) -> bool {
        float dz = m.worldZ - playerZ;
        float sx = 0, sy = 0;
        bool ok = large ? ctx->Render.GridToLargeMap(m.gridX, m.gridY, dz, sx, sy)
                        : ctx->Render.GridToMiniMap(m.gridX, m.gridY, dz, sx, sy);
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

    // Chests: per-type colors/visibility, tier-colored labels, and the highlight
    // ring drawn LAST so it stays visible over tightly packed circles. Every
    // chest of a Ring-checked type gets the ring.
    if (s.showChests && !ents.chests.empty()) {
        const float cr = large ? s.chestRadius : s.chestRadius * 0.6f;

        auto typeOf = [&](const std::string& id) -> const ChestTypeSetting* {
            for (const auto& t : s.chestTypes) if (t.id == id) return &t;
            return nullptr;
        };

        struct Drawn { ImVec2 p; const TrialMarker* m; const ChestTypeSetting* t; bool ring; };
        std::vector<Drawn> drawn;
        drawn.reserve(ents.chests.size());

        for (const auto& c : ents.chests) {
            const ChestTypeSetting* t = typeOf(c.label);
            if (t && !t->show) continue;
            ImVec2 sp;
            if (!projM(c, sp)) continue;
            drawn.push_back({sp, &c, t, t && t->highlight});
        }

        // Pass 1: circles.
        for (const auto& d : drawn) {
            ImU32 col = d.t ? ImGui::ColorConvertFloat4ToU32(d.t->color)
                            : IM_COL32(190, 190, 190, 255);
            dl->AddCircleFilled(d.p, cr, col);
            dl->AddCircle(d.p, cr, IM_COL32(0, 0, 0, 180), 0, 1.2f);
        }
        // Pass 2: labels (large map only) — short type name, tier-colored,
        // "+" = Superior, "++" = Prime.
        if (labels && s.showChestLabels) {
            for (const auto& d : drawn) {
                const ChestTypeInfo* info = FindChestTypeInfo(d.m->label);
                const char* base = info ? info->mapLabel : d.m->label.c_str();
                char text[48];
                std::snprintf(text, sizeof(text), "%s%s", base,
                              d.m->quality == 2 ? "+" : d.m->quality == 3 ? "++" : "");
                ImVec2 ts = ImGui::CalcTextSize(text);
                ImVec2 tp(d.p.x - ts.x * 0.5f, d.p.y + cr + 1.0f);
                dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 210), text);
                dl->AddText(tp, TierLabelColor(d.m->chestTier), text);
            }
        }
        // Pass 3: highlight rings on top of circles AND labels.
        for (const auto& d : drawn)
            if (d.ring) dl->AddCircle(d.p, cr + 2.5f, IM_COL32(255, 255, 255, 235), 0, 2.2f);
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
                         const Settings& s, const PluginSDK::Context* ctx) {
    if (!ctx) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;
    const float playerZ = ctx->Entities.GetPlayer().TerrainHeight;
    if (ctx->Render.GetLargeMapTransform().IsVisible) DrawOn(dl, true,  playerZ, ents, route, s, ctx);
    if (ctx->Render.GetMiniMapTransform().IsVisible)  DrawOn(dl, false, playerZ, ents, route, s, ctx);
}

} // namespace sekhema
