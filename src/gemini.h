#pragma once
#include <string>
#include <vector>
#include <functional>
#include "db.h"

struct GeminiMessage {
    std::string role;    // "user" | "model"
    std::string content;
};

class GeminiAPI {
public:
    explicit GeminiAPI(const std::string& api_key);

    // Send conversation history, get reply. Calls cb with streamed chunks as they arrive.
    // Final cb call has done=true.
    void sendAsync(const std::vector<GeminiMessage>& history,
                   std::function<void(std::string chunk, bool done, std::string error)> cb);

    static std::string buildApiUrl();
    const std::string& apiKey() const { return api_key_; }
    void setApiKey(const std::string& k) { api_key_ = k; }

private:
    std::string api_key_;
};
