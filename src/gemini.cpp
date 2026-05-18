#include "gemini.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

using json = nlohmann::json;

GeminiAPI::GeminiAPI(const std::string& api_key) : api_key_(api_key) {}

std::string GeminiAPI::buildApiUrl(GeminiModel model) {
    switch (model) {
        case GeminiModel::Pro:
            return "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-pro:generateContent";
        case GeminiModel::Flash:
        case GeminiModel::Thinking:
        default:
            return "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
    }
}

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

void GeminiAPI::sendAsync(const std::vector<GeminiMessage>& history,
                          std::function<void(std::string, bool, std::string)> cb) {
    std::string key   = api_key_;
    GeminiModel model = model_;

    // Build JSON payload
    json contents = json::array();
    for (const auto& msg : history) {
        json parts = json::array();

        // Add image part if present
        if (!msg.image_b64.empty() && !msg.image_mime.empty()) {
            parts.push_back({
                {"inline_data", {
                    {"mime_type", msg.image_mime},
                    {"data",      msg.image_b64}
                }}
            });
        }

        // Text part
        parts.push_back({{"text", msg.content}});

        contents.push_back({
            {"role",  msg.role},
            {"parts", parts}
        });
    }

    json payload = {{"contents", contents}};

    // Native system instruction injection for self-identification
    std::string model_name = (model == GeminiModel::Pro) ? "Gemini 2.5 Pro" : "Gemini 2.5 Flash";
    payload["systemInstruction"] = {
        {"parts", {{{"text", "You are " + model_name + ", a helpful AI assistant integrated into the AeroMCP desktop environment."}}}}
    };

    // Add thinking config for Thinking mode
    if (model == GeminiModel::Thinking) {
        payload["generationConfig"] = {
            {"thinkingConfig", {{"thinkingBudget", -1}}}
        };
    }

    std::string body = payload.dump();
    std::string url  = buildApiUrl(model);

    std::thread([key, url, body, cb]() {
        CURL* curl = curl_easy_init();
        if (!curl) { cb("", true, "curl_easy_init failed"); return; }

        std::string response_buf;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string key_header = "X-goog-api-key: " + key;
        headers = curl_slist_append(headers, key_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response_buf);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,       120L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            cb("", true, std::string("curl error: ") + curl_easy_strerror(res));
            return;
        }

        try {
            auto j = json::parse(response_buf);
            if (j.contains("error")) {
                cb("", true, j["error"]["message"].get<std::string>());
                return;
            }
            // Extract text, skip thought parts
            std::string text;
            for (const auto& part : j["candidates"][0]["content"]["parts"]) {
                if (part.contains("text") && !part.contains("thought")) {
                    text += part["text"].get<std::string>();
                }
            }
            cb(text, true, "");
        } catch (const std::exception& e) {
            cb("", true, std::string("parse error: ") + e.what() + "\nRaw: " + response_buf.substr(0, 300));
        }
    }).detach();
                          }
