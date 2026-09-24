#include "WeightCalculator.h"
#include <algorithm>
#include <optional>

namespace sekhema {

static double TableWeight(const std::map<std::string,float>& m, const std::string& key) {
    auto it = m.find(key);
    return it == m.end() ? 0.0 : (double)it->second;
}

double ArmourRelevance(const PlayerDefenses& d) {
    double pool = std::max(double(d.armour + d.evasion), 4000.0);
    return double(d.armour) / pool;
}
double EvasionRelevance(const PlayerDefenses& d) {
    double pool = std::max(double(d.armour + d.evasion), 4000.0);
    return double(d.evasion) / pool;
}
double EnergyShieldRelevance(const PlayerDefenses& d) {
    double pool = std::max(double(d.energyShield + d.life), 4000.0);
    return double(d.energyShield) / pool;
}

double RoomTypeWeight(const std::string& roomType, const WeightProfile& p) {
    return TableWeight(p.roomTypeWeights, roomType);
}

// Returns a dynamic (build-aware) weight for the stat-removal curses, or
// std::nullopt to fall back to the table value.
static std::optional<double> DynamicAffliction(const std::string& name,
                                                const PlayerDefenses& d) {
    const double K = 5000.0;
    if (name == "Sharpened Arrowhead") return -K * ArmourRelevance(d);
    if (name == "Iron Manacles")       return -K * EvasionRelevance(d);
    if (name == "Shattered Shield")    return -K * EnergyShieldRelevance(d) * 0.5;
    if (name == "Corrosive Concoction")
        return -K * (ArmourRelevance(d) + EvasionRelevance(d)) - K * EnergyShieldRelevance(d);
    if (name == "Worn Sandals")        return d.hasQueenOfTheForest ? std::optional<double>(0.0)
                                                                    : std::nullopt;
    return std::nullopt;
}

double AfflictionWeight(const std::string& name, const WeightProfile& p, const PlayerDefenses& d) {
    if (name.empty()) return 0.0;
    if (auto dyn = DynamicAffliction(name, d)) return *dyn;
    return TableWeight(p.afflictionWeights, name);
}

bool IsBuildAwareAffliction(const std::string& name) {
    return name == "Sharpened Arrowhead" || name == "Iron Manacles" ||
           name == "Shattered Shield"    || name == "Corrosive Concoction" ||
           name == "Worn Sandals";
}

double RewardWeight(const std::string& reward, const WeightProfile& p, const SekhemaResources& r) {
    double base = TableWeight(p.rewardWeights, reward);
    if (!r.valid) return base;
    const double SUPPRESS = 300.0; // soft penalty, not a ban
    auto contains = [&](const char* s){ return reward.find(s) != std::string::npos; };
    if (contains("Merchant") && r.sacredWater < (int)p.avoidMerchantBelowWater) base -= SUPPRESS;
    if (contains("Honour")   && r.honourPct  > p.avoidHonourAbovePct)           base -= SUPPRESS;
    return base;
}

double ScoreRoom(const SekhemaRoom& room, const WeightProfile& p,
                 const PlayerDefenses& d, const SekhemaResources& r) {
    double s = kRoomScoreBase;
    s += RoomTypeWeight(room.roomType, p);
    s += AfflictionWeight(room.affliction, p, d);
    s += RewardWeight(room.reward, p, r);
    return s;
}

Risk RiskFor(double afflictionWeight, bool hasAffliction) {
    if (!hasAffliction || afflictionWeight >= 0.0) return Risk::None;
    if (afflictionWeight <= -2000.0) return Risk::Severe;
    if (afflictionWeight <= -300.0)  return Risk::Moderate;
    return Risk::Minor;
}

} // namespace sekhema
