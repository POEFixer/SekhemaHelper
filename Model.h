#pragma once
#include <string>
#include <vector>

namespace sekhema {

// Per-room base added to every ScoreRoom result. It keeps a single room's score
// positive for the 0..10 display normalization, but it is NOT a weight: it must be
// removed before summing scores along a path, otherwise a longer branch wins on
// room-count alone (1e6 per extra room) and the configured weights stop mattering.
inline constexpr double kRoomScoreBase = 1000000.0;

enum class Risk { None, Minor, Moderate, Severe };

struct SekhemaRoom {
    std::string roomType;     // "Hourglass","Chalice","Escape","Ritual","Gauntlet","Boss",""
    std::string affliction;   // display name; "" if none
    std::string reward;       // "Boon","Large Fountain","Merchant",...; "" if none
    int   rewardValue = 0;    // e.g. +200 (display only)
    std::vector<int> connections; // indices into the NEXT layer
    double score     = 0.0;   // raw weight (engine fills; mock supplies directly)
    double normScore = 0.0;   // 0..10 normalized, for display
    double pathScore = 0.0;   // best onward path value from here, weights only/base-free (PathFinder)
    Risk   risk      = Risk::None;
    bool   onBestPath= false;
    bool   isChosen  = false;   // player has walked into this room (== Choices[layer])
    // room widget screen rect (filled by the reader in Phase 3; 0 in mock)
    float sx = 0, sy = 0, sw = 0, sh = 0; bool screenValid = false;
};

struct SekhemaFloor {
    std::vector<std::vector<SekhemaRoom>> layers; // layers[layer][room]
    int  playerLayer = 0;
    int  playerRoom  = 0;
    bool valid       = false;

    const SekhemaRoom* CurrentRoom() const {
        if (playerLayer < 0 || playerLayer >= (int)layers.size()) return nullptr;
        const auto& L = layers[playerLayer];
        if (playerRoom < 0 || playerRoom >= (int)L.size()) return nullptr;
        return &L[playerRoom];
    }

    // The live choices: next-layer rooms reachable from the current room, or —
    // before the first choice (playerLayer < 0) — the entrance (layer 0) rooms.
    std::vector<const SekhemaRoom*> Choices() const {
        std::vector<const SekhemaRoom*> out;
        if (playerLayer < 0) {
            if (!layers.empty())
                for (const auto& r : layers[0]) out.push_back(&r);
            return out;
        }
        const SekhemaRoom* cur = CurrentRoom();
        if (!cur) return out;
        int next = playerLayer + 1;
        if (next < 0 || next >= (int)layers.size()) return out;
        const auto& N = layers[next];
        for (int idx : cur->connections)
            if (idx >= 0 && idx < (int)N.size()) out.push_back(&N[idx]);
        return out;
    }
};

struct SekhemaResources {
    int   honourCurrent = 0, honourMax = 0;
    float honourPct     = 0.0f;
    int   sacredWater   = 0;
    int   keysBronze = 0, keysSilver = 0, keysGold = 0;
    bool  valid = false;
};

// Player build defenses — drives the build-aware affliction scoring.
struct PlayerDefenses {
    int  armour = 0, evasion = 0, energyShield = 0, life = 0;
    bool hasQueenOfTheForest = false;
};

} // namespace sekhema
