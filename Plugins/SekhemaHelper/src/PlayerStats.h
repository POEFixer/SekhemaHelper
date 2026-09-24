#pragma once
// PlayerStats.h — read the player's defenses for the build-aware affliction model.
#include "Model.h"
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

namespace sekhema {
// Armour/Evasion/ES/Life (+ Queen of the Forest) via the player's Stats
// component. Zeroed if the player / Stats component isn't available.
PlayerDefenses ReadDefenses(const PluginSDK::Context* ctx);
}
