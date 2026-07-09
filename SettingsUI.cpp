#include "SettingsUI.h"
#include "AfflictionCatalog.h"
#include "WeightCalculator.h"
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

// Rich tooltip for one affliction: game icon + Major/Minor category + the
// curse description from SanctumPersistentEffects.
static void AfflictionTooltip(const AfflictionInfo& info, AfflictionIcons* icons) {
    ImGui::BeginTooltip();
    ImTextureID tex = icons ? icons->Get(info.iconFile) : ImTextureID{};
    if (tex) {
        ImGui::Image(tex, ImVec2(48.0f, 48.0f));
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::TextUnformatted(info.name);
    if (info.major)
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Major Affliction");
    else
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "Minor Affliction");
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 320.0f);
    ImGui::TextUnformatted(info.desc);
    if (IsBuildAwareAffliction(info.name)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f),
                           "Weight is computed from your defences (build-aware).");
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// The Afflictions column: weight drag + game curse icon + name per row, with
// the rich tooltip on the icon/name. Falls back to text-only when an icon is
// missing (resources/afflictions not deployed).
static void DrawAfflictionColumn(const char* title, std::map<std::string,float>& dict,
                                 AfflictionIcons* icons, float width, float height) {
    ImGui::BeginChild(title, ImVec2(width, height), true);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::PushID(title);
    const float iconSz = ImGui::GetFrameHeight();
    for (auto& kv : dict) {
        if (!Contains(kv.first, s_search)) continue;
        const AfflictionInfo* info = FindAffliction(kv.first.c_str());
        ImGui::PushID(kv.first.c_str());
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("##w", &kv.second, 25.0f, -1000000.0f, 1000000.0f, "%.0f");
        ImGui::SameLine();
        ImTextureID tex = (icons && info) ? icons->Get(info->iconFile) : ImTextureID{};
        if (tex) ImGui::Image(tex, ImVec2(iconSz, iconSz));
        else     ImGui::Dummy(ImVec2(iconSz, iconSz));
        bool hovered = ImGui::IsItemHovered();
        ImGui::SameLine(0.0f, 5.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(kv.first.c_str());
        hovered = hovered || ImGui::IsItemHovered();
        if (hovered) {
            if (info) AfflictionTooltip(*info, icons);
            else      ImGui::SetTooltip("%s", kv.first.c_str());
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndChild();
}

static void DrawProfilesTab(Settings& s, AfflictionIcons* icons) {
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
            if (def.name == prof->name) { *prof = def; MergeProfileDefaults(*prof); break; }
    }
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##search", "Search afflictions...", s_search, sizeof(s_search));
    // Three side-by-side columns: Afflictions | Room types | Rewards.
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float colW = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
    const float colH = 320.0f;
    DrawAfflictionColumn("Afflictions", prof->afflictionWeights, icons, colW, colH);
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
    ImGui::SeparatorText("Timers");
    ImGui::Checkbox("Timer overlay (run / floor / room)", &s.timerOverlayEnabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag the overlay window in-trial to reposition it.");
    ImGui::Checkbox("Show floor line", &s.timerShowFloorLine);
    ImGui::Checkbox("Show room line", &s.timerShowRoomLine);
    ImGui::Checkbox("Debug: log tracker events", &s.timerDebugLog);
}

static void DrawOverlaysTab(Settings& s) {
    // POI row: portals / levers / crystals inline with their colors.
    ImGui::Checkbox("Portals", &s.showPortals); ImGui::SameLine();
    ImGui::ColorEdit4("##pc", &s.portalColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::Checkbox("Levers", &s.showLevers); ImGui::SameLine();
    ImGui::ColorEdit4("##lc", &s.leverColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::Checkbox("Crystals (Escape)", &s.showCrystals); ImGui::SameLine();
    ImGui::ColorEdit4("##cc", &s.crystalColor.x, ImGuiColorEditFlags_NoInputs);

    ImGui::SetNextItemWidth(150);
    ImGui::SliderFloat("POI size", &s.poiRadius, 4.0f, 20.0f, "%.0f");
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextUnformatted("Room bounds: automatic (walls + closed doors)");
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Chests: master toggles + per-type table below.
    ImGui::Checkbox("Chests", &s.showChests);
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::Checkbox("Labels", &s.showChestLabels);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Short type name under each circle, colored by cache tier\n"
                          "(Bronze / Silver / Gold). \"+\" = Superior, \"++\" = Prime.");
    ImGui::SameLine(0.0f, 18.0f);
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("Circle size", &s.chestRadius, 2.0f, 16.0f, "%.0f");

    ImGui::TextDisabled("Ring = white ring on top of every chest of the checked types.");

    if (ImGui::SmallButton("Show all")) for (auto& t : s.chestTypes) t.show = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Hide all")) for (auto& t : s.chestTypes) t.show = false;
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) s.chestTypes = DefaultChestTypes();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restore default colors and flags.");

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));
    if (ImGui::BeginTable("chest_types", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
            ImVec2(0.0f, 320.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("On",    ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Cache", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Ring",  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < s.chestTypes.size(); ++i) {
            auto& t = s.chestTypes[i];
            const ChestTypeInfo* info = FindChestTypeInfo(t.id);
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Checkbox("##show", &t.show);
            ImGui::TableNextColumn(); ImGui::ColorEdit4("##col", &t.color.x,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(info ? info->uiName : t.id.c_str());
            ImGui::TableNextColumn(); ImGui::Checkbox("##hl", &t.highlight);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
}

void DrawSettingsPanel(Settings& s, RunDatabase* db, HistoryUIState* hist,
                       AfflictionIcons* icons) {
    if (ImGui::BeginTabBar("sekhema_settings")) {
        if (ImGui::BeginTabItem("Display"))  { DrawDisplayTab(s);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Profiles")) { DrawProfilesTab(s, icons); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Overlays")) { DrawOverlaysTab(s); ImGui::EndTabItem(); }
        if (db && hist && ImGui::BeginTabItem("History")) { DrawHistoryTab(*db, *hist); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

} // namespace sekhema
