#pragma once
// LargeMapMarkers.h — draw trial-object markers on the large map AND minimap.
#include "EntityScanner.h"
#include "HazardRoute.h"
#include "Settings.h"
#include "Model.h"
#include "sdk/PluginSDK.h"

namespace sekhema {
// Projects the scanned entities + crystal route onto whichever of the large map /
// minimap is visible, on the foreground draw list.
void DrawLargeMapMarkers(const TrialEntities& ents, const CrystalRoute& route,
                         const Settings& s, const SekhemaResources& res,
                         const PluginSDK::Context* ctx);
}
