#include "PathFinder.h"
#include <vector>

namespace sekhema {

void ComputeBestPath(SekhemaFloor& floor) {
    for (auto& layer : floor.layers)
        for (auto& room : layer) room.onBestPath = false;

    int L = (int)floor.layers.size();
    if (L == 0) return;
    if (floor.playerLayer >= L) return;   // playerLayer < 0 = pre-first-choice (handled below)

    // dp[layer][room] = best cumulative score from this room to the deepest layer;
    // nextChoice[layer][room] = the chosen child index in the next layer (-1 = none).
    std::vector<std::vector<double>> dp(L);
    std::vector<std::vector<int>>    nextChoice(L);
    for (int l = 0; l < L; ++l) {
        dp[l].assign(floor.layers[l].size(), 0.0);
        nextChoice[l].assign(floor.layers[l].size(), -1);
    }

    for (int l = L - 1; l >= 0; --l) {
        for (int r = 0; r < (int)floor.layers[l].size(); ++r) {
            const SekhemaRoom& room = floor.layers[l][r];
            double best = 0.0; int bestIdx = -1; int bestConns = -1;
            if (l + 1 < L) {
                for (int c : room.connections) {
                    if (c < 0 || c >= (int)floor.layers[l + 1].size()) continue;
                    double cand = dp[l + 1][c];
                    int conns = (int)floor.layers[l + 1][c].connections.size();
                    if (bestIdx < 0 || cand > best || (cand == best && conns > bestConns)) {
                        best = cand; bestIdx = c; bestConns = conns;
                    }
                }
            }
            // Accumulate WEIGHTS along the path, not raw scores: strip the per-room
            // base (kRoomScoreBase) or a branch that simply chains through one more
            // room wins by ~1e6, drowning out the configured weights. (`best` is
            // already a base-free child path-sum, so it is added as-is.)
            dp[l][r] = (room.score - kRoomScoreBase) + (bestIdx >= 0 ? best : 0.0);
            floor.layers[l][r].pathScore = dp[l][r];   // for the dashboard's choice ranking
            nextChoice[l][r] = bestIdx;
        }
    }

    // Walk the chain from the current room — or, before the first choice
    // (playerLayer < 0), from the best entrance (layer-0) room.
    int l = floor.playerLayer, r = floor.playerRoom;
    if (l < 0) {
        l = 0;
        double best = 0.0; int bestIdx = -1;
        for (int i = 0; i < (int)floor.layers[0].size(); ++i)
            if (bestIdx < 0 || dp[0][i] > best) { best = dp[0][i]; bestIdx = i; }
        r = bestIdx;
    }
    if (l < 0 || l >= L || r < 0 || r >= (int)floor.layers[l].size()) return;
    while (l >= 0 && l < L && r >= 0 && r < (int)floor.layers[l].size()) {
        floor.layers[l][r].onBestPath = true;
        int nxt = nextChoice[l][r];
        if (nxt < 0) break;
        ++l; r = nxt;
    }
}

} // namespace sekhema
