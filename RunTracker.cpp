#include "RunTracker.h"

namespace sekhema {

// Absence window after which a tracked boss that vanished from the entity list
// counts as dead (covers despawn-on-death; Zarokh phase blinks are shorter).
static constexpr uint64_t kBossAbsenceMs = 5000;

TrackerEvent RunTracker::Ev(TrackerEvent::Type t, uint64_t atMs) const {
    TrackerEvent e;
    e.type = t; e.atMs = atMs;
    e.floorNum = m_floorNum; e.floorName = m_floorName;
    return e;
}

void RunTracker::OpenRoom(std::vector<TrackerEvent>& out, const TrackerInput& in,
                          int layer, int idx, const TrackerRoomInfo& info, bool isBoss,
                          bool atFloorStart, uint64_t atMs) {
    m_roomOpen = true; m_roomCleared = false; m_roomIsBoss = isBoss;
    m_roomAtFloorStart = atFloorStart; m_roomStartAdjusted = false;
    m_roomLayer = layer; m_roomIdx = idx; m_roomStartMs = atMs; m_roomClearedMs = 0;
    m_roomInfo = info;
    (void)in;
    TrackerEvent e = Ev(TrackerEvent::Type::RoomEntered, atMs);
    e.layer = layer; e.roomIdx = idx; e.info = info; e.isBoss = isBoss;
    out.push_back(e);
}

// The device-at-the-room's-end moment: the room is done — emit RoomCleared and
// keep it as the frozen ✓ display until the next room starts.
void RunTracker::FreezeRoom(std::vector<TrackerEvent>& out, uint64_t atMs) {
    if (!m_roomOpen || m_roomCleared) return;
    TrackerEvent e = Ev(TrackerEvent::Type::RoomCleared, atMs);
    e.layer = m_roomLayer; e.roomIdx = m_roomIdx; e.info = m_roomInfo; e.isBoss = m_roomIsBoss;
    out.push_back(e);
    m_roomCleared = true;
    m_roomClearedMs = atMs;
    ++m_roomsCleared;
}

void RunTracker::CloseRoomIfOpen(std::vector<TrackerEvent>& out, uint64_t atMs, bool emitCleared) {
    if (!m_roomOpen) return;
    if (!m_roomCleared && emitCleared)
        FreezeRoom(out, atMs);
    m_roomOpen = false; m_roomCleared = false; m_roomIsBoss = false;
    m_roomAtFloorStart = false; m_roomStartAdjusted = false;
    m_roomClearedMs = 0;
}

void RunTracker::EnterFloor(std::vector<TrackerEvent>& out, const TrackerInput& in) {
    m_floorNum = in.floorNum; m_floorName = in.floorName;
    m_floorStartMs = in.nowMs; m_layerCount = in.layerCount;
    m_roomsCleared = 0; m_bossKilledThisFloor = false; m_bossRoomOpened = false;
    m_prevChoicesMade = in.choicesMade;
    m_floorOpen = true;
    m_pending = Pending{};
    m_bosses.clear();
    out.push_back(Ev(TrackerEvent::Type::FloorEntered, in.nowMs));

    // The floor's current room opens at zone arrival (fresh floor: layer 0 with
    // unknown identity — its commit is late; floors 2-4 include the rest area
    // until the entrance flip re-anchors the start).
    int layer = (in.choicesMade > 0) ? in.choicesMade - 1 : 0;
    if (layer > 7) layer = 7;
    int idx = (in.choicesMade > 0) ? in.choices[layer] : -1;
    OpenRoom(out, in, layer, idx, in.rooms[layer], false, /*atFloorStart*/true, in.nowMs);
}

void RunTracker::ExitFloor(std::vector<TrackerEvent>& out, uint64_t atMs) {
    if (!m_floorOpen) return;
    CloseRoomIfOpen(out, atMs, /*emitCleared*/false);   // unfinished rooms stay un-cleared
    m_pending = Pending{};
    TrackerEvent e = Ev(TrackerEvent::Type::FloorExited, atMs);
    e.roomsCleared = m_roomsCleared;
    out.push_back(e);
    m_floorOpen = false;
}

void RunTracker::EndRun(std::vector<TrackerEvent>& out, const TrackerInput& in,
                        const char* status, uint64_t atMs) {
    TrackerEvent e = Ev(TrackerEvent::Type::RunEnded, atMs);
    e.runStatus = status;
    e.floorsDone = m_floorsDone; e.lastFloor = m_floorNum;
    e.honourEnd = in.honour;
    out.push_back(e);
    m_runEnded = true;
    if (e.runStatus == "completed") { m_ov.runComplete = true; m_ov.runEndMs = atMs; }
}

void RunTracker::ResetToIdle() {
    m_inTrial = false; m_runEnded = false; m_floorOpen = false;
    m_roomOpen = false; m_roomCleared = false; m_roomIsBoss = false;
    m_roomAtFloorStart = false; m_roomStartAdjusted = false;
    m_roomLayer = -1; m_roomIdx = -1; m_roomClearedMs = 0;
    m_pending = Pending{};
    m_doorOpen.clear(); m_bosses.clear();
    m_prevHonour = -1; m_prevHp = -1;
    m_ov.inTrial = false; m_ov.roomOpen = false;   // runComplete/runEndMs kept for overlay linger
}

void RunTracker::UpdateOverlay(const TrackerInput& in) {
    (void)in;
    m_ov.inTrial = m_inTrial;
    m_ov.runStartMs = m_runStartMs;
    m_ov.floorStartMs = m_floorStartMs;
    m_ov.roomStartMs = m_roomStartMs;
    m_ov.floorNum = m_floorNum; m_ov.floorName = m_floorName;
    m_ov.roomOpen = m_roomOpen; m_ov.roomIsBoss = m_roomIsBoss;
    m_ov.roomLayer = m_roomOpen ? m_roomLayer : -1;
    m_ov.roomFrozen = m_roomOpen && m_roomCleared;
    m_ov.roomFrozenDurationMs =
        (m_ov.roomFrozen && m_roomClearedMs >= m_roomStartMs) ? m_roomClearedMs - m_roomStartMs : 0;
    m_ov.roomLabel = m_roomIsBoss ? "Boss"
                   : (!m_roomInfo.type.empty() ? m_roomInfo.type
                                               : (m_roomOpen ? std::string("?") : std::string()));
}

void RunTracker::Tick(const TrackerInput& in, std::vector<TrackerEvent>& out) {
    if (!in.inGame) return;   // frozen (menus/loading app states)

    // Area change: entity ids are per-instance — reset the door/boss sensors.
    if (!m_areaCounterInit) {
        m_areaCounterInit = true;
        m_lastAreaCounter = in.areaChangeCounter;
    } else if (in.areaChangeCounter != m_lastAreaCounter) {
        m_lastAreaCounter = in.areaChangeCounter;
        m_doorOpen.clear();
        m_bosses.clear();
    }

    // Selector-door closed->open flip detection (first sighting is never a
    // flip). Completion/transition doors are tracked for debugging only — the
    // device commit is the room-cleared signal in this model.
    bool selectorFlipped = false;
    for (const auto& d : in.doors) {
        auto it = m_doorOpen.find(d.id);
        bool prev = (it != m_doorOpen.end()) ? it->second : d.open;
        if (!prev && d.open && d.kind == DoorKind::Selector)
            selectorFlipped = true;
        m_doorOpen[d.id] = d.open;
    }
    if (selectorFlipped) m_lastSelectorFlipMs = in.nowMs;

    // Trial-Map panel opening edge (the device only exists at a room's end
    // junction / the floor foyer, so an opening while a room is engaged is the
    // player standing at ITS end = the user-facing "room done" moment).
    const bool panelOpened = in.panelVisible && !m_prevPanelVisible;
    m_prevPanelVisible = in.panelVisible;

    // Fail-edge sensors (recorded across all in-trial paths).
    const bool honourFail = m_inTrial && !m_runEnded && (in.honour == 0 && m_prevHonour > 0);
    const bool hpFail     = m_inTrial && !m_runEnded && (in.playerHp == 0 && m_prevHp > 0);
    if (in.honour   >= 0) m_prevHonour = in.honour;
    if (in.playerHp >= 0) m_prevHp = in.playerHp;

    if (!m_inTrial) {
        if (in.floorValid && in.floorNum > 0) {
            m_inTrial = true; m_runEnded = false;
            m_runStartMs = in.nowMs; m_floorsDone = 0; m_lastBossKillMs = 0;
            m_ov = Overlay{};   // clears any previous run's lock-in / complete state
            m_floorNum = in.floorNum; m_floorName = in.floorName;   // for Ev() on RunStarted
            TrackerEvent e = Ev(TrackerEvent::Type::RunStarted, in.nowMs);
            e.charName = in.charName; e.areaLevel = in.areaLevel;
            out.push_back(e);
            EnterFloor(out, in);
        }
        UpdateOverlay(in);
        return;
    }

    if (m_runEnded) {
        // Completed (floor 4) or failed: only waiting for the trial exit.
        if (in.trialAbsent) {
            ExitFloor(out, in.nowMs);
            ResetToIdle();
        }
        UpdateOverlay(in);
        return;
    }

    if (honourFail || hpFail) {
        ExitFloor(out, in.nowMs);
        EndRun(out, in, "failed", in.nowMs);
        UpdateOverlay(in);
        return;
    }

    if (in.trialAbsent) {
        // Left the trial: completed if the (last) floor's boss died, else abandoned.
        const bool completed = m_bossKilledThisFloor;
        const uint64_t endMs = completed ? m_lastBossKillMs : in.nowMs;
        ExitFloor(out, in.nowMs);
        EndRun(out, in, completed ? "completed" : "abandoned", endMs);
        ResetToIdle();
        UpdateOverlay(in);
        return;
    }

    if (!in.floorValid) { UpdateOverlay(in); return; }   // loading between floors

    if (in.floorNum > 0 && in.floorNum != m_floorNum) {
        ExitFloor(out, in.nowMs);
        EnterFloor(out, in);
        UpdateOverlay(in);
        return;
    }

    // ── same floor, panel valid: room logic ──────────────────────────────────
    if (in.layerCount != m_layerCount) { UpdateOverlay(in); return; }       // drifted read
    if (in.choicesMade < m_prevChoicesMade) { UpdateOverlay(in); return; }  // drifted read

    bool flipConsumed = false;

    // 0) Device-open freeze: the map panel opening while a room is engaged
    //    marks it done (green ✓, timer stops). Layer 0 needs its own commit
    //    first — that distinguishes the room-end device from the floor-start
    //    foyer device (opening the foyer map must not freeze the first room).
    if (panelOpened && m_roomOpen && !m_roomCleared && !m_roomIsBoss &&
        (m_roomLayer > 0 || in.choicesMade >= 1)) {
        FreezeRoom(out, in.nowMs);
    }

    // 1) Choice commits.
    //    - The open room's OWN layer committing = late identity fill (relabel).
    //    - A HIGHER layer committing = the player is at the junction: the open
    //      room is done (freeze ✓ at the device). With a selector flip in the
    //      same tick it was a direct click — the new room starts immediately;
    //      without one it goes PENDING until the click.
    if (in.choicesMade > m_prevChoicesMade) {
        for (int L = m_prevChoicesMade; L < in.choicesMade && L < 8; ++L) {
            int idx = in.choices[L];
            if (m_roomOpen && L == m_roomLayer) {
                if (m_roomIdx < 0 && idx >= 0) {
                    m_roomIdx = idx; m_roomInfo = in.rooms[L];
                    TrackerEvent e = Ev(TrackerEvent::Type::RoomRelabeled, in.nowMs);
                    e.layer = L; e.roomIdx = idx; e.info = m_roomInfo;
                    out.push_back(e);
                }
            } else if (L > m_roomLayer || !m_roomOpen) {
                FreezeRoom(out, in.nowMs);                    // device / click junction moment
                // A click's door flip and its graph commit can land one tick
                // apart — a flip in the last second means this commit IS the click.
                const bool clickCommit = selectorFlipped ||
                    (m_lastSelectorFlipMs > 0 && in.nowMs >= m_lastSelectorFlipMs &&
                     in.nowMs - m_lastSelectorFlipMs <= 1000);
                if (clickCommit) {
                    CloseRoomIfOpen(out, in.nowMs, false);
                    OpenRoom(out, in, L, idx, in.rooms[L], false, false, in.nowMs);
                    m_pending = Pending{};
                    flipConsumed = true;
                } else {
                    m_pending.active = true;
                    m_pending.layer = L; m_pending.idx = idx; m_pending.info = in.rooms[L];
                }
            }
        }
        m_prevChoicesMade = in.choicesMade;
    } else if (m_roomOpen && !m_roomIsBoss && m_roomLayer >= 0 && m_roomLayer < in.choicesMade) {
        // Defensive relabel: the committed value was rewritten without a count
        // change (map pre-selection overwritten by the real click).
        int cur = in.choices[m_roomLayer];
        if (cur >= 0 && cur != m_roomIdx) {
            m_roomIdx = cur; m_roomInfo = in.rooms[m_roomLayer];
            TrackerEvent e = Ev(TrackerEvent::Type::RoomRelabeled, in.nowMs);
            e.layer = m_roomLayer; e.roomIdx = cur; e.info = m_roomInfo;
            out.push_back(e);
        }
    }
    if (m_roomOpen && m_roomInfo.type.empty() && m_roomLayer >= 0 && m_roomLayer < 8 &&
        !in.rooms[m_roomLayer].type.empty()) {
        // Provisional/late classification caught up — refresh the label silently.
        m_roomInfo = in.rooms[m_roomLayer];
    }
    if (m_pending.active && m_pending.layer < in.choicesMade) {
        // Pending identity may be rewritten by the click on the other door.
        int cur = in.choices[m_pending.layer];
        if (cur >= 0 && cur != m_pending.idx) {
            m_pending.idx = cur; m_pending.info = in.rooms[m_pending.layer];
        }
    }

    // 2) Selector flip not consumed by a same-tick commit:
    //    a. a pending room starts NOW (the click after the device pre-commit);
    //    a2. no pending but the current room is frozen (✓ at the device): the
    //        flip IS the choice — the next layer starts immediately with a
    //        lazy identity (the game may commit Choices[L] much later,
    //        especially at the first junction; the commit relabels the room);
    //    b. a floor-start room re-anchors its start to the entrance click
    //       (removes the foyer/rest-area dwell from the first room's time);
    //    c. with every normal layer committed and no active room, the flip is
    //       the boss door — the boss room starts.
    if (selectorFlipped && !flipConsumed) {
        if (m_pending.active) {
            CloseRoomIfOpen(out, in.nowMs, false);            // frozen ✓ room ends here
            OpenRoom(out, in, m_pending.layer, m_pending.idx, m_pending.info, false, false, in.nowMs);
            m_pending = Pending{};
        } else if (m_roomOpen && m_roomCleared && !m_roomIsBoss &&
                   m_roomLayer + 1 < in.layerCount) {
            int L = m_roomLayer + 1; if (L > 7) L = 7;
            const bool isBoss = (L == in.layerCount - 1);
            int idx = (L < in.choicesMade) ? in.choices[L] : (isBoss ? 0 : -1);
            CloseRoomIfOpen(out, in.nowMs, false);
            OpenRoom(out, in, L, idx, in.rooms[L], isBoss, false, in.nowMs);
            if (isBoss) m_bossRoomOpened = true;
        } else if (m_roomOpen && !m_roomCleared && m_roomAtFloorStart && m_roomLayer == 0 &&
                   !m_roomStartAdjusted && in.nowMs > m_roomStartMs) {
            m_roomStartMs = in.nowMs;
            m_roomStartAdjusted = true;
            TrackerEvent e = Ev(TrackerEvent::Type::RoomStartAdjusted, in.nowMs);
            e.layer = m_roomLayer; e.roomIdx = m_roomIdx; e.info = m_roomInfo;
            out.push_back(e);
        } else if (!m_bossRoomOpened &&
                   m_layerCount > 1 && m_prevChoicesMade >= m_layerCount - 1) {
            // Every normal layer is committed — this flip is the boss door.
            // The last room has no device-commit to freeze it, so the flip is
            // also its cleared moment.
            FreezeRoom(out, in.nowMs);
            CloseRoomIfOpen(out, in.nowMs, false);
            int bl = m_layerCount - 1; if (bl > 7) bl = 7;
            OpenRoom(out, in, bl, 0, in.rooms[bl], /*isBoss*/true, false, in.nowMs);
            m_bossRoomOpened = true;
            m_pending = Pending{};
        }
    }

    // 3) Boss sighting fallback: enter the boss room only when no normal room
    //    is still active (the boss is visible through the wall from the last
    //    room — a premature switch would cut that room short). A boss already
    //    seen dying/dead forces the switch so the death machinery can run even
    //    if the boss-door flip was missed.
    if (!m_bossRoomOpened && !in.bosses.empty()) {
        bool anyBossDead = false;
        for (const auto& b : in.bosses)
            if (!b.alive) { anyBossDead = true; break; }
        if (!m_roomOpen || m_roomCleared || anyBossDead) {
            FreezeRoom(out, in.nowMs);   // only reachable un-frozen in the forced case
            CloseRoomIfOpen(out, in.nowMs, false);
            int bl = (in.layerCount > 0) ? in.layerCount - 1 : 0; if (bl > 7) bl = 7;
            OpenRoom(out, in, bl, 0, in.rooms[bl], /*isBoss*/true, false, in.nowMs);
            m_bossRoomOpened = true;
            m_pending = Pending{};
        }
    }

    // 4) Boss tracking + death (all seen bosses dead or long-absent).
    for (const auto& b : in.bosses) {
        auto& t = m_bosses[b.id];
        t.lastAlive = b.alive;
        t.lastPresentMs = in.nowMs;
    }
    if (m_bossRoomOpened && !m_bossKilledThisFloor && !m_bosses.empty()) {
        bool allDead = true;
        for (const auto& [id, t] : m_bosses) {
            const bool deadByHp = !t.lastAlive;
            const bool deadByAbsence =
                in.nowMs > t.lastPresentMs && (in.nowMs - t.lastPresentMs) > kBossAbsenceMs;
            if (!deadByHp && !deadByAbsence) { allDead = false; break; }
        }
        if (allDead) {
            FreezeRoom(out, in.nowMs);                         // boss room cleared at the kill
            m_bossKilledThisFloor = true;
            ++m_floorsDone;
            m_lastBossKillMs = in.nowMs;
            out.push_back(Ev(TrackerEvent::Type::FloorBossKilled, in.nowMs));
            m_ov.lockedFloorNum = m_floorNum;
            m_ov.lockedFloorDurationMs = in.nowMs - m_floorStartMs;
            m_ov.floorBossKillMs = in.nowMs;
            if (m_floorNum == 4)
                EndRun(out, in, "completed", in.nowMs);
        }
    }

    UpdateOverlay(in);
}

} // namespace sekhema
