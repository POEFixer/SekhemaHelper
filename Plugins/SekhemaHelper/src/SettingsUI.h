#pragma once
#include "Settings.h"
#include "HistoryUI.h"
#include "AfflictionIcons.h"

namespace sekhema {
// db/hist may be null (History tab hidden then); icons may be null (no images).
void DrawSettingsPanel(Settings& settings, RunDatabase* db, HistoryUIState* hist,
                       AfflictionIcons* icons);
}
