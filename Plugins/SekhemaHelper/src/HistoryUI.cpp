#include "HistoryUI.h"
#include "Theme.h"
#include <imgui.h>
#include <cstdio>

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
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete this run");
    return clicked;
}

static ImU32 StatusColor(const Theme& th, const std::string& s) {
    if (s == "completed")   return th.good;
    if (s == "failed")      return th.riskSevere;
    if (s == "abandoned")   return th.riskModerate;
    return th.accent;   // in_progress
}

static void DrawRoomsTable(const std::vector<RunDatabase::RoomRow>& rooms, const Theme& th) {
    if (rooms.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted("  (no rooms recorded)");
        ImGui::PopStyleColor();
        return;
    }
    if (ImGui::BeginTable("rooms", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("#",          ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Type",       ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Affliction", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Reward",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Time",       ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableHeadersRow();
        char dur[32];
        for (const auto& r : rooms) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", r.layer + 1);
            ImGui::TableNextColumn();
            if (r.isBoss) {
                ImGui::PushStyleColor(ImGuiCol_Text, th.riskSevere);
                ImGui::TextUnformatted(r.type.empty() ? "Boss" : r.type.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextUnformatted(r.type.empty() ? "?" : r.type.c_str());
            }
            ImGui::TableNextColumn(); ImGui::TextUnformatted(r.affliction.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(r.reward.c_str());
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
        char label[128];
        FmtDurMs(f.durationMs, dur, sizeof(dur));
        snprintf(label, sizeof(label), "%s Floor %d \xC2\xB7 %s   %s   (%d rooms)%s",
                 open ? "\xE2\x96\xBE" : "\xE2\x96\xB8", f.floorNum, f.floorName.c_str(), dur,
                 f.roomsCleared, f.bossKilledMs >= 0 ? "  \xE2\x9C\x93 boss" : "");
        if (ImGui::Selectable(label, open)) {
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
        ImGui::TextUnformatted("Runs"); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::Text("%d", st.stats.total); ImGui::SameLine(0, 18);
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted("Completed"); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::Text("%d (%d%%)", st.stats.completed, pct); ImGui::SameLine(0, 18);
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted("Best"); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::TextUnformatted(best); ImGui::SameLine(0, 18);
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted("Avg"); ImGui::SameLine(); ImGui::PopStyleColor();
        ImGui::TextUnformatted(avg);
    }
    ImGui::Separator();

    if (st.runs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, th.textDim);
        ImGui::TextUnformatted("No runs recorded yet — enter the Trial of the Sekhemas.");
        ImGui::PopStyleColor();
        return;
    }

    // Runs table (expand rows into floors -> rooms)
    if (ImGui::BeginTable("runs", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Date",      ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Status",    ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Duration",  ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Floors",    ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##del",     ImGuiTableColumnFlags_WidthFixed, 26.0f);
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
            ImGui::TextUnformatted(r.status.c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            FmtDurMs(r.durationMs, dur, sizeof(dur));
            ImGui::TextUnformatted(dur);

            ImGui::TableNextColumn();
            ImGui::Text("%d/%d", r.floorsDone, r.lastFloor > 0 ? r.lastFloor : r.floorsDone);

            ImGui::TableNextColumn();
            if (r.areaLevel > 0) ImGui::Text("%s (lvl %d)", r.charName.c_str(), r.areaLevel);
            else                 ImGui::TextUnformatted(r.charName.c_str());

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
        if (ImGui::Button("Clear history")) st.confirmClear = true;
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, th.riskSevere);
        ImGui::TextUnformatted("Delete ALL recorded runs?");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Delete all")) {
            db.ClearAll();
            st.expandedRun = -1; st.expandedFloor = -1;
            st.floors.clear(); st.rooms.clear();
            st.dirty = true;
            st.confirmClear = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) st.confirmClear = false;
    }
}

} // namespace sekhema
