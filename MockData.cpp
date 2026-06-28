#include "MockData.h"

namespace sekhema {

SekhemaFloor MakeMockFloor() {
    SekhemaFloor f;
    f.valid = true; f.playerLayer = 0; f.playerRoom = 0;

    SekhemaRoom start; start.roomType = "Chalice"; start.connections = {0, 1, 2};
    f.layers.push_back({start});

    SekhemaRoom arena;  arena.roomType  = "Hourglass"; arena.reward  = "Boon";       arena.rewardValue = 200; arena.connections = {0};
    SekhemaRoom lair;   lair.roomType   = "Chalice";   lair.reward   = "Honour";     lair.rewardValue  = 50;  lair.affliction = "Death Toll";    lair.connections = {0};
    SekhemaRoom escape; escape.roomType = "Escape";    escape.reward = "Silver Key";                          escape.affliction = "Iron Manacles"; escape.connections = {1};
    f.layers.push_back({arena, lair, escape});

    SekhemaRoom mid0; mid0.roomType = "Ritual";   mid0.reward = "Large Fountain"; mid0.rewardValue = 100; mid0.connections = {0};
    SekhemaRoom mid1; mid1.roomType = "Gauntlet"; mid1.affliction = "Glass Shard";                        mid1.connections = {0};
    f.layers.push_back({mid0, mid1});

    SekhemaRoom boss; boss.roomType = "Boss";
    f.layers.push_back({boss});
    return f;
}

SekhemaResources MakeMockResources() {
    SekhemaResources r;
    r.valid = true;
    r.honourCurrent = 812; r.honourMax = 1000;
    r.honourPct = 100.0f * (float)r.honourCurrent / (float)r.honourMax;
    r.sacredWater = 240;
    r.keysBronze = 2; r.keysSilver = 1; r.keysGold = 0;
    return r;
}

// Armour-leaning build so the build-aware risk is visible: armour curses look
// severe, evasion curses look mild.
PlayerDefenses MakeMockDefenses() {
    PlayerDefenses d;
    d.armour = 6000; d.evasion = 400; d.energyShield = 0; d.life = 4500;
    d.hasQueenOfTheForest = false;
    return d;
}

} // namespace sekhema
