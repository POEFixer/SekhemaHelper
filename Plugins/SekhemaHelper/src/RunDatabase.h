#pragma once
// RunDatabase.h — SQLite persistence for Sekhema runs/floors/rooms (KillCount
// pattern: sqlite3 amalgamation compiled into the DLL). SDK-free; standalone-
// tested in tests/test_db.cpp. All methods no-op when the DB failed to open.
#include "RunTracker.h"   // TrackerRoomInfo
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;   // forward declare — sqlite3.h included only in the .cpp

namespace sekhema {

class RunDatabase {
public:
    bool Open(const std::filesystem::path& pluginDir);   // <dir>/data/sekhema_runs.db, creates schema
    void Close();
    void CloseDanglingRuns();                            // in_progress -> abandoned (crash/restart)

    int64_t BeginRun(const std::string& charName, int areaLevel, uint64_t startedMs);
    void    EndRun(int64_t runId, uint64_t endedMs, const std::string& status,
                   int floorsDone, int lastFloor, int honourEnd);
    int64_t BeginFloor(int64_t runId, int floorNum, const std::string& floorName, uint64_t enteredMs);
    void    SetFloorBossKilled(int64_t floorId, uint64_t killedMs);
    void    EndFloor(int64_t floorId, uint64_t exitedMs, int roomsCleared);
    int64_t BeginRoom(int64_t floorId, int layer, int roomIdx, const TrackerRoomInfo& info,
                      uint64_t enteredMs, bool isBoss);
    void    RelabelRoom(int64_t roomId, int roomIdx, const TrackerRoomInfo& info);
    void    UpdateRoomStart(int64_t roomId, uint64_t enteredMs);
    void    EndRoom(int64_t roomId, uint64_t clearedMs);

    struct RunRow   { int64_t id = 0; std::string charName; int areaLevel = 0; uint64_t startedMs = 0;
                      int64_t durationMs = -1; std::string status; int floorsDone = 0, lastFloor = 0,
                      honourEnd = -1; std::string startedText; };
    struct FloorRow { int64_t id = 0; int floorNum = 0; std::string floorName; uint64_t enteredMs = 0;
                      int64_t durationMs = -1; int64_t bossKilledMs = -1; int roomsCleared = 0; };
    struct RoomRow  { int64_t id = 0; int layer = 0, roomIdx = -1; std::string type, affliction, reward;
                      int64_t durationMs = -1; bool isBoss = false; };
    struct Stats    { int total = 0, completed = 0; int64_t bestMs = -1, avgMs = -1; };

    std::vector<RunRow>   QueryRuns(int limit = 200);    // newest first
    std::vector<FloorRow> QueryFloors(int64_t runId);    // by entered time
    std::vector<RoomRow>  QueryRooms(int64_t floorId);   // by entered time
    Stats  QueryStats();                                 // best/avg over completed runs
    void   DeleteRun(int64_t runId);                     // removes floors + rooms too
    void   ClearAll();

private:
    sqlite3* m_db = nullptr;
    void Exec(const char* sql);
    void CreateTables();
};

} // namespace sekhema
