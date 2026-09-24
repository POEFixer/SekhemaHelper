#pragma once
// RunTracker.h — pure run/floor/room timing state machine for the Sekhema trial.
// SDK-free (standalone-tested in tests/test_tracker.cpp). Consumes one
// TrackerInput per plugin read-tick and emits TrackerEvents; the plugin glue
// maps events to RunDatabase rows and the overlay reads GetOverlay().
//
// Semantics implement the CE-verified model (spec 2026-07-02, v2 room flow):
//  - A commit of a layer ABOVE the open room without a selector-door flip =
//    the Trial-Map device at the room's end (pre-commit) -> the open room is
//    CLEARED there (timer freezes, shown with a checkmark) and the next room
//    becomes PENDING; the real door click (selector flip) STARTS it. A commit
//    WITH a flip in the same tick = direct click (clear + start together).
//  - The open room's OWN layer committing = identity fill only (late commit,
//    floor's first room); the boss layer NEVER commits.
//  - Boss room: starts on the boss-door flip (or first boss sighting once no
//    normal room is active); ends on boss death (hp<=0 or long absence).
//  - Floor = one zone; floor boundaries are zone changes; boss death is the
//    only end-of-floor signal (graph never records it).
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sekhema {

struct TrackerRoomInfo { std::string type, affliction, reward; };

enum class DoorKind { Selector, Completion, Transition };
struct DoorState { uint32_t id = 0; DoorKind kind = DoorKind::Selector; bool open = false; };
struct BossState { uint32_t id = 0; bool alive = false; };

struct TrackerInput {
    uint64_t nowMs = 0;
    bool     inGame = false;
    uint64_t areaChangeCounter = 0;
    bool     floorValid = false;    // trial panel resolved this tick
    bool     trialAbsent = false;   // in game AND panel definitively absent (post-BFS stand-down)
    int      floorNum = 0;          // 1..4 (FloorNumFromTileset), 0 unknown
    std::string floorName;          // tileset string ("Caverns"...)
    int      layerCount = 0;
    int      choicesMade = 0;       // Counter & 7
    std::array<int, 8> choices{ {-1,-1,-1,-1,-1,-1,-1,-1} };
    std::array<TrackerRoomInfo, 8> rooms{};   // graph info per layer (chosen idx); [layerCount-1] = boss node
    int      honour = -1;           // -1 unknown
    int      playerHp = -1;         // -1 unknown
    bool     panelVisible = false;  // the Trial-Map panel is open on screen
    std::vector<DoorState> doors;   // absolute open-state of nearby trial doors this tick
    std::vector<BossState> bosses;  // current-floor boss entities visible this tick
    std::string charName;
    int      areaLevel = 0;
};

struct TrackerEvent {
    enum class Type { RunStarted, FloorEntered, RoomEntered, RoomRelabeled, RoomStartAdjusted,
                      RoomCleared, FloorBossKilled, FloorExited, RunEnded };
    Type type{};
    uint64_t atMs = 0;
    int  floorNum = 0; std::string floorName;
    int  layer = -1, roomIdx = -1;
    TrackerRoomInfo info; bool isBoss = false;
    int  roomsCleared = 0;                                 // FloorExited
    std::string runStatus;                                 // RunEnded: completed|failed|abandoned
    int  honourEnd = -1, floorsDone = 0, lastFloor = 0;    // RunEnded
    std::string charName; int areaLevel = 0;               // RunStarted
};

class RunTracker {
public:
    void Tick(const TrackerInput& in, std::vector<TrackerEvent>& out);

    struct Overlay {
        bool inTrial = false;
        uint64_t runStartMs = 0, floorStartMs = 0, roomStartMs = 0;
        int floorNum = 0; std::string floorName;
        std::string roomLabel; bool roomOpen = false; bool roomIsBoss = false;
        int roomLayer = -1;                      // 0-based graph layer of the open room
        bool roomFrozen = false;                 // cleared at the device, next not started yet
        uint64_t roomFrozenDurationMs = 0;       // frozen (final) room duration for the ✓ display
        uint64_t floorBossKillMs = 0; int lockedFloorNum = 0; uint64_t lockedFloorDurationMs = 0;
        bool runComplete = false; uint64_t runEndMs = 0;
    };
    Overlay GetOverlay() const { return m_ov; }

private:
    // Event helpers
    TrackerEvent Ev(TrackerEvent::Type t, uint64_t atMs) const;
    void OpenRoom(std::vector<TrackerEvent>& out, const TrackerInput& in,
                  int layer, int idx, const TrackerRoomInfo& info, bool isBoss,
                  bool atFloorStart, uint64_t atMs);
    void FreezeRoom(std::vector<TrackerEvent>& out, uint64_t atMs);   // emit RoomCleared, keep as ✓
    void CloseRoomIfOpen(std::vector<TrackerEvent>& out, uint64_t atMs, bool emitCleared);
    void EnterFloor(std::vector<TrackerEvent>& out, const TrackerInput& in);
    void ExitFloor(std::vector<TrackerEvent>& out, uint64_t atMs);
    void EndRun(std::vector<TrackerEvent>& out, const TrackerInput& in,
                const char* status, uint64_t atMs);
    void ResetToIdle();
    void UpdateOverlay(const TrackerInput& in);

    // run state
    bool m_inTrial = false;
    bool m_runEnded = false;        // completed/failed emitted; waiting for trial exit
    bool m_floorOpen = false;       // FloorEntered emitted, FloorExited pending
    uint64_t m_runStartMs = 0;
    int m_floorsDone = 0;
    uint64_t m_lastBossKillMs = 0;
    // floor state
    int m_floorNum = 0; std::string m_floorName;
    uint64_t m_floorStartMs = 0;
    int m_layerCount = 0;
    int m_roomsCleared = 0;
    bool m_bossKilledThisFloor = false;
    bool m_bossRoomOpened = false;
    int m_prevChoicesMade = 0;
    // room state (a cleared room stays "open" as the frozen ✓ display until
    // the next room starts)
    bool m_roomOpen = false, m_roomCleared = false, m_roomIsBoss = false;
    bool m_roomAtFloorStart = false, m_roomStartAdjusted = false;
    int m_roomLayer = -1, m_roomIdx = -1;
    uint64_t m_roomStartMs = 0, m_roomClearedMs = 0;
    TrackerRoomInfo m_roomInfo;
    // pending room: committed at the device (no flip yet); starts on the click
    struct Pending { bool active = false; int layer = -1, idx = -1; TrackerRoomInfo info; };
    Pending m_pending;
    // sensors
    uint64_t m_lastAreaCounter = 0; bool m_areaCounterInit = false;
    bool m_prevPanelVisible = false;
    uint64_t m_lastSelectorFlipMs = 0;                           // click->commit race window
    std::unordered_map<uint32_t, bool> m_doorOpen;               // last seen open state
    struct BossTrack { bool lastAlive = true; uint64_t lastPresentMs = 0; };
    std::unordered_map<uint32_t, BossTrack> m_bosses;
    int m_prevHonour = -1, m_prevHp = -1;

    Overlay m_ov;
};

} // namespace sekhema
