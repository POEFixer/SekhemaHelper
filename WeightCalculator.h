#pragma once
#include "Model.h"
#include "WeightProfiles.h"
#include <string>

namespace sekhema {

double ArmourRelevance(const PlayerDefenses& d);
double EvasionRelevance(const PlayerDefenses& d);
double EnergyShieldRelevance(const PlayerDefenses& d);

double RoomTypeWeight(const std::string& roomType, const WeightProfile& p);
double AfflictionWeight(const std::string& name, const WeightProfile& p, const PlayerDefenses& d);
double RewardWeight(const std::string& reward, const WeightProfile& p, const SekhemaResources& r);

double ScoreRoom(const SekhemaRoom& room, const WeightProfile& p,
                 const PlayerDefenses& d, const SekhemaResources& r);

// Map a (build-aware) affliction weight to a chip severity. hasAffliction=false
// always yields Risk::None.
Risk RiskFor(double afflictionWeight, bool hasAffliction);

} // namespace sekhema
