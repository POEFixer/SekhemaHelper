#pragma once
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "Model.h"
#include "Settings.h"

namespace sekhema {
// The dashboard window: an informational window of the host (ctx()->Overlay), with its
// gear menu; its Close clears settings.dashboardVisible.
void DrawDashboard(const PluginSDK::OverlayService& overlay, const SekhemaFloor& floor,
                   const SekhemaResources& res, Settings& settings);
}
