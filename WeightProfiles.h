#pragma once
#include <map>
#include <string>
#include <vector>

namespace sekhema {

struct WeightProfile {
    std::string name;
    std::map<std::string, float> roomTypeWeights;
    std::map<std::string, float> afflictionWeights;
    std::map<std::string, float> rewardWeights;
    float avoidMerchantBelowWater = 120.0f;
    float avoidHonourAbovePct      = 75.0f;
};

// Stat-removal curses scored dynamically by WeightCalculator (Phase 2); 0 here.
inline WeightProfile MakeDefaultProfile() {
    WeightProfile p;
    p.name = "Default";
    p.roomTypeWeights = {
        {"Gauntlet", -1000.0f}, {"Hourglass", -200.0f}, {"Chalice", 0.0f},
        {"Ritual", 0.0f}, {"Escape", 100.0f}, {"Boss", 0.0f},
    };
    p.afflictionWeights = {
        {"Glass Shard", -4000.0f}, {"Ghastly Scythe", -4000.0f}, {"Veiled Sight", -4000.0f},
        {"Myriad Aspersions", -4000.0f}, {"Deceptive Mirror", -4000.0f}, {"Purple Smoke", -4000.0f},
        {"Red Smoke", -4000.0f}, {"Black Smoke", -4000.0f},
        {"Rapid Quicksand", -1000.0f}, {"Deadly Snare", -1000.0f}, {"Forgotten Traditions", -1000.0f},
        {"Season of Famine", -1000.0f}, {"Orb of Negation", -1000.0f}, {"Winter Drought", -1000.0f},
        {"Branded Balbalakh", -1000.0f}, {"Chiselled Stone", -1000.0f}, {"Untouchable", -1000.0f},
        {"Blunt Sword", -1000.0f}, {"Spiked Shell", -1000.0f}, {"Unassuming Brick", -1000.0f},
        {"Costly Aid", -900.0f}, {"Tradition's Demand", -800.0f}, {"Hungry Fangs", -600.0f},
        {"Death Toll", -400.0f}, {"Worn Sandals", -400.0f}, {"Golden Smoke", -400.0f}, {"Fiendish Wings", -400.0f},
        {"Trade Tariff", -300.0f}, {"Spiked Exit", -300.0f},
        {"Suspected Sympathiser", -200.0f}, {"Unquenched Thirst", -200.0f},
        {"Weakened Flesh", -100.0f}, {"Haemorrhage", -100.0f}, {"Gate Toll", -100.0f},
        {"Leaking Waterskin", -100.0f}, {"Low Rivers", -100.0f},
        // dynamic (build-aware) — 0 in table:
        {"Corrosive Concoction", 0.0f}, {"Iron Manacles", 0.0f}, {"Shattered Shield", 0.0f},
        {"Sharpened Arrowhead", 0.0f}, {"Rusted Mallet", 0.0f}, {"Chains of Binding", 0.0f},
        {"Dishonoured Tattoo", 0.0f}, {"Tattered Blindfold", 0.0f}, {"Dark Pit", 0.0f},
        {"Honed Claws", 0.0f}, {"Exhausted Wells", 0.0f}, {"Orbala's Leathers", 0.0f},
    };
    p.rewardWeights = {
        {"Large Fountain", 100.0f}, {"Fountain", 50.0f}, {"Honour", 50.0f}, {"Honour Galai", 300.0f},
        {"Boon", 200.0f}, {"Merchant", 20.0f}, {"Pledge to Kochai", 20.0f},
    };
    return p;
}

inline WeightProfile MakeNoHitProfile() {
    WeightProfile p = MakeDefaultProfile();
    p.name = "No-Hit";
    p.roomTypeWeights["Hourglass"] = -1000.0f;
    p.roomTypeWeights["Gauntlet"]  = -200.0f;
    p.afflictionWeights["Death Toll"]        = -500000.0f;
    p.afflictionWeights["Spiked Exit"]       = -600000.0f;
    p.afflictionWeights["Deceptive Mirror"]  = -400000.0f;
    p.afflictionWeights["Glass Shard"]       = -50000.0f;
    p.afflictionWeights["Myriad Aspersions"] = -50000.0f;
    return p;
}

inline std::vector<WeightProfile> DefaultProfiles() {
    return { MakeDefaultProfile(), MakeNoHitProfile() };
}

} // namespace sekhema
