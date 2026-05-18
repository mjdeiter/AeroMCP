#include "gemini.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

using json = nlohmann::json;

GeminiAPI::GeminiAPI(const std::string& api_key) : api_key_(api_key) {}

std::string GeminiAPI::buildApiUrl() {
    return "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
}

// Curl write callback — accumulates full response body
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

void GeminiAPI::sendAsync(const std::vector<GeminiMessage>& history,
                          std::function<void(std::string, bool, std::string)> cb) {
    std::string key = api_key_;

    // Build JSON payload
    json contents = json::array();
    for (const auto& msg : history) {
        contents.push_back({
            {"role", msg.role},
            {"parts", {{ {"text", msg.content} }}}
        });
    }
    json payload = { {"contents", contents} };
    std::string body = payload.dump();

    std::thread([key, body, cb]() {
        CURL* curl = curl_easy_init();
        if (!curl) { cb("", true, "curl_easy_init failed"); return; }

        std::string response_buf;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string key_header = "X-goog-api-key: " + key;
        headers = curl_slist_append(headers, key_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, GeminiAPI::buildApiUrl().c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            cb("", true, std::string("curl error: ") + curl_easy_strerror(res));
            return;
        }

        // Parse response
        try {
            auto j = json::parse(response_buf);
            if (j.contains("error")) {
                cb("", true, j["error"]["message"].get<std::string>());
                return;
            }
            std::string text = j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
            cb(text, true, "");
        } catch (const std::exception& e) {
            cb("", true, std::string("parse error: ") + e.what() + "\nRaw: " + response_buf.substr(0, 200));
        }
    }).detach();
}
