#pragma once
// MemoryLayout.h — ALL version-specific offsets for the SekhemaHelper reader in
// one place (the seam). Values are PoE2 ~0.5.x, validated by SekhemaHelper's
// author (2026-06-20) + cross-checked vs Gordin/GameHelper2 (see spec
// Appendix A/C). On a client patch, fix offsets HERE; the fail-closed bounds +
// VecCount divisibility guard keep a drifted read from crashing the host.
//
// This header is pure (no SDK / no Windows) so VecCount stays standalone-testable.
#include <cstdint>

namespace sekhema {

// A std::vector's control block reduces to its First/Last pointers for our reads.
struct StdVec { uintptr_t First = 0; uintptr_t Last = 0; };

namespace layout {

// ── FloorData resolution (from the SekhemasTrialMapPanel UI element) ──────────
constexpr int MapElement_FloorObjPtr = 0x3B8;  // panel -> floorObj
constexpr int FloorObj_Flag          = 0x25A;  // byte: selects active FloorData base
constexpr int FloorData_OffActive    = 0x1F8;  // FloorData = floorObj + this  (flag != 0)
constexpr int FloorData_OffAlt       = 0x1B0;  //                          ... (flag == 0 / fallback)
constexpr int UiElement_ParentPtr    = 0xB8;   // panel parent fallback

// ── FloorData layout ──────────────────────────────────────────────────────────
constexpr int FloorData_Layers  = 0x00;  // StdVec, element stride 0x20 (inline)
constexpr int FloorData_Content = 0x18;  // StdVec, element stride 0x40 (inline)
constexpr int FloorData_Choices = 0x38;  // byte[layerCount]; 0xFF = none
constexpr int FloorData_Counter = 0x40;  // byte; (Counter & 7) = choices made

constexpr int LayerStride = 0x20;
constexpr int Layer_Rooms = 0x00;        // StdVec<RoomStruct>, stride 0x38

constexpr int RoomStride     = 0x38;
constexpr int Room_ConnFirst = 0x00;     // StdVec<uint8_t> First (next-layer indices)
constexpr int Room_ConnLast  = 0x08;     // StdVec<uint8_t> Last

// ── Content vector entry (stride 0x40) ────────────────────────────────────────
constexpr int Content_Stride   = 0x40;
constexpr int Content_Layer    = 0x00;   // uint8_t -> graph layer
constexpr int Content_RoomIdx  = 0x01;   // uint8_t -> graph room index
constexpr int Content_FkRow0   = 0x08;   // pair k: row   @ +0x08 + k*0x10
constexpr int Content_FkTable0 = 0x10;   // pair k: table @ +0x10 + k*0x10
constexpr int Content_FkStride = 0x10;
constexpr int Content_FkCount  = 3;

// ── DAT FK field offsets (row/table are pointers to DAT objects) ──────────────
constexpr int DatTable_PathPtr = 0x08;   // table+0x08 -> ptr -> UTF-16 table path
constexpr int DatRow_IdPtr     = 0x00;   // row+0x00   -> ptr -> UTF-16 row Id   (SanctumRooms)
constexpr int DatRow_NamePtr   = 0x28;   // row+0x28   -> ptr -> UTF-16 name     (SanctumPersistentEffects)

// ── UI leaf (SekhemaHelper's OWN offset, not a GH2 field) ─────────────────────
constexpr int UiLeaf_TextWString = 0x4C0; // std::wstring (MSVC SSO) of a leaf's rendered number

// ── Component (SekhemaHelper's OWN raw read; first byte past the 0x10 header) ──
constexpr int StateMachine_UsedByte = 0x10; // 0 = active/uncollected, !=0 = used/collected/closed

// ── Player stat dictionary keys (1-based; spec Appendix C) ────────────────────
constexpr int Stat_Armour              = 235;
constexpr int Stat_MaximumLife         = 239;
constexpr int Stat_MaximumEnergyShield = 241;
constexpr int Stat_EvasionRating       = 276;
constexpr int Stat_QueenOfTheForest    = 9490;

// ── Fail-closed bounds (reproduce the original's guards) ──────────────────────
constexpr int MaxLayers  = 64;
constexpr int MaxRooms   = 64;
constexpr int MaxConn    = 16;
constexpr int MaxContent = 512;

} // namespace layout

// Element count of a vector given its element stride. Requires the byte span to
// be a positive exact multiple of the stride (the original's guard) and bounds
// the result — a drifted/garbage read returns 0 instead of a wild count.
inline int VecCount(const StdVec& v, int stride) {
    if (!v.First || !v.Last || stride <= 0) return 0;
    long long bytes = static_cast<long long>(v.Last) - static_cast<long long>(v.First);
    if (bytes <= 0 || (bytes % stride) != 0) return 0;
    long long c = bytes / stride;
    return (c > 0 && c <= 4096) ? static_cast<int>(c) : 0;
}

} // namespace sekhema
