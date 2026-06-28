#include "SettingsUI.h"
#include <imgui.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

namespace sekhema {

static char s_search[64] = {};

// Human-readable name for a virtual-key code (e.g. 0x75 -> "F6").
static void VkName(int vk, char* buf, int bufsize) {
    if (vk <= 0) { if (bufsize) buf[0] = '\0'; return; }
    UINT sc = ::MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    switch (vk) {
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT: case VK_HOME: case VK_END:
        case VK_INSERT: case VK_DELETE: sc |= 0x100; break;
        default: break;
    }
    LONG lparam = static_cast<LONG>(sc << 16);
    wchar_t wname[32] = {};
    if (::GetKeyNameTextW(lparam, wname, 32) > 0 && wname[0])
        ::WideCharToMultiByte(CP_UTF8, 0, wname, -1, buf, bufsize, nullptr, nullptr);
    else
        std::snprintf(buf, bufsize, "VK 0x%02X", vk);
}

// Click-to-capture hotkey picker. Returns immediately; sets s.toggleVk on capture.
static void HotkeyPicker(Settings& s) {
    static bool capturing = false;
    ImGui::TextUnformatted("Toggle hotkey");
    ImGui::SameLine();
    if (capturing) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "press a key... (Esc cancels)");
        for (int vk = 0x08; vk <= 0xFE; ++vk) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
                vk == VK_XBUTTON1 || vk == VK_XBUTTON2) continue;
            if (::GetAsyncKeyState(vk) & 0x8000) {
                if (vk != VK_ESCAPE) s.toggleVk = vk;
                capturing = false;
                break;
            }
        }
    } else {
        char name[32]; VkName(s.toggleVk, name, sizeof(name));
        if (ImGui::Button(name[0] ? name : "(set)")) capturing = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click, then press the key to bind.");
    }
}

static bool Contains(const std::string& hay, const char* needle) {
    if (!needle || !needle[0]) return true;
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return h.find(n) != std::string::npos;
}

// One weight group as a bordered, scrollable column. `filter` applies the
// affliction search box; width/height size the child region.
static void DrawWeightColumn(const char* title, std::map<std::string,float>& dict,
                             bool filter, float width, float height) {
    ImGui::BeginChild(title, ImVec2(width, height), true);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::PushID(title);
    for (auto& kv : dict) {
        if (filter && !Contains(kv.first, s_search)) continue;
        ImGui::PushID(kv.first.c_str());
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("##w", &kv.second, 25.0f, -1000000.0f, 1000000.0f, "%.0f");
        ImGui::SameLine();
        ImGui::TextUnformatted(kv.first.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kv.first.c_str());
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndChild();
}

static void DrawProfilesTab(Settings& s) {
    if (ImGui::BeginCombo("Profile", s.activeProfileName.c_str())) {
        for (auto& p : s.profiles)
            if (ImGui::Selectable(p.name.c_str(), p.name == s.activeProfileName))
                s.activeProfileName = p.name;
        ImGui::EndCombo();
    }
    WeightProfile* prof = s.ActiveProfile();
    if (!prof) { ImGui::TextDisabled("No profile"); return; }
    ImGui::SameLine();
    if (ImGui::Button("Reset profile")) {
        for (auto& def : DefaultProfiles())
            if (def.name == prof->name) { *prof = def; break; }
    }
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##search", "Search afflictions...", s_search, sizeof(s_search));
    // Three side-by-side columns: Afflictions | Room types | Rewards.
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float colW = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
    const float colH = 320.0f;
    DrawWeightColumn("Afflictions", prof->afflictionWeights, true, colW, colH);
    ImGui::SameLine();
    DrawWeightColumn("Room types", prof->roomTypeWeights, false, colW, colH);
    ImGui::SameLine();
    DrawWeightColumn("Rewards", prof->rewardWeights, false, colW, colH);
    ImGui::Separator();
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("Avoid Merchant when Water <", &prof->avoidMerchantBelowWater,
                     1.0f, 0.0f, 1000.0f, "%.0f");
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("Avoid Honour shrine when Honour% >", &prof->avoidHonourAbovePct,
                     1.0f, 0.0f, 100.0f, "%.0f");
}

static void DrawDisplayTab(Settings& s) {
    ImGui::Checkbox("Draw best path", &s.drawBestPath);
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("Frame thickness", &s.frameThickness, 1.0f, 6.0f, "%.1f");
    ImGui::ColorEdit4("Best path color", &s.bestPathColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();
    ImGui::Checkbox("Show dashboard", &s.dashboardVisible);
    ImGui::Checkbox("Auto-show in Trial", &s.dashboardAutoShow);
    HotkeyPicker(s);
}

static void DrawOverlaysTab(Settings& s) {
    ImGui::Checkbox("Portals", &s.showPortals); ImGui::SameLine();
    ImGui::ColorEdit4("##pc", &s.portalColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::Checkbox("Levers", &s.showLevers); ImGui::SameLine();
    ImGui::ColorEdit4("##lc", &s.leverColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::Checkbox("Crystals (Escape)", &s.showCrystals); ImGui::SameLine();
    ImGui::ColorEdit4("##cc", &s.crystalColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::Checkbox("Chests", &s.showChests); ImGui::SameLine();
    ImGui::ColorEdit4("##hc", &s.chestColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("POI radius", &s.poiRadius, 4.0f, 20.0f, "%.0f");
    ImGui::SetNextItemWidth(180);
    ImGui::SliderFloat("Current-room radius", &s.roomRadius, 50.0f, 2000.0f, "%.0f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Only mark trial objects within this grid distance of the\n"
                          "player (all Sekhema floors share one map).");
    ImGui::Separator();
    ImGui::TextUnformatted("Chest content priority (top = best)");
    for (size_t i = 0; i < s.chestOrder.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::Checkbox("##en", &s.chestOrder[i].second);
        ImGui::SameLine(); ImGui::TextUnformatted(s.chestOrder[i].first.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::ArrowButton("up", ImGuiDir_Up) && i > 0)
            std::swap(s.chestOrder[i], s.chestOrder[i-1]);
        ImGui::SameLine();
        if (ImGui::ArrowButton("down", ImGuiDir_Down) && i + 1 < s.chestOrder.size())
            std::swap(s.chestOrder[i], s.chestOrder[i+1]);
        ImGui::PopID();
    }
}

void DrawSettingsPanel(Settings& s) {
    if (ImGui::BeginTabBar("sekhema_settings")) {
        if (ImGui::BeginTabItem("Display"))  { DrawDisplayTab(s);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Profiles")) { DrawProfilesTab(s); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Overlays")) { DrawOverlaysTab(s); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

} // namespace sekhema
