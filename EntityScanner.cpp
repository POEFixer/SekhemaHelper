#include "EntityScanner.h"

#include <string>

namespace sekhema {

// ASCII narrowing of a wide entity path for substring matching (paths are ASCII).
static std::string Narrow(const std::wstring& w) {
    std::string s; s.reserve(w.size());
    for (wchar_t c : w) s.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return s;
}
static bool Has(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

// Parse "(Bronze|Silver|Gold)Chest<Content>[123]?" from the last path segment.
// Also matches the untiered SmallChests/BurialChamber{Pot,SmallChest,Urn}_NN
// breakables as content "Burial" (tier 0). Quality digit: 1=base, 2=Superior,
// 3=Prime (kept, drawn as "+" / "++" label suffixes).
static bool ParseChest(const std::string& path, int& tier, int& quality, std::string& content) {
    size_t slash = path.find_last_of('/');
    std::string seg = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const char* tiers[3] = { "Bronze", "Silver", "Gold" };
    for (int t = 0; t < 3; ++t) {
        std::string pref = std::string(tiers[t]) + "Chest";
        if (seg.size() >= pref.size() && seg.compare(0, pref.size(), pref) == 0) {
            tier = t + 1;
            quality = 0;
            content = seg.substr(pref.size());
            if (!content.empty() && content.back() >= '1' && content.back() <= '3') {
                quality = content.back() - '0';
                content.pop_back();
            }
            return true;
        }
    }
    if (seg.rfind("BurialChamber", 0) == 0) {
        tier = 0; quality = 0; content = "Burial";
        return true;
    }
    return false;
}

// Read one shared-state VALUE from a door's StateMachine component via the
// host Sekhema service. The values vector holds one 8-byte value per
// define_shared_state entry, in define order ("activate; open;" ->
// activate=idx0, open=idx1). Out-of-range / unreadable -> false (closed).
static bool ReadDoorState(const PluginSDK::Context* ctx, uintptr_t sm, int stateIdx) {
    if (!sm || stateIdx < 0) return false;
    uint64_t v = 0;
    return ctx->Sekhema.GetStateMachineValue(sm, stateIdx, v) && v != 0;
}

// Sekhema trial doors (PoE2 reuses the PoE1-league sanctum room templates, so
// both object families appear). stateIdx = index of "open" in the door's
// define_shared_state order (see the .ot definitions in the 2026-07-02 spec).
struct DoorDef { const char* sub; DoorKind kind; int stateIdx; const char* name; };
static const DoorDef kDoorDefs[] = {
    { "Sanctum/Objects/SanctumRoomSelectorDoor", DoorKind::Selector,   0, "Selector"   }, // open;wall
    { "Sanctum/Objects/SanctumLogicDoor",        DoorKind::Completion, 1, "Logic"      }, // activate;open
    { "Sanctum/Objects/FloorTransitionDoor",     DoorKind::Transition, 1, "Transition" }, // door_opened;open
    { "KethAscendancy/Objects/KethDoor",         DoorKind::Completion, 0, "Keth"       }, // open
    // Escape-room exit gate — opens when every crystal is activated
    // (SanctumAirlocks.dat mechanic; single shared state).
    { "Objects/SanctumAirlockBlocker",           DoorKind::Completion, 0, "Airlock"    }, // sanctum_completed
};

// Floor-boss path match (monstervarieties, spec table). Floor 3 must exclude
// the Shakari minions (same display name "Ashar") and the ShakariDuo map boss.
static bool IsFloorBossPath(const std::string& path, int floorNum) {
    switch (floorNum) {
    case 1: return Has(path, "Monsters/SaltGolem/SaltGolemBoss");
    case 2: return Has(path, "MarakethSanctumTrial/Boss/SentinelMaceBoss")
              || Has(path, "MarakethSanctumTrial/Boss/SentinelUnarmedBoss");
    case 3: return Has(path, "MarakethSanctumTrial/Boss/Shakari/Shakari")
              && !Has(path, "ShakariMinion") && !Has(path, "ShakariDuo");
    case 4: return Has(path, "Monsters/ApparitionBoss/SilentSpiresApparition");
    default: return false;
    }
}

TrialEntities ScanTrialEntities(const PluginSDK::Context* ctx,
                                float playerGridX, float playerGridY, float maxDist,
                                int floorNum) {
    TrialEntities out;
    if (!ctx) return out;
    const float maxDistSq = maxDist * maxDist;

    ctx->Entities.Enumerate([&](const PluginSDK::Entity& e) -> bool {
        if (!e.IsValid) return true;
        std::string path = Narrow(e.Path);
        if (path.empty()) return true;

        TrialMarker m;
        m.gridX = e.GridPositionX; m.gridY = e.GridPositionY; m.worldZ = e.TerrainHeight;
        m.entityId = e.Id;

        // Doors and bosses are exempt from the radius cap: doors seal the
        // auto-detected room region (closed ones must be seen even a bit far)
        // and bosses drive the run tracker. Both are rare entities.
        for (const auto& dd : kDoorDefs) {
            if (Has(path, dd.sub)) {
                TrialDoor td;
                td.entityId = e.Id; td.kind = dd.kind; td.debugName = dd.name;
                td.gridX = m.gridX; td.gridY = m.gridY;
                td.open = ReadDoorState(ctx, e.Components.StateMachine, dd.stateIdx);
                out.doors.push_back(td);
                return true;
            }
        }

        if (floorNum >= 1 && floorNum <= 4 && IsFloorBossPath(path, floorNum)) {
            TrialBoss b;
            b.entityId = e.Id; b.hp = e.CurrentHP; b.maxHp = e.MaxHP;
            b.alive = e.IsValid && e.CurrentHP > 0;
            out.bosses.push_back(b);
            return true;
        }

        // Coarse sanity radius for the rest (the RoomRegion filter in the glue
        // does the real per-room cut).
        if (maxDist > 0.0f) {
            float dx = m.gridX - playerGridX, dy = m.gridY - playerGridY;
            if (dx * dx + dy * dy > maxDistSq) return true;
        }

        auto used = [&]() -> bool {
            // -1 (unreadable) counts as NOT used, like the old raw read's
            // zero-on-fail — a marker is only dropped on a positive "used".
            uintptr_t sm = e.Components.StateMachine;
            return sm && ctx->Sekhema.GetRoomUsedFlag(sm) == 1;
        };

        if (Has(path, "Hazards/HourglassLethal")) {
            m.type = MarkerType::Crystal; m.active = !used();
            if (m.active) out.crystals.push_back(m);
        } else if (Has(path, "/MarakethSanctum/")) {
            int tier = 0, quality = 0; std::string content;
            if (ParseChest(path, tier, quality, content)) {
                bool opened = e.Components.Chest
                              && ctx->Components.ReadChest(e.Components.Chest).IsOpened;
                if (!opened) {
                    m.type = MarkerType::Chest; m.chestTier = tier;
                    m.quality = quality; m.label = content;
                    out.chests.push_back(m);
                }
            }
        } else if (Has(path, "MarakethSanctumTrial/Hazards/PortalPlatform")) {
            m.type = MarkerType::Portal; m.active = !used();
            if (m.active) out.portals.push_back(m);
        } else if (Has(path, "Sanctum/Objects/SanctumGenericLever")) {
            m.type = MarkerType::Lever; m.active = !used();
            if (m.active) out.levers.push_back(m);
        }
        return true;
    });

    return out;
}

} // namespace sekhema
