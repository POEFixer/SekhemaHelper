#include "SettingsUI.h"
#include "AfflictionCatalog.h"
#include "WeightCalculator.h"
#include "Text.h"
#include <imgui.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sekhema {

static char s_search[64] = {};

// Puts the next item, `width` pixels wide, on the current line `gap` pixels after the
// last item when it still fits there; otherwise it starts the next line. A translated
// label is often wider than the English one, so a fixed SameLine would push the item
// past the window's right edge.
static void SameLineIfFits(float gap, float width) {
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + gap + width <= right) ImGui::SameLine(0.0f, gap);
}

// The width of a checkbox with its label.
static float CheckboxWidth(const char* label) {
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
}

// The width of a checkbox with its label followed by a colour button (ColorEdit4 with
// ImGuiColorEditFlags_NoInputs) on the same line.
static float CheckboxWithColorWidth(const char* label) {
    return CheckboxWidth(label) + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFrameHeight();
}

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
    if (::GetKeyNameTextW(lparam, wname, 32) > 0 && wname[0] &&
        ::WideCharToMultiByte(CP_UTF8, 0, wname, -1, buf, bufsize, nullptr, nullptr) > 0)
        return;
    std::snprintf(buf, bufsize, "VK 0x%02X", vk);
}

// Click-to-capture hotkey picker. Returns immediately; sets s.toggleVk on capture.
static void HotkeyPicker(Settings& s) {
    static bool capturing = false;
    ImGui::TextUnformatted(I18n::Tr("display_toggle_hotkey"));
    ImGui::SameLine();
    if (capturing) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", I18n::Tr("display_hotkey_press"));
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
        char name[128]; VkName(s.toggleVk, name, sizeof(name));
        // The button keeps the id "###toggle_hotkey" whatever key or language it shows.
        const std::string label = std::string(name[0] ? name : I18n::Tr("display_hotkey_unset")) + "###toggle_hotkey";
        if (ImGui::Button(label.c_str())) capturing = true;
        Tooltip(I18n::Tr("display_hotkey_tip"));
    }
}

// Case folding for the search box: ASCII and Cyrillic (the names are shown in the
// program language). Anything else is compared as it is.
static std::string FoldCase(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { out.push_back(static_cast<char>(std::tolower(c))); continue; }
        if (c == 0xD0 && i + 1 < text.size()) {
            const unsigned char d = static_cast<unsigned char>(text[i + 1]);
            if (d >= 0x90 && d <= 0x9F) {          // U+0410..U+041F -> U+0430..U+043F
                out.push_back(static_cast<char>(0xD0)); out.push_back(static_cast<char>(d + 0x20)); ++i; continue;
            }
            if (d >= 0xA0 && d <= 0xAF) {          // U+0420..U+042F -> U+0440..U+044F
                out.push_back(static_cast<char>(0xD1)); out.push_back(static_cast<char>(d - 0x20)); ++i; continue;
            }
            if (d >= 0x80 && d <= 0x8F) {          // U+0400..U+040F -> U+0450..U+045F
                out.push_back(static_cast<char>(0xD1)); out.push_back(static_cast<char>(d + 0x10)); ++i; continue;
            }
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

using WeightMap = std::map<std::string, float>;
using NameOf = std::string (*)(const std::string&);

// One drawn weight row: its text in the program language and its (English-keyed) entry.
struct WeightRow {
    std::string text;
    WeightMap::iterator entry;
};

// The rows of one weight group, sorted by their text; `filter` applies the search box
// (to the text and to the English name).
static std::vector<WeightRow> SortedRows(WeightMap& dict, NameOf nameOf, bool filter) {
    const std::string needle = filter ? FoldCase(s_search) : std::string();
    std::vector<WeightRow> rows;
    rows.reserve(dict.size());
    for (auto it = dict.begin(); it != dict.end(); ++it) {
        std::string text = nameOf(it->first);
        if (!needle.empty() && FoldCase(text).find(needle) == std::string::npos &&
            FoldCase(it->first).find(needle) == std::string::npos)
            continue;
        rows.push_back({ std::move(text), it });
    }
    std::sort(rows.begin(), rows.end(), [](const WeightRow& a, const WeightRow& b) { return a.text < b.text; });
    return rows;
}

// One weight group as a bordered, scrollable column: `id` is its ImGui id, `title` its
// heading; width/height size the child region.
static void DrawWeightColumn(const char* id, const char* title, WeightMap& dict, NameOf nameOf,
                             float width, float height) {
    ImGui::BeginChild(id, ImVec2(width, height), true);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::PushID(id);
    for (auto& row : SortedRows(dict, nameOf, false)) {
        ImGui::PushID(row.entry->first.c_str());
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
        ImGui::DragFloat("##w", &row.entry->second, 25.0f, -1000000.0f, 1000000.0f, "%.0f");
        ImGui::SameLine();
        ImGui::TextUnformatted(row.text.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", row.text.c_str());
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
    ImGui::TextUnformatted(AfflictionName(info.name).c_str());
    if (info.major)
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", I18n::Tr("tooltip_major_affliction"));
    else
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "%s", I18n::Tr("tooltip_minor_affliction"));
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetFontSize() * 24.0f);
    ImGui::TextUnformatted(AfflictionDescription(info).c_str());
    if (IsBuildAwareAffliction(info.name)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "%s", I18n::Tr("tooltip_build_aware"));
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// The Afflictions column: weight drag + game curse icon + name per row, with
// the rich tooltip on the icon/name. Falls back to text-only when an icon is
// missing (resources/afflictions not deployed).
static void DrawAfflictionColumn(const char* id, const char* title, WeightMap& dict,
                                 AfflictionIcons* icons, float width, float height) {
    ImGui::BeginChild(id, ImVec2(width, height), true);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::PushID(id);
    const float iconSz = ImGui::GetFrameHeight();
    for (auto& row : SortedRows(dict, &AfflictionName, true)) {
        const AfflictionInfo* info = FindAffliction(row.entry->first.c_str());
        ImGui::PushID(row.entry->first.c_str());
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
        ImGui::DragFloat("##w", &row.entry->second, 25.0f, -1000000.0f, 1000000.0f, "%.0f");
        ImGui::SameLine();
        ImTextureID tex = (icons && info) ? icons->Get(info->iconFile) : ImTextureID{};
        if (tex) ImGui::Image(tex, ImVec2(iconSz, iconSz));
        else     ImGui::Dummy(ImVec2(iconSz, iconSz));
        bool hovered = ImGui::IsItemHovered();
        ImGui::SameLine(0.0f, 5.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.text.c_str());
        hovered = hovered || ImGui::IsItemHovered();
        if (hovered) {
            if (info) AfflictionTooltip(*info, icons);
            else      ImGui::SetTooltip("%s", row.text.c_str());
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::EndChild();
}

static void DrawProfilesTab(Settings& s, AfflictionIcons* icons) {
    if (ImGui::BeginCombo(I18n::TrId("profiles_profile"), ProfileName(s.activeProfileName).c_str())) {
        for (auto& p : s.profiles) {
            const std::string label = ProfileName(p.name) + "###" + p.name;
            if (ImGui::Selectable(label.c_str(), p.name == s.activeProfileName))
                s.activeProfileName = p.name;
        }
        ImGui::EndCombo();
    }
    WeightProfile* prof = s.ActiveProfile();
    if (!prof) { ImGui::TextDisabled("%s", I18n::Tr("profiles_none")); return; }
    const ImGuiStyle& style = ImGui::GetStyle();
    SameLineIfFits(style.ItemSpacing.x,
                   ImGui::CalcTextSize(I18n::Tr("profiles_reset")).x + style.FramePadding.x * 2.0f);
    if (ImGui::Button(I18n::TrId("profiles_reset"))) {
        for (auto& def : DefaultProfiles())
            if (def.name == prof->name) { *prof = def; MergeProfileDefaults(*prof); break; }
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
    ImGui::InputTextWithHint("##search", I18n::Tr("profiles_search_hint"), s_search, sizeof(s_search));
    // Three side-by-side columns: Afflictions | Room types | Rewards.
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float colW = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
    const float colH = 320.0f;
    DrawAfflictionColumn("##afflictions", I18n::Tr("profiles_afflictions"), prof->afflictionWeights, icons, colW, colH);
    ImGui::SameLine();
    DrawWeightColumn("##room_types", I18n::Tr("profiles_room_types"), prof->roomTypeWeights, &RoomTypeName, colW, colH);
    ImGui::SameLine();
    DrawWeightColumn("##rewards", I18n::Tr("profiles_rewards"), prof->rewardWeights, &RewardName, colW, colH);
    ImGui::Separator();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.5f);
    ImGui::DragFloat(I18n::TrId("profiles_avoid_merchant"), &prof->avoidMerchantBelowWater,
                     1.0f, 0.0f, 1000.0f, "%.0f");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.5f);
    ImGui::DragFloat(I18n::TrId("profiles_avoid_honour"), &prof->avoidHonourAbovePct,
                     1.0f, 0.0f, 100.0f, "%.0f%%");
}

static void DrawDisplayTab(Settings& s) {
    ImGui::Checkbox(I18n::TrId("display_draw_best_path"), &s.drawBestPath);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 11.0f);
    ImGui::SliderFloat(I18n::TrId("display_frame_thickness"), &s.frameThickness, 1.0f, 6.0f, "%.1f");
    ImGui::ColorEdit4(I18n::TrId("display_best_path_color"), &s.bestPathColor.x, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();
    ImGui::Checkbox(I18n::TrId("display_show_dashboard"), &s.dashboardVisible);
    ImGui::Checkbox(I18n::TrId("display_auto_show"), &s.dashboardAutoShow);
    HotkeyPicker(s);
    ImGui::SeparatorText(I18n::Tr("display_timers"));
    ImGui::Checkbox(I18n::TrId("display_timer_overlay"), &s.timerOverlayEnabled);
    Tooltip(I18n::Tr("display_timer_overlay_tip"));
    ImGui::Checkbox(I18n::TrId("display_show_floor_line"), &s.timerShowFloorLine);
    ImGui::Checkbox(I18n::TrId("display_show_room_line"), &s.timerShowRoomLine);
    ImGui::Checkbox(I18n::TrId("display_debug_log"), &s.timerDebugLog);
}

static void DrawOverlaysTab(Settings& s) {
    const float gap = ImGui::GetFontSize();
    // POI row: portals / levers / crystals inline with their colors.
    ImGui::Checkbox(I18n::TrId("overlays_portals"), &s.showPortals); ImGui::SameLine();
    ImGui::ColorEdit4("##pc", &s.portalColor.x, ImGuiColorEditFlags_NoInputs);
    SameLineIfFits(gap, CheckboxWithColorWidth(I18n::Tr("overlays_levers")));
    ImGui::Checkbox(I18n::TrId("overlays_levers"), &s.showLevers); ImGui::SameLine();
    ImGui::ColorEdit4("##lc", &s.leverColor.x, ImGuiColorEditFlags_NoInputs);
    SameLineIfFits(gap, CheckboxWithColorWidth(I18n::Tr("overlays_crystals")));
    ImGui::Checkbox(I18n::TrId("overlays_crystals"), &s.showCrystals); ImGui::SameLine();
    ImGui::ColorEdit4("##cc", &s.crystalColor.x, ImGuiColorEditFlags_NoInputs);

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
    ImGui::SliderFloat(I18n::TrId("overlays_poi_size"), &s.poiRadius, 4.0f, 20.0f, "%.0f");
    SameLineIfFits(gap, ImGui::CalcTextSize(I18n::Tr("overlays_room_bounds")).x);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(I18n::Tr("overlays_room_bounds"));
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Chests: master toggles + per-type table below.
    ImGui::Checkbox(I18n::TrId("overlays_chests"), &s.showChests);
    SameLineIfFits(gap, CheckboxWidth(I18n::Tr("overlays_labels")));
    ImGui::Checkbox(I18n::TrId("overlays_labels"), &s.showChestLabels);
    Tooltip(I18n::Tr("overlays_labels_tip"));
    SameLineIfFits(gap, ImGui::GetFontSize() * 9.0f + ImGui::GetStyle().ItemInnerSpacing.x +
                            ImGui::CalcTextSize(I18n::Tr("overlays_circle_size")).x);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
    ImGui::SliderFloat(I18n::TrId("overlays_circle_size"), &s.chestRadius, 2.0f, 16.0f, "%.0f");

    ImGui::TextDisabled("%s", I18n::Tr("overlays_ring_hint"));

    if (ImGui::SmallButton(I18n::TrId("overlays_show_all"))) for (auto& t : s.chestTypes) t.show = true;
    ImGui::SameLine();
    if (ImGui::SmallButton(I18n::TrId("overlays_hide_all"))) for (auto& t : s.chestTypes) t.show = false;
    ImGui::SameLine();
    if (ImGui::SmallButton(I18n::TrId("overlays_reset"))) s.chestTypes = DefaultChestTypes();
    Tooltip(I18n::Tr("overlays_reset_tip"));

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));
    if (ImGui::BeginTable("chest_types", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
            ImVec2(0.0f, 320.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(I18n::TrId("overlays_col_on"),    ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("overlays_col_color"), ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("overlays_col_cache"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(I18n::TrId("overlays_col_ring"),  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < s.chestTypes.size(); ++i) {
            auto& t = s.chestTypes[i];
            const ChestTypeInfo* info = FindChestTypeInfo(t.id);
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Checkbox("##show", &t.show);
            ImGui::TableNextColumn(); ImGui::ColorEdit4("##col", &t.color.x,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(ChestName(t.id, info ? info->uiName : t.id).c_str());
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
        if (ImGui::BeginTabItem(I18n::TrId("tab_display")))  { DrawDisplayTab(s);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(I18n::TrId("tab_profiles"))) { DrawProfilesTab(s, icons); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(I18n::TrId("tab_overlays"))) { DrawOverlaysTab(s); ImGui::EndTabItem(); }
        if (db && hist && ImGui::BeginTabItem(I18n::TrId("tab_history"))) { DrawHistoryTab(*db, *hist); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

} // namespace sekhema
