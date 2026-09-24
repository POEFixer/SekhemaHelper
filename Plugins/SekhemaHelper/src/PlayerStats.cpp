#include "PlayerStats.h"
#include "MemoryLayout.h"

namespace sekhema {

PlayerDefenses ReadDefenses(const PluginSDK::Context* ctx) {
    PlayerDefenses d;
    if (!ctx) return d;

    PluginSDK::Entity player = ctx->Entities.GetPlayer();
    uintptr_t statsAddr = player.Components.Stats;
    if (!statsAddr) return d;

    // Stat-dict keys are 1-based (spec Appendix C). A stat may appear in both the
    // items and buff/action dicts, so accumulate.
    auto stats = ctx->Components.EnumerateStats(statsAddr);
    for (const auto& s : stats) {
        switch (s.Key) {
            case layout::Stat_Armour:              d.armour       += s.Value; break;
            case layout::Stat_MaximumLife:         d.life         += s.Value; break;
            case layout::Stat_MaximumEnergyShield: d.energyShield += s.Value; break;
            case layout::Stat_EvasionRating:       d.evasion      += s.Value; break;
            case layout::Stat_QueenOfTheForest:    if (s.Value > 0) d.hasQueenOfTheForest = true; break;
            default: break;
        }
    }
    return d;
}

} // namespace sekhema
