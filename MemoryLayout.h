#pragma once
// MemoryLayout.h — the plugin's remaining game-data constants. The raw memory
// offsets that used to live here (FloorData walk, DAT FK fields, StateMachine
// bytes, UI-leaf StringId) moved host-side: GameLibrary offset headers read via
// ctx->Sekhema / ctx->Ui (core/sekhema/SekhemaTrialReader). On a client patch,
// fix offsets THERE — the host reader keeps the fail-closed bounds. What stays
// plugin-side is pure policy: the player stat dictionary keys the build-aware
// affliction scoring matches against ctx->Entities stat enumeration.

namespace sekhema {
namespace layout {

// ── Player stat dictionary keys (1-based; spec Appendix C) ────────────────────
constexpr int Stat_Armour              = 235;
constexpr int Stat_MaximumLife         = 239;
constexpr int Stat_MaximumEnergyShield = 241;
constexpr int Stat_EvasionRating       = 276;
constexpr int Stat_QueenOfTheForest    = 9490;

} // namespace layout
} // namespace sekhema
