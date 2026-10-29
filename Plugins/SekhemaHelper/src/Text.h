#pragma once
// Text.h — the plugin's texts in the program language (PluginI18n.h, through PluginSDK.h):
// lang/<code>.json next to the DLL, English compiled in through lang_en.generated.h
// (included once, by SekhemaHelper.cpp; python tools/plugin_i18n/gen.py
// Plugins/SekhemaHelper after editing lang/en.json).
//
// Draw thread only. The worker, the run history and the saved weight profiles keep the
// English catalog names (they are identities); the functions below turn them into text
// when they are drawn, and show the English name itself when the plugin has no text for
// it (a game row newer than the plugin, a name read from a localized client).
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "AfflictionCatalog.h"
#include "I18nKeys.h"
#include <imgui.h>
#include <sal.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace sekhema {

namespace I18n = PluginSDK::I18n;

// printf for a TrF argument: Num("%d", n). The format is code, never a translation.
inline std::string Num(_In_z_ _Printf_format_string_ const char* format, ...) {
    char buf[128];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

// One line of a text TrF built (TextUnformatted: a translation is no format).
inline void Show(const std::string& text) { ImGui::TextUnformatted(text.c_str()); }

// A wrapped tooltip for the item just drawn, while it is hovered.
inline void Tooltip(const char* text) {
    if (!ImGui::IsItemHovered()) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// The text of a key built at run time (I18nKeys.h); `english` when lang/en.json has none.
inline std::string TrOr(const std::string& key, const std::string& english) {
    const char* text = I18n::Tr(key.c_str());
    return std::strcmp(text, key.c_str()) == 0 ? english : std::string(text);
}

inline std::string AfflictionName(const std::string& english) {
    return TrOr(keys::Make(keys::kAfflictionName, english), english);
}

// The catalog description in the program language; its percentage comes from the catalog.
inline std::string AfflictionDescription(const AfflictionInfo& info) {
    const std::string key = keys::Make(keys::kAfflictionDesc, info.name);
    if (std::strcmp(I18n::Tr(key.c_str()), key.c_str()) == 0) return info.desc;
    return I18n::TrF(key.c_str(), { { "percent", keys::FirstPercent(info.desc) } });
}

inline std::string RoomTypeName(const std::string& english) {
    return TrOr(keys::Make(keys::kRoomType, english), english);
}

inline std::string RewardName(const std::string& english) {
    return TrOr(keys::Make(keys::kReward, english), english);
}

inline std::string FloorName(const std::string& english) {
    return TrOr(keys::Make(keys::kFloor, english), english);
}

// A chest type's Overlays-tab row name and its short map label (ChestTypes.h ids).
inline std::string ChestName(const std::string& id, const std::string& english) {
    return TrOr(keys::Make(keys::kChestName, id), english);
}

inline std::string ChestLabel(const std::string& id, const std::string& english) {
    return TrOr(keys::Make(keys::kChestLabel, id), english);
}

// A weight profile's name: the two shipped profiles in the program language while they
// keep their shipped names (the saved name is the profile's identity); any other as saved.
inline std::string ProfileName(const std::string& name) {
    if (name == "Default") return I18n::Tr("profile_default");
    if (name == "No-Hit") return I18n::Tr("profile_no_hit");
    return name;
}

} // namespace sekhema
