#include "HistoryUI.h"
#include "Theme.h"
#include "Text.h"
#include <imgui.h>
#include <cstdio>
#include <string>

namespace sekhema {

static void FmtDurMs(int64_t ms, char* out, size_t n) {
    if (ms < 0) { snprintf(out, n, "\xE2\x80\x94"); return; }   // em dash
    uint64_t t = static_cast<uint64_t>(ms);
    uint64_t s = t / 1000, h = s / 3600, m = (s % 3600) / 60, sec = s % 60, mil = t % 1000;
    if (h) snprintf(out, n, "%llu:%02llu:%02llu.%03llu", (unsigned long long)h,
                    (unsigned long long)m, (unsigned long long)sec, (unsigned long long)mil);
    else   snprintf(out, n, "%02llu:%02llu.%03llu", (unsigned long long)m,
                    (unsigned long long)sec, (unsigned long long)mil);
}

// Dim ✕ that turns red on hover — table-row delete affordance.
static bool DeleteButton(const Theme& th) {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 40, 40, 160));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(160, 50, 50, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
    bool clicked = ImGui::SmallButton("\xE2\x9C\x95");
    ImGui::PopStyleColor(4);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", I18n::Tr("history_delete_run"));
    return clicked;
}

static ImU32 StatusColor(const Theme& th, const std::string& s) {
    if (s == "completed")   return th.good;
    if (s == "failed")      return th.riskSevere;
    if (s == "abandoned")   return th.riskModerate;
    return th.accent;   // in_progress
}

// A run's stored status ("completed", ...: an identity in the database) as text.
static std::string StatusText(const std::string& s) {
    if (s == "completed")   return I18n::Tr("history_status_completed");
    if (s == "failed")      return I18n::Tr("history_status_failed");
    if (s == "abandoned")   return I18n::Tr("history_status_abandoned");
    if (s == "in_progress") return I18n::Tr("history_status_in_progress");
    return s;
}

static void DrawRoomsTable(const std::vector<RunDatabase::RoomRow>& rooms, const Theme& th) {
    if (rooms.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::Indent();
        ImGui::TextUnformatted(I18n::Tr("history_no_rooms"));
        ImGui::Unindent();
        ImGui::PopStyleColor();
        return;
    }
    if (ImGui::BeginTable("rooms", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        // Fixed columns without a width fit their (translated) header and cells every frame.
        ImGui::TableSetupColumn("#",                                  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("history_col_type"),       ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(I18n::TrId("history_col_affliction"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(I18n::TrId("history_col_reward"),     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(I18n::TrId("history_col_time"),       ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        char dur[32];
        for (const auto& r : rooms) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", r.layer + 1);
            ImGui::TableNextColumn();
            if (r.isBoss) {
                ImGui::PushStyleColor(ImGuiCol_Text, th.riskSevere);
                ImGui::TextUnformatted(r.type.empty() ? I18n::Tr("room_boss") : RoomTypeName(r.type).c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextUnformatted(r.type.empty() ? "?" : RoomTypeName(r.type).c_str());
            }
            ImGui::TableNextColumn(); ImGui::TextUnformatted(AfflictionName(r.affliction).c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(RewardName(r.reward).c_str());
            ImGui::TableNextColumn();
            FmtDurMs(r.durationMs, dur, sizeof(dur));
            ImGui::TextUnformatted(dur);
        }
        ImGui::EndTable();
    }
}

static void DrawFloorsTable(RunDatabase& db, HistoryUIState& st, const Theme& th) {
    char dur[32];
    for (const auto& f : st.floors) {
        ImGui::PushID(static_cast<int>(f.id));
        const bool open = (st.expandedFloor == f.id);
        FmtDurMs(f.durationMs, dur, sizeof(dur));
        // "Floor 2 · Ruins   01:23.456   (5 rooms)  boss" with the arrow and check glyphs;
        // the row's id stays "###floor" in every language.
        std::string label = std::string(open ? "\xE2\x96\xBE " : "\xE2\x96\xB8 ") +
            I18n::TrF("history_floor", { { "number", Num("%d", f.floorNum) } }) + " \xC2\xB7 " +
            FloorName(f.floorName) + "   " + dur + "   (" +
            I18n::TrF("history_floor_rooms", { { "count", Num("%d", f.roomsCleared) } }) + ")";
        if (f.bossKilledMs >= 0) label += std::string("  \xE2\x9C\x93 ") + I18n::Tr("history_floor_boss");
        label += "###floor";
        if (ImGui::Selectable(label.c_str(), open)) {
            if (open) { st.expandedFloor = -1; st.rooms.clear(); }
            else      { st.expandedFloor = f.id; st.rooms = db.QueryRooms(f.id); }
        }
        if (st.expandedFloor == f.id) {
            ImGui::Indent(18.0f);
            DrawRoomsTable(st.rooms, th);
            ImGui::Unindent(18.0f);
        }
        ImGui::PopID();
    }
}

void DrawHistoryTab(RunDatabase& db, HistoryUIState& st) {
    Theme th = MakeTheme();

    if (st.dirty) {
        st.runs = db.QueryRuns(200);
        st.stats = db.QueryStats();
        if (st.expandedRun >= 0)   st.floors = db.QueryFloors(st.expandedRun);
        if (st.expandedFloor >= 0) st.rooms = db.QueryRooms(st.expandedFloor);
        st.dirty = false;
    }

    // Stats strip
    {
        char best[32], avg[32];
        FmtDurMs(st.stats.bestMs, best, sizeof(best));
        FmtDurMs(st.stats.avgMs, avg, sizeof(avg));
        int pct = st.stats.total > 0 ? (st.stats.completed * 100) / st.stats.total : 0;
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        const float gap = ImGui::GetFontSize();
        ImGui::TextUnformatted(I18n::Tr("history_runs")); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::Text("%d", st.stats.total); ImGui::SameLine(0, gap);
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted(I18n::Tr("history_completed")); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::Text("%d (%d%%)", st.stats.completed, pct); ImGui::SameLine(0, gap);
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted(I18n::Tr("history_best")); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::TextUnformatted(best); ImGui::SameLine(0, gap);
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted(I18n::Tr("history_average")); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::TextUnformatted(avg);
    }
    ImGui::Separator();

    if (st.runs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted(I18n::Tr("history_empty"));
        ImGui::PopStyleColor();
        return;
    }

    // Runs table (expand rows into floors -> rooms)
    if (ImGui::BeginTable("runs", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        // Fixed columns without a width fit their (translated) header and cells every frame.
        ImGui::TableSetupColumn(I18n::TrId("history_col_date"),      ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("history_col_status"),    ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("history_col_duration"),  ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("history_col_floors"),    ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn(I18n::TrId("history_col_character"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##del",                             ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        char dur[32];
        int64_t deleteId = -1;
        for (const auto& r : st.runs) {
            ImGui::PushID(static_cast<int>(r.id));
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            const bool open = (st.expandedRun == r.id);
            if (ImGui::Selectable(r.startedText.c_str(), open,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                  ImGuiSelectableFlags_AllowOverlap)) {
                if (open) { st.expandedRun = -1; st.floors.clear(); }
                else      { st.expandedRun = r.id; st.floors = db.QueryFloors(r.id); }
                st.expandedFloor = -1; st.rooms.clear();
            }

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(th, r.status));
            ImGui::TextUnformatted(StatusText(r.status).c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            FmtDurMs(r.durationMs, dur, sizeof(dur));
            ImGui::TextUnformatted(dur);

            ImGui::TableNextColumn();
            ImGui::Text("%d/%d", r.floorsDone, r.lastFloor > 0 ? r.lastFloor : r.floorsDone);

            ImGui::TableNextColumn();
            if (r.areaLevel > 0)
                Show(I18n::TrF("history_character_level", { { "name", r.charName },
                    { "level", Num("%d", r.areaLevel) } }));
            else
                ImGui::TextUnformatted(r.charName.c_str());

            ImGui::TableNextColumn();
            if (DeleteButton(th)) deleteId = r.id;
            ImGui::PopID();
        }
        ImGui::EndTable();

        if (deleteId >= 0) {
            db.DeleteRun(deleteId);
            if (st.expandedRun == deleteId) { st.expandedRun = -1; st.floors.clear(); }
            st.dirty = true;
        }
    }

    // Expanded run: floors (below the table so the layout stays simple)
    if (st.expandedRun >= 0) {
        ImGui::Separator();
        DrawFloorsTable(db, st, th);
    }

    // Clear-all with inline confirm (no modal — overlay-mode clickthrough trap)
    ImGui::Separator();
    if (!st.confirmClear) {
        if (ImGui::Button(I18n::TrId("history_clear"))) st.confirmClear = true;
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, th.riskSevere);
        ImGui::TextUnformatted(I18n::Tr("history_clear_confirm"));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("history_delete_all"))) {
            db.ClearAll();
            st.expandedRun = -1; st.expandedFloor = -1;
            st.floors.clear(); st.rooms.clear();
            st.dirty = true;
            st.confirmClear = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("history_cancel"))) st.confirmClear = false;
    }
}

} // namespace sekhema
