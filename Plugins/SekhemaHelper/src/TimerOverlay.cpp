#include "TimerOverlay.h"
#include "Theme.h"
#include <imgui.h>
#include <cstdio>

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
static void Line(ImU32 labelCol, const char* label, ImU32 valueCol, const char* value) {
    ImGui::PushStyleColor(ImGuiCol_Text, labelCol);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, valueCol);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

void DrawTimerOverlay(const RunTracker::Overlay& ov, Settings& s, uint64_t nowMs) {
    if (!s.timerOverlayEnabled) return;
    const bool linger = ov.runComplete && nowMs >= ov.runEndMs &&
                        nowMs - ov.runEndMs < kRunCompleteLinger;
    if (!ov.inTrial && !linger) return;
    if (ov.runStartMs == 0) return;

    Theme th = MakeTheme();

    ImGui::SetNextWindowPos(s.timerOverlayPos, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 3));
    if (ImGui::Begin("##sekhema_timers", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
        s.timerOverlayPos = ImGui::GetWindowPos();   // drag persists via SaveSettings

        char dur[40], label[96];

        // Line 1: run (frozen at the final boss kill once completed)
        if (ov.runComplete) {
            FmtDur(ov.runEndMs - ov.runStartMs, dur, sizeof(dur));
            Line(th.good, "RUN COMPLETE", th.good, dur);
        } else {
            FmtDur(nowMs > ov.runStartMs ? nowMs - ov.runStartMs : 0, dur, sizeof(dur));
            Line(th.accent, "SEKHEMA", th.text, dur);
        }

        // Line 2: floor (locks in green for a few seconds after the boss kill)
        if (s.timerShowFloorLine && ov.floorNum > 0) {
            const bool locked = ov.lockedFloorNum > 0 && ov.floorBossKillMs > 0 &&
                                nowMs >= ov.floorBossKillMs &&
                                nowMs - ov.floorBossKillMs < kFloorLockShowMs;
            if (locked) {
                FmtDur(ov.lockedFloorDurationMs, dur, sizeof(dur));
                snprintf(label, sizeof(label), "Floor %d \xE2\x9C\x93", ov.lockedFloorNum);
                Line(th.good, label, th.good, dur);
            } else {
                FmtDur(nowMs > ov.floorStartMs ? nowMs - ov.floorStartMs : 0, dur, sizeof(dur));
                snprintf(label, sizeof(label), "Floor %d \xC2\xB7 %s",
                         ov.floorNum, ov.floorName.c_str());
                Line(th.textDim, label, th.text, dur);
            }
        }

        // Line 3: room — running, frozen (✓ at the map device, until the next
        // door click), or idle.
        if (s.timerShowRoomLine) {
            const char* roomName = (ov.roomLabel.empty() || ov.roomLabel == "?")
                                       ? "?" : ov.roomLabel.c_str();
            if (ov.roomOpen && ov.roomFrozen && !ov.runComplete) {
                FmtDur(ov.roomFrozenDurationMs, dur, sizeof(dur));
                snprintf(label, sizeof(label), "Room %d \xC2\xB7 %s \xE2\x9C\x93",
                         ov.roomLayer + 1, roomName);
                Line(th.good, label, th.good, dur);
            } else if (ov.roomOpen && !ov.runComplete) {
                FmtDur(nowMs > ov.roomStartMs ? nowMs - ov.roomStartMs : 0, dur, sizeof(dur));
                snprintf(label, sizeof(label), "Room %d \xC2\xB7 %s", ov.roomLayer + 1, roomName);
                Line(ov.roomIsBoss ? th.riskSevere : th.textDim, label, th.text, dur);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
                ImGui::TextUnformatted("Room \xC2\xB7 \xE2\x80\x94");
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace sekhema
