#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "db.h"
#include "gemini.h"
#include "executor.h"
#include "config.h"

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <fstream>
#include <cmath>

// ─── App State ───────────────────────────────────────────────────────────────

static DB          g_db;
static GeminiAPI*  g_gemini  = nullptr;
static Config*     g_config  = nullptr;

static std::vector<ChatSession> g_sessions;
static int                      g_active_session = -1;
static std::vector<ChatMessage> g_messages;

// Reply streaming
static std::mutex        g_reply_mutex;
static std::string       g_reply_chunk;
static std::atomic<bool> g_reply_done{true};
static std::string       g_reply_error;
static bool              g_waiting_reply = false;
static std::string       g_pending_reply;

// Console
static std::mutex        g_exec_mutex;
static std::string       g_exec_output;
static std::atomic<bool> g_exec_running{false};

// Input
static char g_input_buf[4096]  = {};
static char g_api_key_buf[256] = {};
static char g_cmd_buf[1024]    = {};

// Layout
static float g_sidebar_w = 230.0f;

// Pending attachment
static std::string g_attach_b64;
static std::string g_attach_mime;
static std::string g_attach_name;

// ─── Military jet icon (32x32 RGBA) ─────────────────────────────────────────
static void setRadioWaveIcon(GLFWwindow* win) {
    static const unsigned char map[32][32] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0 },
        { 0, 0, 0, 2, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 0, 0, 0 },
        { 0, 0, 0, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
        { 0, 0, 0, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 },
        { 0, 0, 0, 2, 2, 2, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    };

    static unsigned char pixels[32 * 32 * 4];
    memset(pixels, 0, sizeof(pixels));

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int i = (y * 32 + x) * 4;
            switch (map[y][x]) {
                case 1: pixels[i+0]=150; pixels[i+1]=195; pixels[i+2]=225; pixels[i+3]=255; break; // fuselage
                case 2: pixels[i+0]=90;  pixels[i+1]=140; pixels[i+2]=185; pixels[i+3]=255; break; // wing
                case 3: pixels[i+0]=80;  pixels[i+1]=215; pixels[i+2]=245; pixels[i+3]=210; break; // canopy
                case 4: pixels[i+0]=100; pixels[i+1]=200; pixels[i+2]=255; pixels[i+3]=255; break; // tail
                default: break;
            }
        }
    }

    GLFWimage img;
    img.width  = 32;
    img.height = 32;
    img.pixels = pixels;
    glfwSetWindowIcon(win, 1, &img);
}

// ─── Base64 encode ────────────────────────────────────────────────────────────
static std::string base64Encode(const std::vector<unsigned char>& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned int v = data[i] << 16;
        if (i+1 < data.size()) v |= data[i+1] << 8;
        if (i+2 < data.size()) v |= data[i+2];
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += (i+1 < data.size()) ? tbl[(v >> 6) & 63] : '=';
        out += (i+2 < data.size()) ? tbl[v & 63]        : '=';
    }
    return out;
}

static bool loadFileAsBase64(const std::string& path, std::string& b64, std::string& mime) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
    // Detect mime from extension
    std::string ext = path.size() > 4 ? path.substr(path.rfind('.')) : "";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if      (ext == ".png")  mime = "image/png";
    else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
    else if (ext == ".gif")  mime = "image/gif";
    else if (ext == ".webp") mime = "image/webp";
    else if (ext == ".pdf")  mime = "application/pdf";
    else return false; // unsupported
    b64 = base64Encode(buf);
    return true;
}

// ─── Code block parsing ───────────────────────────────────────────────────────

static bool isExecutable(const std::string& lang) {
    static const std::vector<std::string> exec = {"bash","sh","shell","zsh","console","terminal",""};
    std::string l = lang;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    for (const auto& e : exec) if (l == e) return true;
    return false;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void refreshSessions() { g_sessions = g_db.getSessions(); }

static void loadSession(int id) {
    g_active_session = id;
    g_messages       = g_db.getMessages(id);
    g_pending_reply.clear();
    g_waiting_reply = false;
}

static void newSession() {
    int id = g_db.createSession("New Chat");
    refreshSessions();
    loadSession(id);
}

static void deleteSession(int id) {
    g_db.deleteSession(id);
    refreshSessions();
    if (g_active_session == id) { g_active_session = -1; g_messages.clear(); }
}

static const char* modelName(GeminiModel m) {
    switch(m) {
        case GeminiModel::Flash:    return "Gemini 2.5 Flash";
        case GeminiModel::Thinking: return "Gemini 2.5 Flash (Thinking)";
        case GeminiModel::Pro:      return "Gemini 2.5 Pro";
    }
    return "Unknown";
}

static std::vector<GeminiMessage> buildHistory() {
    GeminiModel m = g_gemini->getModel();
    std::vector<GeminiMessage> h;
    h.push_back({"user",
        std::string("You are AeroMCP, a native desktop AI assistant integrated into the ELITEBOOK system "
        "of Matt (Matthew Deiter), an Electronics Tech II on CachyOS. "
        "You are running on ") + modelName(m) + ". "
        "When asked what model you are, say \"" + modelName(m) + "\". "
        "You help with system administration, homelab tasks, and technical projects. "
        "When suggesting commands, wrap them in ```bash code blocks so they can be run directly. "
        "Be concise and technical."
    });
    h.push_back({"model", "Understood. AeroMCP ready."});
    for (const auto& msg : g_messages) {
        GeminiMessage gm;
        gm.role    = (msg.role == "user") ? "user" : "model";
        gm.content = msg.content;
        h.push_back(gm);
    }
    return h;
}

static void runInConsole(const std::string& cmd) {
    if (g_exec_running) return;
    g_exec_running = true;
    {
        std::lock_guard<std::mutex> lk(g_exec_mutex);
        g_exec_output = "$ " + cmd + "\n";
    }
    Executor::runAsync(cmd, [](ExecResult r) {
        std::lock_guard<std::mutex> lk(g_exec_mutex);
        if (!r.stdout_out.empty()) g_exec_output += r.stdout_out;
        if (!r.stderr_out.empty()) g_exec_output += "\n[stderr]\n" + r.stderr_out;
        g_exec_output += "\n[exit " + std::to_string(r.returncode) + "]";
        g_exec_running = false;
    });
}

static void sendMessage() {
    if (g_active_session < 0) newSession();
    std::string text(g_input_buf);
    if (text.empty() && g_attach_b64.empty()) return;
    memset(g_input_buf, 0, sizeof(g_input_buf));

    g_db.addMessage(g_active_session, "user", text.empty() ? "[image]" : text);

    for (auto& s : g_sessions) {
        if (s.id == g_active_session && s.title == "New Chat") {
            std::string title = text.empty() ? g_attach_name : text.substr(0, 42);
            g_db.updateSessionTitle(g_active_session, title);
            refreshSessions();
            break;
        }
    }
    g_messages = g_db.getMessages(g_active_session);
    g_pending_reply.clear();
    g_waiting_reply = true;
    g_reply_done    = false;
    g_reply_error.clear();

    auto history = buildHistory();
    // Attach image to last user message if present
    if (!g_attach_b64.empty()) {
        history.back().image_b64  = g_attach_b64;
        history.back().image_mime = g_attach_mime;
    }
    g_attach_b64.clear();
    g_attach_mime.clear();
    g_attach_name.clear();

    g_gemini->sendAsync(history, [](std::string chunk, bool done, std::string err) {
        std::lock_guard<std::mutex> lk(g_reply_mutex);
        if (!err.empty()) { g_reply_error = err; g_reply_done = true; return; }
        g_reply_chunk += chunk;
        if (done) g_reply_done = true;
    });
}

// ─── Render helpers ───────────────────────────────────────────────────────────

static void renderMessageContent(const std::string& content, int msg_id) {
    static const std::regex fence_re("```([a-zA-Z]*)\\n([\\s\\S]*?)```");
    std::string remaining = content;
    std::smatch m;
    int block_idx = 0;

    while (std::regex_search(remaining, m, fence_re)) {
        std::string before = m.prefix().str();
        if (!before.empty()) ImGui::TextWrapped("%s", before.c_str());

        std::string lang = m[1].str();
        std::string code = m[2].str();
        if (!code.empty() && code.back() == '\n') code.pop_back();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
        std::string child_id = "##cb_" + std::to_string(msg_id) + "_" + std::to_string(block_idx);
        float child_h = ImGui::GetTextLineHeight() * (std::count(code.begin(), code.end(), '\n') + 2) + 12.0f;
        child_h = std::min(child_h, 300.0f);
        ImGui::BeginChild(child_id.c_str(), ImVec2(-1, child_h), true);
        if (!lang.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.82f, 1.0f, 0.7f));
            ImGui::TextUnformatted(lang.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.95f, 0.75f, 1.0f));
        ImGui::TextUnformatted(code.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (isExecutable(lang)) {
            std::string btn_id  = "▶ Run##run_" + std::to_string(msg_id) + "_" + std::to_string(block_idx);
            std::string copy_id = "Copy##cp_"   + std::to_string(msg_id) + "_" + std::to_string(block_idx);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.28f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.42f, 0.25f, 1.0f));
            if (ImGui::SmallButton(btn_id.c_str())) runInConsole(code);
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            if (ImGui::SmallButton(copy_id.c_str())) ImGui::SetClipboardText(code.c_str());
        }

        remaining = m.suffix().str();
        block_idx++;
    }
    if (!remaining.empty()) ImGui::TextWrapped("%s", remaining.c_str());
}

// ─── Windows ──────────────────────────────────────────────────────────────────

static void renderSidebar(float h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(g_sidebar_w, h));
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.82f, 1.0f, 1.0f));
    ImGui::TextUnformatted("AeroMCP v1.1");
    ImGui::PopStyleColor();

    // Model selector (Pro requires billing)
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    GeminiModel cur = g_gemini->getModel();
    int sel = (int)cur;
    if (sel > 1) { sel = 0; g_gemini->setModel(GeminiModel::Flash); }
    const char* sel_label = sel == 0 ? "Fast" : "Thinking";
    if (ImGui::BeginCombo("##model", sel_label)) {
        if (ImGui::Selectable("Fast",     sel == 0)) { sel = 0; g_gemini->setModel(GeminiModel::Flash);    g_config->set("model", "0"); }
        if (ImGui::Selectable("Thinking", sel == 1)) { sel = 1; g_gemini->setModel(GeminiModel::Thinking); g_config->set("model", "1"); }
        ImGui::BeginDisabled(true);
        ImGui::Selectable("Pro (needs billing)", false);
        ImGui::EndDisabled();
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", modelName((GeminiModel)sel));
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("+ New Chat", ImVec2(-1, 0))) newSession();
    ImGui::Spacing();
    ImGui::TextDisabled("Sessions");
    ImGui::Spacing();

    for (const auto& s : g_sessions) {
        bool selected = (s.id == g_active_session);
        std::string lbl = s.title.substr(0, 28) + "##" + std::to_string(s.id);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.32f, 0.52f, 1.0f));
        if (ImGui::Selectable(lbl.c_str(), selected)) loadSession(s.id);
        if (selected) ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem(("ctx##" + std::to_string(s.id)).c_str())) {
            if (ImGui::MenuItem("Delete")) deleteSession(s.id);
            ImGui::EndPopup();
        }
    }

    // Sticky bottom: API key
    ImGui::SetCursorPosY(h - 80.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Gemini API Key");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##apikey", g_api_key_buf, sizeof(g_api_key_buf),
                         ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string k(g_api_key_buf);
        g_gemini->setApiKey(k);
        g_config->set("api_key", k);
    }

    ImGui::End();
}

static void renderChat(float x, float w, float chat_h) {
    ImGui::SetNextWindowPos(ImVec2(x, 0));
    ImGui::SetNextWindowSize(ImVec2(w, chat_h));
    ImGui::Begin("##chat", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Poll reply from worker thread
    if (g_waiting_reply) {
        std::lock_guard<std::mutex> lk(g_reply_mutex);
        if (!g_reply_chunk.empty()) { g_pending_reply += g_reply_chunk; g_reply_chunk.clear(); }
        if (g_reply_done) {
            g_waiting_reply = false;
            if (!g_reply_error.empty())
                g_db.addMessage(g_active_session, "assistant", "[ERROR] " + g_reply_error);
            else if (!g_pending_reply.empty())
                g_db.addMessage(g_active_session, "assistant", g_pending_reply);
            g_messages = g_db.getMessages(g_active_session);
            g_pending_reply.clear();
        }
    }

    for (const auto& msg : g_messages) {
        bool is_user = msg.role == "user";
        ImGui::PushStyleColor(ImGuiCol_Text,
            is_user ? ImVec4(0.48f, 0.82f, 1.0f, 1.0f) : ImVec4(0.56f, 0.93f, 0.56f, 1.0f));
        ImGui::TextUnformatted(is_user ? "You" : "AeroMCP");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", msg.created_at.substr(11, 5).c_str());
        renderMessageContent(msg.content, msg.id);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    if (g_waiting_reply && !g_pending_reply.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.56f, 0.93f, 0.56f, 1.0f));
        ImGui::TextUnformatted("AeroMCP");
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", g_pending_reply.c_str());
    }
    if (g_waiting_reply) {
        ImGui::Spacing();
        ImGui::TextDisabled("● thinking...");
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 40.0f)
        ImGui::SetScrollHereY(1.0f);
    ImGui::End();
}

static void renderInputBar(float x, float w, float y) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, 58.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.13f, 1.0f));
    ImGui::Begin("##input", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();

    // Attachment button
    ImGui::PushStyleColor(ImGuiCol_Button,
        g_attach_b64.empty() ? ImVec4(0.10f, 0.28f, 0.48f, 1.0f) : ImVec4(0.08f, 0.40f, 0.20f, 1.0f));
    if (ImGui::Button(g_attach_b64.empty() ? "📎" : ("📎 " + g_attach_name).c_str())) {
        // Open file dialog via zenity
        FILE* fp = popen("zenity --file-selection --title='Attach image' "
                         "--file-filter='Images | *.png *.jpg *.jpeg *.gif *.webp' 2>/dev/null", "r");
        if (fp) {
            char path[1024] = {};
            if (fgets(path, sizeof(path), fp)) {
                std::string p(path);
                if (!p.empty() && p.back() == '\n') p.pop_back();
                std::string b64, mime;
                if (loadFileAsBase64(p, b64, mime)) {
                    g_attach_b64  = b64;
                    g_attach_mime = mime;
                    // Extract filename
                    size_t sl = p.rfind('/');
                    g_attach_name = (sl != std::string::npos) ? p.substr(sl+1) : p;
                }
            }
            pclose(fp);
        }
    }
    ImGui::PopStyleColor();
    if (!g_attach_b64.empty() && ImGui::IsItemHovered())
        ImGui::SetTooltip("Click again to clear attachment");
    // Second click clears attachment
    if (!g_attach_b64.empty() && ImGui::IsItemClicked()) {
        g_attach_b64.clear(); g_attach_mime.clear(); g_attach_name.clear();
    }

    ImGui::SameLine();
    float btn_w  = 75.0f;
    ImGui::SetNextItemWidth(w - btn_w - 50.0f);
    bool send = ImGui::InputText("##msg", g_input_buf, sizeof(g_input_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(g_waiting_reply);
    if (ImGui::Button("Send", ImVec2(btn_w, 0)) || send) sendMessage();
    ImGui::EndDisabled();
    ImGui::End();
}

static void renderConsole(float x, float w, float y, float h) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin("Console", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::SetNextItemWidth(w - 120.0f);
    bool run = ImGui::InputText("##cmd", g_cmd_buf, sizeof(g_cmd_buf),
                                ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(g_exec_running);
    if ((ImGui::Button("Run") || run) && strlen(g_cmd_buf) > 0) {
        runInConsole(std::string(g_cmd_buf));
        memset(g_cmd_buf, 0, sizeof(g_cmd_buf));
    }
    ImGui::EndDisabled();
    if (g_exec_running) { ImGui::SameLine(); ImGui::TextDisabled("running..."); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        std::lock_guard<std::mutex> lk(g_exec_mutex);
        g_exec_output.clear();
    }

    ImGui::Separator();
    ImGui::BeginChild("##out", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lk(g_exec_mutex);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.95f, 0.65f, 1.0f));
        ImGui::TextUnformatted(g_exec_output.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

// ─── Sidebar drag handle ──────────────────────────────────────────────────────
static void renderDragHandle(float h) {
    // Invisible but hoverable 6px-wide strip at the sidebar edge
    ImGui::SetNextWindowPos(ImVec2(g_sidebar_w - 3.0f, 0));
    ImGui::SetNextWindowSize(ImVec2(6.0f, h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##drag", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::InvisibleButton("##dragbtn", ImVec2(6.0f, h));
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive()) {
        g_sidebar_w += ImGui::GetIO().MouseDelta.x;
        g_sidebar_w  = std::max(150.0f, std::min(g_sidebar_w, 400.0f));
    }
    // Draw highlight line in foreground draw list (screen space, no window clipping)
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddRectFilled(ImVec2(g_sidebar_w - 1.0f, 0), ImVec2(g_sidebar_w + 1.0f, h),
                          IM_COL32(80, 140, 220, 180));
    }
    ImGui::End();
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    g_config = new Config();

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1280, 800, "AeroMCP", nullptr, nullptr);
    if (!win) return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    setRadioWaveIcon(win);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImFontConfig fc;
    fc.OversampleH = 2; fc.OversampleV = 2;
    const char* font_path = "/usr/share/fonts/TTF/Hack-Regular.ttf";
    if (std::ifstream(font_path).good())
        io.Fonts->AddFontFromFileTTF(font_path, 15.0f, &fc);
    else
        io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 3.0f; st.FrameRounding = 3.0f; st.ScrollbarRounding = 3.0f;
    st.WindowPadding  = ImVec2(10, 8); st.ItemSpacing = ImVec2(8, 5);
    st.Colors[ImGuiCol_WindowBg]       = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    st.Colors[ImGuiCol_ChildBg]        = ImVec4(0.06f, 0.06f, 0.09f, 1.0f);
    st.Colors[ImGuiCol_FrameBg]        = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    st.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.16f, 0.22f, 1.0f);
    st.Colors[ImGuiCol_Button]         = ImVec4(0.10f, 0.28f, 0.48f, 1.0f);
    st.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.15f, 0.40f, 0.65f, 1.0f);
    st.Colors[ImGuiCol_Header]         = ImVec4(0.12f, 0.30f, 0.50f, 1.0f);
    st.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.18f, 0.40f, 0.60f, 1.0f);
    st.Colors[ImGuiCol_TitleBgActive]  = ImVec4(0.10f, 0.25f, 0.42f, 1.0f);
    st.Colors[ImGuiCol_SeparatorActive]= ImVec4(0.30f, 0.60f, 0.90f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::string home = g_config->get("home", getenv("HOME") ? getenv("HOME") : "/home/matt");
    g_db.open(home + "/.config/aeromcp/aeromcp.db");
    refreshSessions();

    std::string saved_key   = g_config->get("api_key", "");
    std::string saved_model = g_config->get("model", "0");
    g_gemini = new GeminiAPI(saved_key);
    g_gemini->setModel((GeminiModel)std::stoi(saved_model));
    if (!saved_key.empty())
        strncpy(g_api_key_buf, saved_key.c_str(), sizeof(g_api_key_buf) - 1);

    if (!g_sessions.empty()) loadSession(g_sessions[0].id);
    else newSession();

    const float CONSOLE_H = 230.0f;
    const float INPUT_H   = 58.0f;

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        float W = (float)fw, H = (float)fh;
        float chat_w = W - g_sidebar_w;
        float chat_h = H - CONSOLE_H - INPUT_H;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderSidebar(H);
        renderDragHandle(H);
        renderChat(g_sidebar_w, chat_w, chat_h);
        renderInputBar(g_sidebar_w, chat_w, chat_h);
        renderConsole(g_sidebar_w, chat_w, chat_h + INPUT_H, CONSOLE_H);

        ImGui::Render();
        glViewport(0, 0, fw, fh);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    delete g_gemini;
    delete g_config;
    return 0;
}
