#pragma once
#include "Model.h"
#include <imgui.h>

namespace sekhema {

struct Theme {
    ImU32 accent, accentDim, surface, surfaceAlt, border, text, textDim;
    ImU32 riskNone, riskMinor, riskModerate, riskSevere, good;
};

inline ImU32 MixU32(const ImVec4& a, const ImVec4& b, float t) {
    ImVec4 c(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
    return ImGui::ColorConvertFloat4ToU32(c);
}

// Build the palette from the active host theme each frame so the plugin
// tracks whichever of the host's 8 Fluent themes is selected.
inline Theme MakeTheme() {
    const ImGuiStyle& s = ImGui::GetStyle();
    auto col = [&](ImGuiCol id){ return s.Colors[id]; };
    Theme t;
    t.accent      = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_HeaderActive));
    t.accentDim   = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_Header));
    t.surface     = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_WindowBg));
    t.surfaceAlt  = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_ChildBg).w > 0.01f
                        ? col(ImGuiCol_ChildBg) : col(ImGuiCol_FrameBg));
    t.border      = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_Border));
    t.text        = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_Text));
    t.textDim     = ImGui::ColorConvertFloat4ToU32(col(ImGuiCol_TextDisabled));
    // semantic triad — fixed hues, full alpha (legible on any theme bg)
    t.riskNone     = IM_COL32(120, 200, 120, 255);
    t.riskMinor    = IM_COL32(170, 210, 110, 255);
    t.riskModerate = IM_COL32(235, 180,  70, 255);
    t.riskSevere   = IM_COL32(225,  85,  85, 255);
    t.good         = IM_COL32(120, 200, 120, 255);
    return t;
}

inline ImU32 RiskColor(const Theme& t, Risk r) {
    switch (r) {
        case Risk::Severe:   return t.riskSevere;
        case Risk::Moderate: return t.riskModerate;
        case Risk::Minor:    return t.riskMinor;
        default:             return t.riskNone;
    }
}

// Honour bar color: >60% green, 30-60% amber, <30% red (lerped).
inline ImU32 HonourColor(float pct) {
    ImVec4 green(0.47f,0.78f,0.47f,1.f), amber(0.92f,0.70f,0.27f,1.f), red(0.88f,0.33f,0.33f,1.f);
    if (pct >= 60.0f) return ImGui::ColorConvertFloat4ToU32(green);
    if (pct >= 30.0f) return MixU32(red, amber, (pct - 30.0f) / 30.0f);
    return MixU32(red, amber, 0.0f);
}

} // namespace sekhema
