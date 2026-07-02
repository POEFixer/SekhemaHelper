// SekhemaHelper — Trial of the Sekhemas helper (SDK v6).
//
// Threading model (perf rework 2026-07-02): ALL heavy reads — FloorData walk +
// classification, entity scan, room-region flood, crystal-route planning —
// run on the plugin's own background worker at ~7 Hz and publish an immutable
// TickSnapshot. DrawUI (render thread) only consumes the newest snapshot,
// ticks the run tracker on it, and draws — the per-frame render cost is
// microseconds (perf capture showed 13-18 ms p99 render spikes when this work
// lived in DrawUI). Bridge calls are thread-safe (SEH-wrapped RPM / host
// snapshot copies); ImGui stays render-only.
#include "sdk/PluginSDK.h"
#include "Settings.h"
#include "Engine.h"
#include "RoomClassifier.h"
#include "SekhemaModel.h"
#include "ResourceReader.h"
#include "PlayerStats.h"
#include "MemReader.h"
#include "MemoryLayout.h"
#include "DashboardUI.h"
#include "MapOverlay.h"
#include "EntityScanner.h"
#include "LargeMapMarkers.h"
#include "RunTracker.h"
#include "RunDatabase.h"
#include "TimerOverlay.h"
#include "SettingsUI.h"
#include <imgui.h>
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sekhema {

// Heap-pointer sanity window (PoE2 UI elements live here; module code is 0x7FF…).
static constexpr uintptr_t kHeapLo = 0x10000000000ull;
static constexpr uintptr_t kHeapHi = 0x7FF000000000ull;
static bool LooksHeap(uintptr_t a) { return a >= kHeapLo && a < kHeapHi; }

// Wall-clock epoch ms — the tracker/DB timestamp base (system_clock, not ticks,
// so history rows carry real dates).
static uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

static std::string NarrowStr(const std::wstring& w) {
    std::string s; s.reserve(w.size());
    for (wchar_t c : w) s.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return s;
}

// One worker iteration's published state. The render thread copies it once per
// new sequence number and draws from its own copy every frame.
struct TickSnapshot {
    uint64_t seq = 0;
    uint64_t nowMs = 0;
    bool     inGame = false;
    uint64_t areaChangeCounter = 0;
    bool     trialAbsent = false;
    SekhemaFloor     floor;
    SekhemaResources res;
    TrialEntities    ents;      // region-filtered
    CrystalRoute     route;
    uintptr_t panel = 0;
    bool      panelVisible = false;
    int       playerHp = -1;
    int       areaLevel = 0;
    int       pgx = 0, pgy = 0;
    std::string charName;
};

class SekhemaHelperPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "SekhemaHelper"; }
    bool        WantsOverlay() const override { return true; }

    void OnEnable(bool /*isGameAttached*/) override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        m_settings.Load(DirectoryPath());
        if (m_db.Open(DirectoryPath()))
            m_db.CloseDanglingRuns();   // crash/restart leftovers -> abandoned
        else
            ctx()->Log.Warn("SekhemaHelper: run history DB failed to open");
        StartWorker();
        ctx()->Log.Info("SekhemaHelper enabled");
    }

    void OnDisable() override {
        StopWorker();
        m_settings.Save(DirectoryPath());
        m_db.Close();
        ctx()->Log.Info("SekhemaHelper disabled");
    }

    void DrawSettings() override {
        std::lock_guard<std::mutex> lk(m_settingsMutex);   // worker copies the profile
        DrawSettingsPanel(m_settings, &m_db, &m_hist);
    }
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

        // Consume the newest worker snapshot (copy is small: one floor graph +
        // a few marker vectors, at ~7 Hz). Tracker/DB run on the render thread
        // against the copy, so sqlite and ImGui stay single-threaded.
        bool fresh = false;
        {
            std::lock_guard<std::mutex> lk(m_snapMutex);
            if (m_snap.seq != m_lastConsumedSeq) {
                m_lastConsumedSeq = m_snap.seq;
                m_view = m_snap;
                fresh = true;
            }
        }
        if (fresh) {
            m_floor = m_view.floor;
            m_res   = m_view.res;
            m_ents  = m_view.ents;
            m_route = m_view.route;
            m_panel = m_view.panel;
            TickRunTracker(m_view);
        }

        if (m_floor.valid) {
            DrawMapOverlay(m_floor, m_settings, ctx(), m_panel);
            DrawLargeMapMarkers(m_ents, m_route, m_settings, ctx());
        }
        DrawDashboard(m_floor, m_res, m_settings);
        DrawTimerOverlay(m_tracker.GetOverlay(), m_settings, NowMs());
    }

private:
    // ── shared state ─────────────────────────────────────────────────────────
    Settings         m_settings;
    std::mutex       m_settingsMutex;     // guards profile edits vs the worker's copy
    bool             m_hotkeyDown = false;

    std::mutex   m_snapMutex;
    TickSnapshot m_snap;                  // latest published (worker -> render)
    uint64_t     m_snapSeq = 0;           // worker-side counter

    // ── render-thread state ──────────────────────────────────────────────────
    TickSnapshot     m_view;              // last consumed snapshot (render copy)
    uint64_t         m_lastConsumedSeq = 0;
    SekhemaFloor     m_floor;
    SekhemaResources m_res;
    TrialEntities    m_ents;
    CrystalRoute     m_route;
    uintptr_t        m_panel = 0;

    RunTracker  m_tracker;
    RunDatabase m_db;
    HistoryUIState m_hist;
    int64_t     m_runId = -1, m_floorId = -1, m_roomId = -1;
    std::unordered_map<uint32_t, bool> m_dbgDoorState, m_dbgBossAlive;   // change-only debug dumps

    // Player position at the current room's start (written on render from
    // tracker events, read on the worker for exit-door disambiguation).
    std::atomic<int> m_roomEntryGX{0}, m_roomEntryGY{0};

    // ── worker thread + its private state (touched only by WorkerLoop) ───────
    std::thread       m_worker;
    std::atomic<bool> m_running{false};
    static constexpr DWORD kReadIntervalMs = 150;
    static constexpr float kScanRadiusRaw = 1200.0f;   // wide pre-filter; region does the real cut

    std::vector<int> m_cachedPath;        // discovered panel index path
    uint64_t m_lastAreaChange = 0;
    int      m_discoverAttempts = 0;
    bool     m_noTrialThisArea = false;
    static constexpr int kBfsAttemptAt = 6;

    std::vector<GridPoint> m_exitPedestals;
    uint64_t m_pedestalsForArea = ~0ull;
    int m_lastRouteTermState = -1;

    RoomRegion m_region;
    uint64_t m_regionForArea = ~0ull, m_regionSealSig = 0, m_regionBuildCount = 0;
    int m_regionAnchorX = 0, m_regionAnchorY = 0;
    CrystalRoute m_workerRoute;           // route cache (worker-side)
    uint64_t m_routeSig = 0;
    int m_routeAnchorX = 0, m_routeAnchorY = 0;

    int      m_areaLevelCache = 0;
    uint64_t m_areaLevelForCounter = ~0ull;

    void StartWorker() {
        if (m_running.exchange(true)) return;
        m_worker = std::thread([this] { WorkerLoop(); });
    }
    void StopWorker() {
        if (!m_running.exchange(false)) return;
        if (m_worker.joinable()) m_worker.join();
    }

    void WorkerLoop() {
        while (m_running.load(std::memory_order_acquire)) {
            const DWORD t0 = GetTickCount();
            TickSnapshot snap;
            BuildSnapshot(snap);
            {
                std::lock_guard<std::mutex> lk(m_snapMutex);
                snap.seq = ++m_snapSeq;
                m_snap = std::move(snap);
            }
            const DWORD spent = GetTickCount() - t0;
            DWORD wait = (spent >= kReadIntervalMs) ? 25 : (kReadIntervalMs - spent);
            while (wait > 0 && m_running.load(std::memory_order_acquire)) {
                const DWORD step = wait < 25 ? wait : 25;
                Sleep(step);
                wait -= step;
            }
        }
    }

    // One full read pass — everything the render thread used to do per tick.
    void BuildSnapshot(TickSnapshot& snap) {
        snap.nowMs = NowMs();
        if (!ctx() || !ctx()->Game.IsAttached() || !ctx()->Game.IsInGame())
            return;                        // inGame=false: tracker freezes
        snap.inGame = true;

        uint64_t ac = ctx()->Game.GetAreaChangeCounter();
        if (ac != m_lastAreaChange) {
            m_lastAreaChange = ac;
            m_discoverAttempts = 0;
            m_noTrialThisArea = false;
        }
        snap.areaChangeCounter = m_lastAreaChange;

        snap.panel = FindTrialPanel(snap.floor);
        snap.trialAbsent = !snap.floor.valid && m_noTrialThisArea;
        if (!snap.floor.valid) return;

        // Settings the worker needs, copied under the lock (profile edits
        // reallocate vectors on the render thread).
        WeightProfile prof;
        bool haveProf = false, dbgLog = false;
        {
            std::lock_guard<std::mutex> lk(m_settingsMutex);
            if (const WeightProfile* p = m_settings.ActiveProfile()) { prof = *p; haveProf = true; }
            dbgLog = m_settings.timerDebugLog;
        }

        snap.res = ReadResources(ctx(), snap.panel);
        PlayerDefenses def = ReadDefenses(ctx());
        if (haveProf)
            Evaluate(snap.floor, prof, def, snap.res);

        PluginSDK::Entity pl = ctx()->Entities.GetPlayer();
        snap.pgx = static_cast<int>(pl.GridPositionX);
        snap.pgy = static_cast<int>(pl.GridPositionY);
        snap.playerHp = (pl.MaxHP > 0) ? pl.CurrentHP : -1;
        snap.charName = NarrowStr(pl.PlayerName);
        snap.panelVisible = snap.panel != 0 && ctx()->Ui.IsVisible(snap.panel);
        if (m_areaLevelForCounter != m_lastAreaChange) {
            m_areaLevelForCounter = m_lastAreaChange;
            m_areaLevelCache = ctx()->Game.GetSnapshot().CurrentAreaLevel;
        }
        snap.areaLevel = m_areaLevelCache;

        snap.ents = ScanTrialEntities(ctx(), pl.GridPositionX, pl.GridPositionY,
                                      kScanRadiusRaw,
                                      FloorNumFromTileset(snap.floor.floorTileset));

        // Auto room bounds (cached): flood-fill from the player with every
        // CLOSED door sealed. Rebuilt on area/door changes or >24-cell drift.
        uint64_t sealSig = 1469598103934665603ull;   // FNV offset
        for (const auto& d : snap.ents.doors) {
            uint64_t v = (static_cast<uint64_t>(d.entityId) << 1) | (d.open ? 1u : 0u);
            sealSig ^= v; sealSig *= 1099511628211ull;
        }
        const bool regionStale = !m_region.valid ||
            m_regionForArea != m_lastAreaChange ||
            m_regionSealSig != sealSig ||
            (std::max)(std::abs(snap.pgx - m_regionAnchorX),
                       std::abs(snap.pgy - m_regionAnchorY)) > 24;
        if (regionStale) {
            PluginSDK::WalkableGridHandle wg = ctx()->Terrain.GetWalkableGrid();
            if (wg.Valid()) {
                std::vector<DoorSeal> seals;
                seals.reserve(snap.ents.doors.size());
                for (const auto& d : snap.ents.doors)
                    seals.push_back({ { static_cast<int>(d.gridX),
                                        static_cast<int>(d.gridY) }, !d.open });
                m_region = BuildRoomRegion(wg.Data(), wg.Width(), wg.Height(),
                                           wg.SizeBytes(),
                                           pl.GridPositionX, pl.GridPositionY,
                                           seals, 3, 600);
                m_regionForArea = m_lastAreaChange;
                m_regionSealSig = sealSig;
                m_regionAnchorX = snap.pgx; m_regionAnchorY = snap.pgy;
                ++m_regionBuildCount;
            }
        }
        if (m_region.valid) {
            auto inRoom = [&](const TrialMarker& m) {
                return m_region.Contains(m.gridX, m.gridY, 3);
            };
            auto drop = [&](std::vector<TrialMarker>& v) {
                v.erase(std::remove_if(v.begin(), v.end(),
                        [&](const TrialMarker& m) { return !inRoom(m); }), v.end());
            };
            drop(snap.ents.crystals); drop(snap.ents.chests);
            drop(snap.ents.portals);  drop(snap.ents.levers);
        }

        if (!snap.ents.crystals.empty()) {
            // Route cache: full re-plan only when crystals/doors/region change
            // or the player moved enough to visibly date the first leg.
            uint64_t routeSig = 1469598103934665603ull;
            for (const auto& c : snap.ents.crystals) {
                routeSig ^= c.entityId; routeSig *= 1099511628211ull;
            }
            routeSig ^= sealSig; routeSig *= 1099511628211ull;
            routeSig ^= m_regionBuildCount; routeSig *= 1099511628211ull;
            const bool routeStale = m_routeSig != routeSig ||
                (std::max)(std::abs(snap.pgx - m_routeAnchorX),
                           std::abs(snap.pgy - m_routeAnchorY)) > 6;
            if (routeStale) {
                m_routeSig = routeSig;
                m_routeAnchorX = snap.pgx; m_routeAnchorY = snap.pgy;

                if (m_pedestalsForArea != m_lastAreaChange) {
                    m_pedestalsForArea = m_lastAreaChange;
                    m_exitPedestals.clear();
                    constexpr int kTileCenter = 11;   // tgt anchors at the 23x23 tile's corner
                    ctx()->Terrain.EnumerateTgtLocations(
                        [&](const PluginSDK::TgtLocation& t) -> bool {
                            // Exit pedestals vary per floor tileset:
                            // .../KethAscendancy/pedestal_01, .../Level_4/Features/
                            // Pedestal_01, .../Level_1/feature/pedestal_02 — match
                            // the family loosely.
                            std::string lp = LowerCopy(t.Path);
                            if (lp.find("kethascendancy") != std::string::npos &&
                                lp.find("pedestal_") != std::string::npos)
                                m_exitPedestals.push_back(
                                    { static_cast<int>(t.X) + kTileCenter,
                                      static_cast<int>(t.Y) + kTileCenter });
                            return true;
                        });
                    if (dbgLog) {
                        char buf[192];
                        snprintf(buf, sizeof(buf), "[SekhemaRoute] pedestal hints in area: %zu",
                                 m_exitPedestals.size());
                        ctx()->Log.Info(buf);
                        for (const auto& p : m_exitPedestals) {
                            snprintf(buf, sizeof(buf),
                                     "[SekhemaRoute] pedestal at %d,%d (player %d,%d)",
                                     p.x, p.y, snap.pgx, snap.pgy);
                            ctx()->Log.Info(buf);
                        }
                    }
                }

                m_workerRoute = ComputeCrystalRoute(snap.ents.crystals, snap.ents.doors,
                                                    m_exitPedestals, m_region,
                                                    snap.pgx, snap.pgy,
                                                    m_roomEntryGX.load(std::memory_order_relaxed),
                                                    m_roomEntryGY.load(std::memory_order_relaxed));

                const int termState =
                    m_workerRoute.stops.empty() ? -1 : (m_workerRoute.hasDoor ? 1 : 0);
                if (dbgLog && termState >= 0 && termState != m_lastRouteTermState) {
                    char buf[160];
                    snprintf(buf, sizeof(buf),
                             "[SekhemaRoute] terminal=%s at %d,%d walkable=%d stops=%zu",
                             m_workerRoute.hasDoor ? "yes" : "NO",
                             m_workerRoute.doorPoint.x, m_workerRoute.doorPoint.y,
                             m_workerRoute.walkable ? 1 : 0, m_workerRoute.stops.size());
                    ctx()->Log.Info(buf);
                }
                m_lastRouteTermState = termState;
            }
            snap.route = m_workerRoute;
        } else {
            m_workerRoute = CrystalRoute{};
            m_routeSig = 0;
        }
    }

    // ── tracker (render thread, driven by consumed snapshots) ────────────────
    void TickRunTracker(const TickSnapshot& s) {
        TrackerInput tin;
        tin.nowMs = s.nowMs;
        tin.inGame = s.inGame;
        tin.areaChangeCounter = s.areaChangeCounter;
        tin.floorValid = s.floor.valid;
        tin.trialAbsent = s.inGame && s.trialAbsent;
        if (s.floor.valid) {
            tin.floorName  = s.floor.floorTileset;
            tin.floorNum   = FloorNumFromTileset(s.floor.floorTileset);
            tin.layerCount = static_cast<int>(s.floor.layers.size());
            tin.choicesMade = s.floor.playerLayer + 1;
            for (int l = 0; l < tin.layerCount && l < 8; ++l) {
                const auto& L = s.floor.layers[l];
                for (int r = 0; r < static_cast<int>(L.size()); ++r)
                    if (L[r].isChosen) { tin.choices[l] = r; break; }
                const SekhemaRoom* node = nullptr;
                int idx = tin.choices[l];
                if (idx >= 0 && idx < static_cast<int>(L.size()))
                    node = &L[idx];
                else if (l == tin.layerCount - 1 && !L.empty())
                    node = &L[0];                       // boss layer: single node, never commits
                if (node) {
                    tin.rooms[l] = { node->roomType, node->affliction, node->reward };
                } else if (!L.empty()) {
                    // Identity not committed yet (the floor's first room commits
                    // late): when every candidate on the layer shares one type,
                    // show it provisionally; the commit relabels with the rest.
                    const std::string& t0 = L[0].roomType;
                    bool same = !t0.empty();
                    for (const auto& r : L)
                        if (r.roomType != t0) { same = false; break; }
                    if (same) tin.rooms[l].type = t0;
                }
            }
            if (s.res.valid) tin.honour = s.res.honourCurrent;
            tin.panelVisible = s.panelVisible;
            tin.playerHp = s.playerHp;
            tin.charName = s.charName;
            tin.areaLevel = s.areaLevel;
            tin.doors.reserve(s.ents.doors.size());
            for (const auto& d : s.ents.doors)
                tin.doors.push_back({ d.entityId, d.kind, d.open });
            tin.bosses.reserve(s.ents.bosses.size());
            for (const auto& b : s.ents.bosses)
                tin.bosses.push_back({ b.entityId, b.alive });
            // Door/boss state dumps: only on CHANGE (per-tick Info would flood).
            if (m_settings.timerDebugLog) {
                for (const auto& d : s.ents.doors) {
                    auto it = m_dbgDoorState.find(d.entityId);
                    if (it != m_dbgDoorState.end() && it->second == d.open) continue;
                    m_dbgDoorState[d.entityId] = d.open;
                    char buf[160];
                    snprintf(buf, sizeof(buf), "[SekhemaTimer] door id=%u kind=%s open=%d",
                             d.entityId, d.debugName, d.open ? 1 : 0);
                    ctx()->Log.Info(buf);
                }
                for (const auto& b : s.ents.bosses) {
                    auto it = m_dbgBossAlive.find(b.entityId);
                    if (it != m_dbgBossAlive.end() && it->second == b.alive) continue;
                    m_dbgBossAlive[b.entityId] = b.alive;
                    char buf[160];
                    snprintf(buf, sizeof(buf), "[SekhemaTimer] boss id=%u hp=%d/%d alive=%d",
                             b.entityId, b.hp, b.maxHp, b.alive ? 1 : 0);
                    ctx()->Log.Info(buf);
                }
            }
        }
        std::vector<TrackerEvent> events;
        m_tracker.Tick(tin, events);
        for (const auto& ev : events) {
            if (ev.type == TrackerEvent::Type::RoomEntered ||
                ev.type == TrackerEvent::Type::FloorEntered) {
                m_roomEntryGX.store(s.pgx, std::memory_order_relaxed);
                m_roomEntryGY.store(s.pgy, std::memory_order_relaxed);
            }
            ApplyTrackerEvent(ev);
        }
    }

    void ApplyTrackerEvent(const TrackerEvent& ev) {
        using T = TrackerEvent::Type;
        if (m_settings.timerDebugLog) {
            static const char* kNames[] = { "RunStarted", "FloorEntered", "RoomEntered",
                "RoomRelabeled", "RoomStartAdjusted", "RoomCleared", "FloorBossKilled",
                "FloorExited", "RunEnded" };
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "[SekhemaTimer] %s floor=%d(%s) layer=%d idx=%d type='%s' status='%s' at=%llu",
                     kNames[static_cast<int>(ev.type)], ev.floorNum, ev.floorName.c_str(),
                     ev.layer, ev.roomIdx, ev.info.type.c_str(), ev.runStatus.c_str(),
                     static_cast<unsigned long long>(ev.atMs));
            ctx()->Log.Info(buf);
        }
        switch (ev.type) {
        case T::RunStarted:
            m_runId = m_db.BeginRun(ev.charName, ev.areaLevel, ev.atMs);
            m_hist.dirty = true;
            break;
        case T::FloorEntered:
            m_floorId = m_db.BeginFloor(m_runId, ev.floorNum, ev.floorName, ev.atMs);
            break;
        case T::RoomEntered:
            m_roomId = m_db.BeginRoom(m_floorId, ev.layer, ev.roomIdx, ev.info, ev.atMs, ev.isBoss);
            break;
        case T::RoomRelabeled:
            m_db.RelabelRoom(m_roomId, ev.roomIdx, ev.info);
            break;
        case T::RoomStartAdjusted:
            m_db.UpdateRoomStart(m_roomId, ev.atMs);
            break;
        case T::RoomCleared:
            m_db.EndRoom(m_roomId, ev.atMs);
            break;
        case T::FloorBossKilled:
            m_db.SetFloorBossKilled(m_floorId, ev.atMs);
            break;
        case T::FloorExited:
            m_db.EndFloor(m_floorId, ev.atMs, ev.roomsCleared);
            m_floorId = -1; m_roomId = -1;
            break;
        case T::RunEnded:
            m_db.EndRun(m_runId, ev.atMs, ev.runStatus, ev.floorsDone, ev.lastFloor, ev.honourEnd);
            m_runId = -1;
            m_hist.dirty = true;
            break;
        }
    }

    // ── trial-panel discovery (worker thread only) ───────────────────────────

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
            if (!m_running.load(std::memory_order_acquire)) return 0;   // fast shutdown
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
        // panel here. Skip all probing until the next area change re-arms us.
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

        // Fast paths failed. The fallback BFS walks the whole game UI tree —
        // run it at most ONCE per area, after a short settle, then stand down.
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
