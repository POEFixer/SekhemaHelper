#pragma once
#include "Model.h"
#include "WeightProfiles.h"

namespace sekhema {
// Scores every room, sets risk + 0..10 normScore, and marks the best path.
void Evaluate(SekhemaFloor& floor, const WeightProfile& profile,
              const PlayerDefenses& defenses, const SekhemaResources& resources);
}
