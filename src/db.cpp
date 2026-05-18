#include "db.h"
#include <stdexcept>
#include <cstring>

DB::DB() {}
DB::~DB() { if (db_) sqlite3_close(db_); }

bool DB::open(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) return false;
    exec("PRAGMA journal_mode=WAL;");
    exec(R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL DEFAULT 'New Chat',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id INTEGER NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE
        );
    )");
    return true;
}

void DB::exec(const std::string& sql) {
    char* err = nullptr;
    sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

int DB::createSession(const std::string& title) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "INSERT INTO sessions (title) VALUES (?);", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (int)sqlite3_last_insert_rowid(db_);
}

void DB::updateSessionTitle(int session_id, const std::string& title) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "UPDATE sessions SET title=? WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, session_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DB::deleteSession(int session_id) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "DELETE FROM sessions WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, session_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<ChatSession> DB::getSessions() {
    std::vector<ChatSession> out;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT id, title, created_at FROM sessions ORDER BY created_at DESC;", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChatSession s;
        s.id         = sqlite3_column_int(stmt, 0);
        s.title      = (const char*)sqlite3_column_text(stmt, 1);
        s.created_at = (const char*)sqlite3_column_text(stmt, 2);
        out.push_back(s);
    }
    sqlite3_finalize(stmt);
    return out;
}

void DB::addMessage(int session_id, const std::string& role, const std::string& content) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_,
        "INSERT INTO messages (session_id, role, content) VALUES (?, ?, ?);",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt,  1, session_id);
    sqlite3_bind_text(stmt, 2, role.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<ChatMessage> DB::getMessages(int session_id) {
    std::vector<ChatMessage> out;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_,
        "SELECT id, session_id, role, content, created_at FROM messages WHERE session_id=? ORDER BY id ASC;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, session_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChatMessage m;
        m.id         = sqlite3_column_int(stmt, 0);
        m.session_id = sqlite3_column_int(stmt, 1);
        m.role       = (const char*)sqlite3_column_text(stmt, 2);
        m.content    = (const char*)sqlite3_column_text(stmt, 3);
        m.created_at = (const char*)sqlite3_column_text(stmt, 4);
        out.push_back(m);
    }
    sqlite3_finalize(stmt);
    return out;
}
