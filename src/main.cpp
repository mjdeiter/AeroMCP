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

// ─── Code block parsing ───────────────────────────────────────────────────────

struct CodeBlock {
    std::string lang;
    std::string code;
};

static std::vector<CodeBlock> extractCodeBlocks(const std::string& text) {
    std::vector<CodeBlock> blocks;
    static const std::regex re("```([a-zA-Z]*)\\n([\\s\\S]*?)```");
    auto begin = std::sregex_iterator(text.begin(), text.end(), re);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        CodeBlock b;
        b.lang = (*it)[1].str();
        b.code = (*it)[2].str();
        // trim trailing newline
        if (!b.code.empty() && b.code.back() == '\n') b.code.pop_back();
        blocks.push_back(b);
    }
    return blocks;
}

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

static std::vector<GeminiMessage> buildHistory() {
    std::vector<GeminiMessage> h;
    h.push_back({"user",
        "You are AeroMCP, a native desktop AI assistant integrated into the ELITEBOOK system "
        "of Matt (Matthew Deiter), an Electronics Tech II on CachyOS. You help with system "
        "administration, homelab tasks, and technical projects. When suggesting commands, wrap "
        "them in ```bash code blocks so they can be run directly. Be concise and technical."});
    h.push_back({"model", "Understood. AeroMCP ready."});
    for (const auto& m : g_messages) {
        h.push_back({m.role == "user" ? "user" : "model", m.content});
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
    if (text.empty()) return;
    memset(g_input_buf, 0, sizeof(g_input_buf));

    g_db.addMessage(g_active_session, "user", text);

    for (auto& s : g_sessions) {
        if (s.id == g_active_session && s.title == "New Chat") {
            g_db.updateSessionTitle(g_active_session, text.substr(0, 42));
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
    g_gemini->sendAsync(history, [](std::string chunk, bool done, std::string err) {
        std::lock_guard<std::mutex> lk(g_reply_mutex);
        if (!err.empty()) { g_reply_error = err; g_reply_done = true; return; }
        g_reply_chunk += chunk;
        if (done) g_reply_done = true;
    });
}

// ─── Render helpers ───────────────────────────────────────────────────────────

// Render message content: plain text segments + code blocks with Run buttons
static void renderMessageContent(const std::string& content, int msg_id) {
    static const std::regex fence_re("```([a-zA-Z]*)\\n([\\s\\S]*?)```");
    std::string remaining = content;
    std::smatch m;
    int block_idx = 0;

    while (std::regex_search(remaining, m, fence_re)) {
        // Plain text before block
        std::string before = m.prefix().str();
        if (!before.empty()) ImGui::TextWrapped("%s", before.c_str());

        std::string lang = m[1].str();
        std::string code = m[2].str();
        if (!code.empty() && code.back() == '\n') code.pop_back();

        // Code block frame
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

        // Run button (only for shell-like blocks)
        if (isExecutable(lang)) {
            std::string btn_id = "▶ Run##run_" + std::to_string(msg_id) + "_" + std::to_string(block_idx);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.28f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.42f, 0.25f, 1.0f));
            if (ImGui::SmallButton(btn_id.c_str())) runInConsole(code);
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            std::string copy_id = "Copy##cp_" + std::to_string(msg_id) + "_" + std::to_string(block_idx);
            if (ImGui::SmallButton(copy_id.c_str())) ImGui::SetClipboardText(code.c_str());
        }

        remaining = m.suffix().str();
        block_idx++;
    }
    if (!remaining.empty()) ImGui::TextWrapped("%s", remaining.c_str());
}

// ─── Windows ──────────────────────────────────────────────────────────────────

static void renderSidebar(float w, float h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.82f, 1.0f, 1.0f));
    ImGui::TextUnformatted("AeroMCP v1.0");
    ImGui::PopStyleColor();
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

    // In-progress reply
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

    bool send = false;
    ImGui::SetNextItemWidth(w - 90.0f);
    if (!g_waiting_reply) ImGui::SetKeyboardFocusHere();
    send = ImGui::InputText("##msg", g_input_buf, sizeof(g_input_buf),
                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(g_waiting_reply);
    if (ImGui::Button("Send", ImVec2(75, 0)) || send) sendMessage();
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

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    // Config
    g_config = new Config();

    // GLFW
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(1280, 800, "AeroMCP", nullptr, nullptr);
    if (!win) return 1;
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Font — Hack Regular 15px
    ImFontConfig fc;
    fc.OversampleH = 2; fc.OversampleV = 2;
    const char* font_path = "/usr/share/fonts/TTF/Hack-Regular.ttf";
    if (std::ifstream(font_path).good())
        io.Fonts->AddFontFromFileTTF(font_path, 15.0f, &fc);
    else
        io.Fonts->AddFontDefault();

    // Style
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

    // DB
    std::string home = g_config->get("home", getenv("HOME") ? getenv("HOME") : "/home/matt");
    g_db.open(home + "/.config/aeromcp/aeromcp.db");
    refreshSessions();

    // Gemini — restore saved API key
    std::string saved_key = g_config->get("api_key", "");
    g_gemini = new GeminiAPI(saved_key);
    if (!saved_key.empty())
        strncpy(g_api_key_buf, saved_key.c_str(), sizeof(g_api_key_buf) - 1);

    if (!g_sessions.empty()) loadSession(g_sessions[0].id);
    else newSession();

    const float SIDEBAR_W = 230.0f;
    const float CONSOLE_H = 230.0f;
    const float INPUT_H   = 58.0f;

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        float W = (float)fw, H = (float)fh;
        float chat_w  = W - SIDEBAR_W;
        float chat_h  = H - CONSOLE_H - INPUT_H;
        float input_y = chat_h;
        float cons_y  = chat_h + INPUT_H;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderSidebar(SIDEBAR_W, H);
        renderChat(SIDEBAR_W, chat_w, chat_h);
        renderInputBar(SIDEBAR_W, chat_w, input_y);
        renderConsole(SIDEBAR_W, chat_w, cons_y, CONSOLE_H);

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
