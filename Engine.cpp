#include "Engine.h"
#include "WeightCalculator.h"
#include "PathFinder.h"
#include <algorithm>
#include <limits>

namespace sekhema {

void Evaluate(SekhemaFloor& floor, const WeightProfile& profile,
              const PlayerDefenses& defenses, const SekhemaResources& resources) {
    double lo =  std::numeric_limits<double>::max();
    double hi = -std::numeric_limits<double>::max();
    for (auto& layer : floor.layers) {
        for (auto& room : layer) {
            room.score = ScoreRoom(room, profile, defenses, resources);
            double aw  = AfflictionWeight(room.affliction, profile, defenses);
            room.risk  = RiskFor(aw, !room.affliction.empty());
            lo = std::min(lo, room.score);
            hi = std::max(hi, room.score);
        }
    }
    double span = hi - lo;
    for (auto& layer : floor.layers)
        for (auto& room : layer)
            room.normScore = (span > 1e-9) ? 10.0 * (room.score - lo) / span : 10.0;

    ComputeBestPath(floor);
}

} // namespace sekhema
