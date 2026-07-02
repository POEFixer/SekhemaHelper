#pragma once
// HistoryUI.h — "History" settings tab: run list -> floors -> rooms with
// durations, stats strip, per-run delete and clear-all.
#include "RunDatabase.h"

namespace sekhema {

struct HistoryUIState {
    bool dirty = true;                        // reload caches on next draw
    std::vector<RunDatabase::RunRow> runs;
    RunDatabase::Stats stats;
    int64_t expandedRun = -1;
    std::vector<RunDatabase::FloorRow> floors;   // of expandedRun
    int64_t expandedFloor = -1;
    std::vector<RunDatabase::RoomRow> rooms;     // of expandedFloor
    bool confirmClear = false;
};

void DrawHistoryTab(RunDatabase& db, HistoryUIState& st);

} // namespace sekhema
