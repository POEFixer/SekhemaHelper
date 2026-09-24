#pragma once
// MapOverlay.h — draw best-path frames + risk dots on the in-game trial map.
#include "Model.h"
#include "Settings.h"
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

namespace sekhema {
// Draws onto the foreground draw list over the trial map. `panel` = the resolved
// SekhemasTrialMapPanel. No-op where ComputeScreenRect fails (map closed).
void DrawMapOverlay(const SekhemaFloor& floor, const Settings& s,
                    const PluginSDK::Context* ctx, uintptr_t panel);
}
