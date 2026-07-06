#include "DashboardUI.h"
#include "Theme.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace sekhema {

static void DrawResources(const Theme& th, const SekhemaResources& res) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Honour bar
    ImGui::TextUnformatted("HONOUR");
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetTextLineHeight() * 1.1f;
    float pct = res.valid ? std::clamp(res.honourPct, 0.0f, 100.0f) : 0.0f;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), th.surfaceAlt, 4.0f);
    dl->AddRectFilled(p, ImVec2(p.x + w * (pct / 100.0f), p.y + h), HonourColor(pct), 4.0f);
    char buf[64];
    if (res.honourMax > 0)
        std::snprintf(buf, sizeof(buf), "%d / %d   %.0f%%", res.honourCurrent, res.honourMax, pct);
    else
        std::snprintf(buf, sizeof(buf), "%.0f%%", pct);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f), th.text, buf);
    ImGui::Dummy(ImVec2(w, h));
    // Water + keys
    ImGui::Text("Water  %d", res.sacredWater);
    ImGui::SameLine(0, 24);
    ImGui::Text("Keys  %d / %d / %d", res.keysBronze, res.keysSilver, res.keysGold);
    ImGui::Separator();
}

static void DrawChoiceRow(const Theme& th, const SekhemaRoom& room, bool best, float displayScore) {
    ImGui::PushID(&room);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = best ? 6.0f : 2.0f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    // reward (+value) detail line
    std::string detail;
    if (!room.reward.empty()) {
        detail = room.reward;
        if (room.rewardValue) { char b[24]; std::snprintf(b, sizeof(b), " +%d", room.rewardValue); detail += b; }
    }

    // Content on channel 1; the highlight background (channel 0) is drawn behind
    // the MEASURED content with symmetric top/bottom padding.
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    ImGui::SetCursorScreenPos(ImVec2(p0.x + pad + 2.0f, p0.y + pad));
    ImGui::BeginGroup();
    {
        ImVec2 titlePos = ImGui::GetCursorScreenPos();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(best ? th.accent : th.text),
                           "%s%s", best ? ">> " : "",
                           room.roomType.empty() ? "Room" : room.roomType.c_str());
        // score, right-aligned (manual draw so it doesn't affect the group bounds)
        char sc[16]; std::snprintf(sc, sizeof(sc), "%.1f", displayScore);
        float scW = ImGui::CalcTextSize(sc).x;
        dl->AddText(ImVec2(p0.x + w - pad - 4.0f - scW, titlePos.y),
                    best ? th.accent : th.textDim, sc);
        const ImU32 detailCol = best ? th.text : th.textDim;
        if (!detail.empty())
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(detailCol), "%s", detail.c_str());
    }
    ImGui::EndGroup();
    float contentBottom = ImGui::GetItemRectMax().y;

    dl->ChannelsSetCurrent(0);
    if (best) {
        ImVec2 r1(p0.x + w, contentBottom + pad);
        dl->AddRectFilled(p0, r1, th.accentDim, 6.0f);
        dl->AddRect(p0, r1, th.accent, 6.0f, 0, 2.0f);
    }
    dl->ChannelsMerge();

    // Advance: best card keeps its bottom pad + a gap; plain rows sit closer.
    float adv = best ? (contentBottom + pad + 5.0f) : (contentBottom + 2.0f);
    ImGui::SetCursorScreenPos(ImVec2(p0.x, adv));
    ImGui::PopID();
}

static void DrawRoute(const Theme& th, const SekhemaFloor& floor) {
    ImGui::TextUnformatted("ROUTE TO BOSS");
    std::string strip;
    int remaining = 0;
    for (size_t L = 0; L < floor.layers.size(); ++L) {
        bool isPlayer = ((int)L == floor.playerLayer);
        const SekhemaRoom* onPath = nullptr;
        for (const auto& r : floor.layers[L]) if (r.onBestPath) { onPath = &r; break; }
        if (!onPath) continue;
        const char* glyph = isPlayer ? "[#]" : (onPath->roomType == "Boss" ? "*" : "[ ]");
        if (!strip.empty()) strip += " > ";
        strip += glyph;
        if ((int)L > floor.playerLayer) ++remaining;
    }
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(th.text), "%s", strip.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(th.textDim), "  %d rooms left", remaining);
}

void DrawDashboard(const SekhemaFloor& floor, const SekhemaResources& res, Settings& settings) {
    if (!settings.dashboardVisible) return;
    Theme th = MakeTheme();

    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(settings.dashboardPos, ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    bool open = true;
    if (ImGui::Begin("SekhemaHelper##dash", &open, flags)) {
        settings.dashboardPos = ImGui::GetWindowPos();   // persist drag
        if (res.valid) DrawResources(th, res);

        if (!floor.structurePresent) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(th.textDim), "Trial not detected");
        } else if (!floor.valid) {
            // Trial floor detected, but its rooms are hidden on the Trial Map
            // (relic "The Burden of Leadership"). Resources (above) still work;
            // room identities reveal as you enter them.
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(th.textDim), "Rooms hidden on the Trial Map");
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(th.textDim), "(rooms reveal as you enter them)");
        } else {
            ImGui::TextUnformatted("NEXT ROOM - take the highlighted");
            auto choices = floor.Choices();
            // Score each choice by its path value to the boss (so the highlighted
            // best-path room is the highest number), normalized among the choices.
            double lo = 1e300, hi = -1e300;
            for (auto* c : choices) {
                if (c->pathScore < lo) lo = c->pathScore;
                if (c->pathScore > hi) hi = c->pathScore;
            }
            double span = hi - lo;
            std::sort(choices.begin(), choices.end(),
                      [](const SekhemaRoom* a, const SekhemaRoom* b){
                          if (a->onBestPath != b->onBestPath) return a->onBestPath;
                          return a->pathScore > b->pathScore; });
            for (auto* c : choices) {
                float ds = (span > 1e-9) ? static_cast<float>(10.0 * (c->pathScore - lo) / span) : 10.0f;
                DrawChoiceRow(th, *c, c->onBestPath, ds);
            }

            ImGui::Separator();
            DrawRoute(th, floor);
        }
    }
    ImGui::End();
    if (!open) settings.dashboardVisible = false;        // window close button hides it
}

} // namespace sekhema
