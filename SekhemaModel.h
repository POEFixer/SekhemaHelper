#pragma once
// SekhemaModel.h — walk the live FloorData room graph from the trial map panel.
#include "Model.h"
#include "sdk/PluginSDK.h"

namespace sekhema {

struct SekhemaReader {
    // Walk the FloorData reachable from the SekhemasTrialMapPanel UI element.
    // Returns {valid=false} when no FloorData resolves (off-Trial / drifted
    // offsets) — never throws, never crashes the host (fail-closed bounds).
    static SekhemaFloor Read(uintptr_t panelAddr, const PluginSDK::Context* ctx);
};

} // namespace sekhema
