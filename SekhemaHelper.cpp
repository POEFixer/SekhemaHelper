// SekhemaHelper — Trial of the Sekhemas helper (SDK v6).
// Phase 3: real memory reads (graceful fallback off-Trial).
#include "sdk/PluginSDK.h"
#include "Settings.h"
#include "Engine.h"
#include "SekhemaModel.h"
#include "ResourceReader.h"
#include "PlayerStats.h"
#include "MemReader.h"
#include "MemoryLayout.h"
#include "DashboardUI.h"
#include "MapOverlay.h"
#include "EntityScanner.h"
#include "LargeMapMarkers.h"
#include "SettingsUI.h"
#include <imgui.h>
#include <Windows.h>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace sekhema {

// Heap-pointer sanity window (PoE2 UI elements live here; module code is 0x7FF…).
static constexpr uintptr_t kHeapLo = 0x10000000000ull;
static constexpr uintptr_t kHeapHi = 0x7FF000000000ull;
static bool LooksHeap(uintptr_t a) { return a >= kHeapLo && a < kHeapHi; }

class SekhemaHelperPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "SekhemaHelper"; }
    bool        WantsOverlay() const override { return true; }

    void OnEnable(bool /*isGameAttached*/) override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        m_settings.Load(DirectoryPath());
        ctx()->Log.Info("SekhemaHelper enabled");
    }

    void OnDisable() override {
        m_settings.Save(DirectoryPath());
        ctx()->Log.Info("SekhemaHelper disabled");
    }

    void DrawSettings() override { DrawSettingsPanel(m_settings); }
    void SaveSettings() override { m_settings.Save(DirectoryPath()); }

    void DrawUI() override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        // Hotkey toggle (rising-edge), only when the game window is foreground.
        if (ctx()->Game.IsForeground() && m_settings.toggleVk) {
            bool down = (GetAsyncKeyState(m_settings.toggleVk) & 0x8000) != 0;
            if (down && !m_hotkeyDown) m_settings.dashboardVisible = !m_settings.dashboardVisible;
            m_hotkeyDown = down;
        }

        // Throttle the heavy memory work (FloorData walk + classification, resource
        // /stat reads, entity scan) to ~7 Hz and cache it; the trial state changes
        // slowly. Rendering runs every frame from the cache, so map-projected
        // overlays still track panning/zoom (projection is cheap, stays per-frame).
        DWORD now = GetTickCount();
        if (now - m_lastReadTick >= kReadIntervalMs) {
            m_lastReadTick = now;
            SekhemaFloor floor;
            SekhemaResources res;
            TrialEntities ents;
            CrystalRoute route;
            uintptr_t panel = 0;
            if (ctx()->Game.IsInGame()) {
                // Re-arm per-area panel discovery on every area transition so a
                // fresh trial is found promptly, while a non-trial area is probed
                // only once (see FindTrialPanel). Cheap counter — no entity scan.
                uint64_t ac = ctx()->Game.GetAreaChangeCounter();
                if (ac != m_lastAreaChange) {
                    m_lastAreaChange = ac;
                    m_discoverAttempts = 0;
                    m_noTrialThisArea = false;
                }
                panel = FindTrialPanel(floor);
                if (floor.valid) {
                    res = ReadResources(ctx(), panel);
                    PlayerDefenses def = ReadDefenses(ctx());
                    if (const WeightProfile* prof = m_settings.ActiveProfile())
                        Evaluate(floor, *prof, def, res);
                    PluginSDK::Entity pl = ctx()->Entities.GetPlayer();
                    ents = ScanTrialEntities(ctx(), pl.GridPositionX, pl.GridPositionY,
                                             m_settings.roomRadius);
                    if (!ents.crystals.empty()) {
                        route = ComputeCrystalRoute(ents.crystals,
                                                    static_cast<int>(pl.GridPositionX),
                                                    static_cast<int>(pl.GridPositionY), ctx());
                    }
                }
            }
            m_floor = std::move(floor);
            m_res   = res;
            m_ents  = std::move(ents);
            m_route = std::move(route);
            m_panel = panel;
        }

        if (m_floor.valid) {
            DrawMapOverlay(m_floor, m_settings, ctx(), m_panel);
            DrawLargeMapMarkers(m_ents, m_route, m_settings, ctx());
        }
        DrawDashboard(m_floor, m_res, m_settings);
    }

private:
    Settings         m_settings;
    bool             m_hotkeyDown = false;
    std::vector<int> m_cachedPath;     // discovered index path from the top UI root
    // Per-area trial-panel discovery gate. The self-healing BFS walks the game UI
    // tree (RPM-heavy) and finds NOTHING in a non-trial map, so it must not run
    // every frame there. Run it at most once per area (after a short settle), then
    // stand down until the next area change. Real trials resolve via the cheap
    // cached/primary path with no BFS at all.
    uint64_t         m_lastAreaChange = 0;
    int              m_discoverAttempts = 0;   // fast-path misses since area change
    bool             m_noTrialThisArea = false;
    static constexpr int kBfsAttemptAt = 6;    // run the one BFS after N misses (~0.9s @ 7Hz)

    // Throttled-read cache (see DrawUI).
    SekhemaFloor     m_floor;
    SekhemaResources m_res;
    TrialEntities    m_ents;
    CrystalRoute     m_route;
    uintptr_t        m_panel = 0;
    DWORD            m_lastReadTick = 0;
    static constexpr DWORD kReadIntervalMs = 150;

    // Walk up from a node to the top heap UI root via Parent (+0xB8). The
    // SDK-exposed roots are the HUD (get_game_ui_root) and the inventory panel
    // (get_ui_root); the trial panel lives under the top root above both.
    uintptr_t WalkUpToRoot(uintptr_t node) {
        for (int i = 0; i < 24 && LooksHeap(node); ++i) {
            uintptr_t p = ctx()->Ui.Read(node).ParentAddress;
            if (!LooksHeap(p) || p == node) break;
            node = p;
        }
        return node;
    }

    // Cheap: does node+0x3B8 resolve a FloorData with a [1,64] layer vector?
    int ProbeFloor(const Mem& mem, uintptr_t node) {
        if (!node) return 0;
        uintptr_t fo = mem.Ptr(node + layout::MapElement_FloorObjPtr);
        if (!fo) return 0;
        uint8_t flag = mem.Read<uint8_t>(fo + layout::FloorObj_Flag);
        int bases[2] = {
            flag ? layout::FloorData_OffActive : layout::FloorData_OffAlt,
            flag ? layout::FloorData_OffAlt : layout::FloorData_OffActive
        };
        for (int b : bases) {
            int lc = VecCount(mem.ReadVec(fo + b + layout::FloorData_Layers), layout::LayerStride);
            if (lc > 0 && lc <= layout::MaxLayers) return lc;
        }
        return 0;
    }

    // BFS from `root` for the first node that resolves a *classified* trial floor
    // (ProbeFloor pre-filter, then a full SekhemaReader::Read to reject
    // coincidental matches). Returns the panel address + its index path.
    uintptr_t BfsFindPanel(uintptr_t root, std::vector<int>& outPath, SekhemaFloor& outFloor) {
        if (!LooksHeap(root)) return 0;
        Mem mem(ctx());
        struct N { uintptr_t a; std::vector<int> p; int d; };
        std::queue<N> q;
        q.push({root, {}, 0});
        int visited = 0;
        while (!q.empty() && visited < 60000) {
            N n = std::move(q.front()); q.pop(); ++visited;
            if (ProbeFloor(mem, n.a) > 0) {
                SekhemaFloor f = SekhemaReader::Read(n.a, ctx());
                if (f.valid) { outPath = n.p; outFloor = std::move(f); return n.a; }
            }
            if (n.d >= 6) continue;   // panel sits at depth ~2; cap deep walks in non-trial maps
            int cc = ctx()->Ui.Read(n.a).ChildCount;
            if (cc < 0) cc = 0;
            if (cc > 512) cc = 512;
            for (int i = 0; i < cc; ++i) {
                uintptr_t c = ctx()->Ui.GetChildAt(n.a, i);
                if (LooksHeap(c)) {
                    std::vector<int> np = n.p; np.push_back(i);
                    q.push({c, std::move(np), n.d + 1});
                }
            }
        }
        return 0;
    }

    // Resolve the trial panel from the top UI root: cached path -> known [1,84]
    // -> throttled BFS discovery. Fills outFloor with the valid graph.
    uintptr_t FindTrialPanel(SekhemaFloor& outFloor) {
        // Stood down for this area: a full BFS already failed, so there is no trial
        // panel here. Skip all probing until the next area change re-arms us. This
        // is what keeps a non-trial map from re-walking the UI tree every frame.
        if (m_noTrialThisArea) return 0;

        uintptr_t root = WalkUpToRoot(ctx()->Ui.GetUiRoot());
        if (!LooksHeap(root)) return 0;

        if (!m_cachedPath.empty()) {
            uintptr_t panel = ctx()->Ui.FollowPath(root, m_cachedPath.data(),
                                                   static_cast<int>(m_cachedPath.size()));
            SekhemaFloor f = SekhemaReader::Read(panel, ctx());
            if (f.valid) { m_discoverAttempts = 0; outFloor = std::move(f); return panel; }
        }

        // CE-confirmed primary path (top root -> child[1] -> child[84] = panel).
        static const int kPrimary[] = {1, 84};
        uintptr_t panel = ctx()->Ui.FollowPath(root, kPrimary, 2);
        SekhemaFloor f = SekhemaReader::Read(panel, ctx());
        if (f.valid) { m_cachedPath.assign(kPrimary, kPrimary + 2); m_discoverAttempts = 0; outFloor = std::move(f); return panel; }

        // Fast paths failed. The fallback BFS walks the whole game UI tree, which is
        // RPM-heavy and finds nothing in a non-trial map — so run it at most ONCE
        // per area, after a short settle (lets the UI finish loading on area entry),
        // then stand down. Real trials never reach here (primary path resolves).
        if (++m_discoverAttempts == kBfsAttemptAt) {
            std::vector<int> found;
            SekhemaFloor bf;
            uintptr_t bpanel = BfsFindPanel(root, found, bf);
            if (bpanel) { m_cachedPath = std::move(found); outFloor = std::move(bf); return bpanel; }
            m_noTrialThisArea = true;   // one BFS, nothing here — stop until area change
        }
        return 0;
    }

};

} // namespace sekhema

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin()              { return new sekhema::SekhemaHelperPlugin(); }
extern "C" PLUGIN_API void               DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
