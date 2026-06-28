#include "EntityScanner.h"
#include "MemoryLayout.h"

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
static bool ParseChest(const std::string& path, int& tier, std::string& content) {
    size_t slash = path.find_last_of('/');
    std::string seg = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const char* tiers[3] = { "Bronze", "Silver", "Gold" };
    for (int t = 0; t < 3; ++t) {
        std::string pref = std::string(tiers[t]) + "Chest";
        if (seg.size() >= pref.size() && seg.compare(0, pref.size(), pref) == 0) {
            tier = t + 1;
            content = seg.substr(pref.size());
            if (!content.empty() && content.back() >= '1' && content.back() <= '3')
                content.pop_back();  // strip trailing quality digit
            return true;
        }
    }
    return false;
}

TrialEntities ScanTrialEntities(const PluginSDK::Context* ctx,
                                float playerGridX, float playerGridY, float maxDist) {
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

        // Drop objects on other floors (all floors share one big map).
        if (maxDist > 0.0f) {
            float dx = m.gridX - playerGridX, dy = m.gridY - playerGridY;
            if (dx * dx + dy * dy > maxDistSq) return true;
        }

        auto used = [&]() -> bool {
            uintptr_t sm = e.Components.StateMachine;
            if (!sm) return false;
            uint8_t b = 0;
            ctx->Memory.Read(sm + layout::StateMachine_UsedByte, &b, sizeof(b));
            return b != 0;
        };

        if (Has(path, "Hazards/HourglassLethal")) {
            m.type = MarkerType::Crystal; m.active = !used();
            if (m.active) out.crystals.push_back(m);
        } else if (Has(path, "/MarakethSanctum/")) {
            int tier = 0; std::string content;
            if (ParseChest(path, tier, content)) {
                bool opened = e.Components.Chest
                              && ctx->Components.ReadChest(e.Components.Chest).IsOpened;
                if (!opened) {
                    m.type = MarkerType::Chest; m.chestTier = tier; m.label = content;
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
