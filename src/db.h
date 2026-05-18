#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>

struct ChatSession {
    int id;
    std::string title;
    std::string created_at;
};

struct ChatMessage {
    int id;
    int session_id;
    std::string role;   // "user" | "assistant" | "system"
    std::string content;
    std::string created_at;
};

class DB {
public:
    DB();
    ~DB();
    bool open(const std::string& path);

    // Sessions
    int  createSession(const std::string& title);
    void updateSessionTitle(int session_id, const std::string& title);
    void deleteSession(int session_id);
    std::vector<ChatSession> getSessions();

    // Messages
    void addMessage(int session_id, const std::string& role, const std::string& content);
    std::vector<ChatMessage> getMessages(int session_id);

private:
    sqlite3* db_ = nullptr;
    void exec(const std::string& sql);
};
