#include "RunDatabase.h"
#include "sqlite3.h"
#include <Windows.h>

namespace sekhema {

// ── small statement helper ────────────────────────────────────────────────────
namespace {
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (db && sqlite3_prepare_v2(db, sql, -1, &m_st, nullptr) != SQLITE_OK)
            m_st = nullptr;
    }
    ~Stmt() { if (m_st) sqlite3_finalize(m_st); }
    Stmt(const Stmt&) = delete; Stmt& operator=(const Stmt&) = delete;

    bool valid() const { return m_st != nullptr; }
    void BindI64(int i, int64_t v)            { if (m_st) sqlite3_bind_int64(m_st, i, v); }
    void BindInt(int i, int v)                { if (m_st) sqlite3_bind_int(m_st, i, v); }
    void BindText(int i, const std::string& s){ if (m_st) sqlite3_bind_text(m_st, i, s.c_str(), -1, SQLITE_TRANSIENT); }
    bool Step()                               { return m_st && sqlite3_step(m_st) == SQLITE_ROW; }
    void Run()                                { if (m_st) sqlite3_step(m_st); }

    int64_t     I64(int c)  const { return sqlite3_column_int64(m_st, c); }
    int         Int(int c)  const { return sqlite3_column_int(m_st, c); }
    bool        Null(int c) const { return sqlite3_column_type(m_st, c) == SQLITE_NULL; }
    std::string Text(int c) const {
        const unsigned char* t = sqlite3_column_text(m_st, c);
        return t ? reinterpret_cast<const char*>(t) : "";
    }
private:
    sqlite3_stmt* m_st = nullptr;
};
} // namespace

bool RunDatabase::Open(const std::filesystem::path& pluginDir) {
    namespace fs = std::filesystem;
    fs::path dataDir = pluginDir / "data";
    std::error_code ec;
    if (!fs::exists(dataDir, ec)) fs::create_directories(dataDir, ec);

    fs::path dbPath = dataDir / "sekhema_runs.db";
    // sqlite3_open expects UTF-8 on Windows; build it from the wide form
    // (path::string() would use the ANSI codepage — KillCount precedent).
    std::wstring wide = dbPath.wstring();
    int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                       nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                          utf8.data(), needed, nullptr, nullptr);

    if (sqlite3_open(utf8.c_str(), &m_db) != SQLITE_OK) {
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return false;
    }
    Exec("PRAGMA foreign_keys = ON;");
    CreateTables();
    return true;
}

void RunDatabase::Close() {
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

void RunDatabase::Exec(const char* sql) {
    if (!m_db) return;
    char* err = nullptr;
    sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

void RunDatabase::CreateTables() {
    Exec(R"(
        CREATE TABLE IF NOT EXISTS runs (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          char_name TEXT DEFAULT '', area_level INTEGER DEFAULT 0,
          started_at_ms INTEGER NOT NULL, started_at TEXT NOT NULL,
          ended_at_ms INTEGER, duration_ms INTEGER,
          status TEXT NOT NULL DEFAULT 'in_progress',
          floors_done INTEGER DEFAULT 0, last_floor INTEGER DEFAULT 0,
          honour_end INTEGER DEFAULT -1);
        CREATE TABLE IF NOT EXISTS floors (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          run_id INTEGER NOT NULL REFERENCES runs(id) ON DELETE CASCADE,
          floor_num INTEGER, floor_name TEXT,
          entered_at_ms INTEGER NOT NULL, boss_killed_at_ms INTEGER, exited_at_ms INTEGER,
          duration_ms INTEGER, rooms_cleared INTEGER DEFAULT 0);
        CREATE TABLE IF NOT EXISTS rooms (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          floor_id INTEGER NOT NULL REFERENCES floors(id) ON DELETE CASCADE,
          layer INTEGER, room_idx INTEGER, room_type TEXT, affliction TEXT, reward TEXT,
          entered_at_ms INTEGER NOT NULL, cleared_at_ms INTEGER, duration_ms INTEGER,
          is_boss INTEGER DEFAULT 0);
        CREATE INDEX IF NOT EXISTS idx_runs_started ON runs(started_at_ms DESC);
        CREATE INDEX IF NOT EXISTS idx_floors_run ON floors(run_id);
        CREATE INDEX IF NOT EXISTS idx_rooms_floor ON rooms(floor_id);
    )");
}

void RunDatabase::CloseDanglingRuns() {
    Exec("UPDATE runs SET status='abandoned' WHERE status='in_progress';");
}

int64_t RunDatabase::BeginRun(const std::string& charName, int areaLevel, uint64_t startedMs) {
    if (!m_db) return -1;
    Stmt s(m_db, "INSERT INTO runs (char_name, area_level, started_at_ms, started_at) "
                 "VALUES (?1, ?2, ?3, datetime(?3/1000,'unixepoch','localtime'));");
    s.BindText(1, charName); s.BindInt(2, areaLevel); s.BindI64(3, static_cast<int64_t>(startedMs));
    s.Run();
    return sqlite3_last_insert_rowid(m_db);
}

void RunDatabase::EndRun(int64_t runId, uint64_t endedMs, const std::string& status,
                         int floorsDone, int lastFloor, int honourEnd) {
    Stmt s(m_db, "UPDATE runs SET ended_at_ms=?2, duration_ms=?2-started_at_ms, status=?3, "
                 "floors_done=?4, last_floor=?5, honour_end=?6 WHERE id=?1;");
    s.BindI64(1, runId); s.BindI64(2, static_cast<int64_t>(endedMs)); s.BindText(3, status);
    s.BindInt(4, floorsDone); s.BindInt(5, lastFloor); s.BindInt(6, honourEnd);
    s.Run();
}

int64_t RunDatabase::BeginFloor(int64_t runId, int floorNum, const std::string& floorName,
                                uint64_t enteredMs) {
    if (!m_db) return -1;
    Stmt s(m_db, "INSERT INTO floors (run_id, floor_num, floor_name, entered_at_ms) "
                 "VALUES (?1, ?2, ?3, ?4);");
    s.BindI64(1, runId); s.BindInt(2, floorNum); s.BindText(3, floorName);
    s.BindI64(4, static_cast<int64_t>(enteredMs));
    s.Run();
    return sqlite3_last_insert_rowid(m_db);
}

void RunDatabase::SetFloorBossKilled(int64_t floorId, uint64_t killedMs) {
    Stmt s(m_db, "UPDATE floors SET boss_killed_at_ms=?2 WHERE id=?1;");
    s.BindI64(1, floorId); s.BindI64(2, static_cast<int64_t>(killedMs));
    s.Run();
}

void RunDatabase::EndFloor(int64_t floorId, uint64_t exitedMs, int roomsCleared) {
    // Floor duration = zone entry -> boss kill (spec); exit time when no kill.
    Stmt s(m_db, "UPDATE floors SET exited_at_ms=?2, rooms_cleared=?3, "
                 "duration_ms=COALESCE(boss_killed_at_ms, ?2)-entered_at_ms WHERE id=?1;");
    s.BindI64(1, floorId); s.BindI64(2, static_cast<int64_t>(exitedMs)); s.BindInt(3, roomsCleared);
    s.Run();
}

int64_t RunDatabase::BeginRoom(int64_t floorId, int layer, int roomIdx, const TrackerRoomInfo& info,
                               uint64_t enteredMs, bool isBoss) {
    if (!m_db) return -1;
    Stmt s(m_db, "INSERT INTO rooms (floor_id, layer, room_idx, room_type, affliction, reward, "
                 "entered_at_ms, is_boss) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);");
    s.BindI64(1, floorId); s.BindInt(2, layer); s.BindInt(3, roomIdx);
    s.BindText(4, info.type); s.BindText(5, info.affliction); s.BindText(6, info.reward);
    s.BindI64(7, static_cast<int64_t>(enteredMs)); s.BindInt(8, isBoss ? 1 : 0);
    s.Run();
    return sqlite3_last_insert_rowid(m_db);
}

void RunDatabase::RelabelRoom(int64_t roomId, int roomIdx, const TrackerRoomInfo& info) {
    Stmt s(m_db, "UPDATE rooms SET room_idx=?2, room_type=?3, affliction=?4, reward=?5 WHERE id=?1;");
    s.BindI64(1, roomId); s.BindInt(2, roomIdx);
    s.BindText(3, info.type); s.BindText(4, info.affliction); s.BindText(5, info.reward);
    s.Run();
}

void RunDatabase::UpdateRoomStart(int64_t roomId, uint64_t enteredMs) {
    Stmt s(m_db, "UPDATE rooms SET entered_at_ms=?2, "
                 "duration_ms=CASE WHEN cleared_at_ms IS NULL THEN NULL ELSE cleared_at_ms-?2 END "
                 "WHERE id=?1;");
    s.BindI64(1, roomId); s.BindI64(2, static_cast<int64_t>(enteredMs));
    s.Run();
}

void RunDatabase::EndRoom(int64_t roomId, uint64_t clearedMs) {
    Stmt s(m_db, "UPDATE rooms SET cleared_at_ms=?2, duration_ms=?2-entered_at_ms WHERE id=?1;");
    s.BindI64(1, roomId); s.BindI64(2, static_cast<int64_t>(clearedMs));
    s.Run();
}

std::vector<RunDatabase::RunRow> RunDatabase::QueryRuns(int limit) {
    std::vector<RunRow> out;
    Stmt s(m_db, "SELECT id, char_name, area_level, started_at_ms, duration_ms, status, "
                 "floors_done, last_floor, honour_end, started_at FROM runs "
                 "ORDER BY started_at_ms DESC LIMIT ?1;");
    s.BindInt(1, limit);
    while (s.Step()) {
        RunRow r;
        r.id = s.I64(0); r.charName = s.Text(1); r.areaLevel = s.Int(2);
        r.startedMs = static_cast<uint64_t>(s.I64(3));
        r.durationMs = s.Null(4) ? -1 : s.I64(4);
        r.status = s.Text(5); r.floorsDone = s.Int(6); r.lastFloor = s.Int(7);
        r.honourEnd = s.Int(8); r.startedText = s.Text(9);
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<RunDatabase::FloorRow> RunDatabase::QueryFloors(int64_t runId) {
    std::vector<FloorRow> out;
    Stmt s(m_db, "SELECT id, floor_num, floor_name, entered_at_ms, duration_ms, boss_killed_at_ms, "
                 "rooms_cleared FROM floors WHERE run_id=?1 ORDER BY entered_at_ms;");
    s.BindI64(1, runId);
    while (s.Step()) {
        FloorRow f;
        f.id = s.I64(0); f.floorNum = s.Int(1); f.floorName = s.Text(2);
        f.enteredMs = static_cast<uint64_t>(s.I64(3));
        f.durationMs = s.Null(4) ? -1 : s.I64(4);
        f.bossKilledMs = s.Null(5) ? -1 : s.I64(5);
        f.roomsCleared = s.Int(6);
        out.push_back(std::move(f));
    }
    return out;
}

std::vector<RunDatabase::RoomRow> RunDatabase::QueryRooms(int64_t floorId) {
    std::vector<RoomRow> out;
    Stmt s(m_db, "SELECT id, layer, room_idx, room_type, affliction, reward, duration_ms, is_boss "
                 "FROM rooms WHERE floor_id=?1 ORDER BY entered_at_ms;");
    s.BindI64(1, floorId);
    while (s.Step()) {
        RoomRow r;
        r.id = s.I64(0); r.layer = s.Int(1); r.roomIdx = s.Int(2);
        r.type = s.Text(3); r.affliction = s.Text(4); r.reward = s.Text(5);
        r.durationMs = s.Null(6) ? -1 : s.I64(6);
        r.isBoss = s.Int(7) != 0;
        out.push_back(std::move(r));
    }
    return out;
}

RunDatabase::Stats RunDatabase::QueryStats() {
    Stats st;
    Stmt s(m_db, "SELECT COUNT(*), "
                 "SUM(CASE WHEN status='completed' THEN 1 ELSE 0 END), "
                 "MIN(CASE WHEN status='completed' THEN duration_ms END), "
                 "CAST(AVG(CASE WHEN status='completed' THEN duration_ms END) AS INTEGER) "
                 "FROM runs;");
    if (s.Step()) {
        st.total = s.Int(0);
        st.completed = s.Null(1) ? 0 : s.Int(1);
        st.bestMs = s.Null(2) ? -1 : s.I64(2);
        st.avgMs  = s.Null(3) ? -1 : s.I64(3);
    }
    return st;
}

void RunDatabase::DeleteRun(int64_t runId) {
    // Explicit child deletes — independent of the foreign_keys pragma state.
    { Stmt s(m_db, "DELETE FROM rooms WHERE floor_id IN (SELECT id FROM floors WHERE run_id=?1);");
      s.BindI64(1, runId); s.Run(); }
    { Stmt s(m_db, "DELETE FROM floors WHERE run_id=?1;"); s.BindI64(1, runId); s.Run(); }
    { Stmt s(m_db, "DELETE FROM runs WHERE id=?1;");       s.BindI64(1, runId); s.Run(); }
}

void RunDatabase::ClearAll() {
    Exec("DELETE FROM rooms; DELETE FROM floors; DELETE FROM runs;");
}

} // namespace sekhema
