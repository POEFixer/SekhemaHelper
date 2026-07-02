#pragma once
#include "Settings.h"
#include "HistoryUI.h"

namespace sekhema {
// db/hist may be null (History tab hidden then).
void DrawSettingsPanel(Settings& settings, RunDatabase* db, HistoryUIState* hist);
}
