#pragma once
#include "Model.h"

namespace sekhema {
// Marks the best-weighted path from the current room to the deepest reachable
// layer and fills each room's pathScore (the sum of weights along the best onward
// branch, with the per-room kRoomScoreBase stripped so branch length doesn't
// dominate). Requires room.score to be populated first.
void ComputeBestPath(SekhemaFloor& floor);
}
