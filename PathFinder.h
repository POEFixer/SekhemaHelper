#pragma once
#include "Model.h"

namespace sekhema {
// Marks the longest-weighted path (by room.score) from the current room to the
// deepest reachable layer. Requires room.score to be populated first.
void ComputeBestPath(SekhemaFloor& floor);
}
