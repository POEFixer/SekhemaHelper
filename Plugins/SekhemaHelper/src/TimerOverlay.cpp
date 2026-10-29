#include "TimerOverlay.h"
#include "Theme.h"
#include "Text.h"
#include "../../../POEFixer/plugin_sdk/PluginInfoWindow.h"
#include <imgui.h>
#include <cstdio>
#include <string>

namespace sekhema {

static constexpr uint64_t kFloorLockShowMs   = 4000;    // green "Floor N ✓" hold
static constexpr uint64_t kRunCompleteLinger = 15000;   // keep the window after exit

// MM:SS.mmm (H:MM:SS.mmm with hours) — millisecond precision per user request.
static void FmtDur(uint64_t ms, char* out, size_t n) {
    uint64_t s = ms / 1000, h = s / 3600, m = (s % 3600) / 60, sec = s % 60, mil = ms % 1000;
    if (h) snprintf(out, n, "%llu:%02llu:%02llu.%03llu", (unsigned long long)h,
                    (unsigned long long)m, (unsigned long long)sec, (unsigned long long)mil);
    else   snprintf(out, n, "%02llu:%02llu.%03llu", (unsigned long long)m,
                    (unsigned long long)sec, (unsigned long long)mil);
}

// One overlay line: dim label + bright value, vertically consistent.
static void Line(ImU32 labelCol, const std::string& label, ImU32 valueCol, const char* value) {
    ImGui::PushStyleColor(ImGuiCol_Text, labelCol);
    ImGui::TextUnformatted(label.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, valueCol);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

// The timer lines, inside the open window.
static void DrawTimerLines(const Theme& th, const RunTracker::Overlay& ov, const Settings& s,
                           uint64_t nowMs) {
    char dur[40];
    const std::string check = " \xE2\x9C\x93";   // " ✓"
    const std::string dot = " \xC2\xB7 ";         // " · "

    // Line 1: run (frozen at the final boss kill once completed)
    if (ov.runComplete) {
        FmtDur(ov.runEndMs - ov.runStartMs, dur, sizeof(dur));
        Line(th.good, I18n::Tr("timer_run_complete"), th.good, dur);
    } else {
        FmtDur(nowMs > ov.runStartMs ? nowMs - ov.runStartMs : 0, dur, sizeof(dur));
        Line(th.accent, I18n::Tr("timer_run"), th.text, dur);
    }

    // Line 2: floor (locks in green for a few seconds after the boss kill)
    if (s.timerShowFloorLine && ov.floorNum > 0) {
        const bool locked = ov.lockedFloorNum > 0 && ov.floorBossKillMs > 0 &&
                            nowMs >= ov.floorBossKillMs &&
                            nowMs - ov.floorBossKillMs < kFloorLockShowMs;
        if (locked) {
            FmtDur(ov.lockedFloorDurationMs, dur, sizeof(dur));
            Line(th.good, I18n::TrF("timer_floor", { { "number", Num("%d", ov.lockedFloorNum) } }) + check,
                 th.good, dur);
        } else {
            FmtDur(nowMs > ov.floorStartMs ? nowMs - ov.floorStartMs : 0, dur, sizeof(dur));
            Line(th.textDim, I18n::TrF("timer_floor", { { "number", Num("%d", ov.floorNum) } }) + dot +
                 FloorName(ov.floorName), th.text, dur);
        }
    }

    // Line 3: room — running, frozen (✓ at the map device, until the next
    // door click), or idle.
    if (s.timerShowRoomLine) {
        // The room's type ("Boss", "Hourglass", ...: the tracker keeps the English name).
        const std::string roomName = (ov.roomLabel.empty() || ov.roomLabel == "?")
                                   ? std::string("?") : RoomTypeName(ov.roomLabel);
        if (ov.roomOpen && ov.roomFrozen && !ov.runComplete) {
            FmtDur(ov.roomFrozenDurationMs, dur, sizeof(dur));
            Line(th.good, I18n::TrF("timer_room", { { "number", Num("%d", ov.roomLayer + 1) } }) + dot +
                 roomName + check, th.good, dur);
        } else if (ov.roomOpen && !ov.runComplete) {
            FmtDur(nowMs > ov.roomStartMs ? nowMs - ov.roomStartMs : 0, dur, sizeof(dur));
            Line(ov.roomIsBoss ? th.riskSevere : th.textDim,
                 I18n::TrF("timer_room", { { "number", Num("%d", ov.roomLayer + 1) } }) + dot + roomName,
                 th.text, dur);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
            ImGui::TextUnformatted((std::string(I18n::Tr("timer_room_idle")) + dot + "\xE2\x80\x94").c_str());
            ImGui::PopStyleColor();
        }
    }
}

void DrawTimerOverlay(const PluginSDK::OverlayService& overlay, const RunTracker::Overlay& ov,
                      Settings& s, uint64_t nowMs) {
    if (!s.timerOverlayEnabled) return;
    const bool linger = ov.runComplete && nowMs >= ov.runEndMs &&
                        nowMs - ov.runEndMs < kRunCompleteLinger;
    if (!ov.inTrial && !linger) return;
    if (ov.runStartMs == 0) return;

    Theme th = MakeTheme();

    // The host places the window, remembers its place in and out of the game and
    // draws its gear menu (top-right corner on hover: lock, Game overlay mode,
    // reset, close). timerOverlayPos only seeds its store once. Close clears
    // timerOverlayEnabled.
    PluginSDK::InfoWindowOptions o;
    o.id = "timers";
    o.title = nullptr;
    o.flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing |
              ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    o.defaultPos = ImVec2(40.0f, 40.0f);
    o.legacyPos = s.timerOverlayPos;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 3));
    {
        PluginSDK::InfoWindow window(overlay, o, &s.timerOverlayEnabled);
        if (window) DrawTimerLines(th, ov, s, nowMs);
    }   // the window ends here; its style is popped only after it
    ImGui::PopStyleVar(2);
}

} // namespace sekhema
