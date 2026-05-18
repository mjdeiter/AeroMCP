#pragma once
#include <string>
#include <vector>
#include <functional>
#include "db.h"

enum class GeminiModel {
    Flash,    // gemini-2.5-flash (fast)
    Thinking, // gemini-2.5-flash with thinking
    Pro       // gemini-2.5-pro
};

struct GeminiMessage {
    std::string role;    // "user" | "model"
    std::string content;
    // Optional image attachment (base64 encoded)
    std::string image_b64;
    std::string image_mime; // e.g. "image/png"
};

class GeminiAPI {
public:
    explicit GeminiAPI(const std::string& api_key);

    void sendAsync(const std::vector<GeminiMessage>& history,
                   std::function<void(std::string chunk, bool done, std::string error)> cb);

    static std::string buildApiUrl(GeminiModel model);
    const std::string& apiKey() const { return api_key_; }
    void setApiKey(const std::string& k) { api_key_ = k; }
    void setModel(GeminiModel m) { model_ = m; }
    GeminiModel getModel() const { return model_; }

private:
    std::string  api_key_;
    GeminiModel  model_ = GeminiModel::Flash;
};
