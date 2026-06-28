#pragma once
// ResourceReader.h — Honour% / Sacred Water / keys from the trial UI.
#include "Model.h"
#include "sdk/PluginSDK.h"

namespace sekhema {
// Reads live resources via ctx()->Ui. `valid` is set only if the Honour bar
// resolves (frame width > 0). Off-Trial / wrong indices -> zeros, valid=false.
SekhemaResources ReadResources(const PluginSDK::Context* ctx, uintptr_t trialPanel);
}
