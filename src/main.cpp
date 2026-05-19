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

// ─── AeroMCP logo icon (32x32 RGBA) ─────────────────────────────────────────
static void setRadioWaveIcon(GLFWwindow* win) {
    static const unsigned char pixels[32 * 32 * 4] = {
    90, 90, 90, 255, 175, 175, 175, 255, 167, 167, 167, 255, 169, 169, 169, 255,
    170, 170, 170, 255, 170, 170, 170, 255, 171, 171, 170, 255, 171, 171, 170, 255,
    171, 171, 170, 255, 172, 172, 172, 255, 172, 172, 172, 255, 172, 172, 172, 255,
    172, 172, 172, 255, 172, 172, 172, 255, 172, 172, 172, 255, 172, 172, 172, 255,
    172, 172, 172, 255, 172, 172, 172, 255, 172, 172, 172, 255, 172, 172, 172, 255,
    172, 172, 172, 255, 172, 172, 172, 255, 172, 172, 172, 255, 171, 171, 171, 255,
    171, 171, 170, 255, 171, 171, 170, 255, 171, 171, 170, 255, 171, 171, 170, 255,
    169, 169, 169, 255, 168, 168, 168, 255, 176, 176, 175, 255, 112, 112, 111, 255,
    193, 193, 193, 255, 255, 255, 255, 255, 250, 250, 250, 255, 253, 253, 253, 255,
    254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 254, 254, 255, 252, 252, 252, 255, 255, 255, 255, 255, 223, 223, 223, 255,
    190, 190, 190, 255, 247, 247, 247, 255, 238, 238, 238, 255, 241, 241, 241, 255,
    241, 241, 241, 255, 243, 243, 243, 255, 244, 244, 244, 255, 244, 244, 244, 255,
    244, 244, 244, 255, 244, 244, 243, 255, 244, 244, 244, 255, 245, 245, 245, 255,
    245, 245, 245, 255, 245, 245, 245, 255, 245, 245, 245, 255, 246, 246, 246, 255,
    245, 246, 246, 255, 246, 246, 246, 255, 245, 245, 245, 255, 244, 244, 244, 255,
    245, 245, 245, 255, 245, 245, 245, 255, 245, 245, 245, 255, 244, 244, 244, 255,
    244, 244, 244, 255, 244, 244, 244, 255, 244, 244, 244, 255, 244, 244, 244, 255,
    242, 242, 242, 255, 239, 239, 239, 255, 243, 243, 243, 255, 217, 217, 217, 255,
    190, 190, 190, 255, 251, 251, 251, 255, 241, 241, 241, 255, 244, 244, 244, 255,
    245, 245, 245, 255, 246, 246, 246, 255, 246, 246, 246, 255, 246, 246, 246, 255,
    246, 246, 246, 255, 247, 247, 247, 255, 247, 247, 247, 255, 248, 248, 248, 255,
    248, 248, 248, 255, 249, 249, 249, 255, 248, 249, 249, 255, 251, 249, 250, 255,
    251, 249, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255, 248, 248, 248, 255,
    248, 248, 248, 255, 248, 248, 248, 255, 247, 247, 247, 255, 245, 246, 246, 255,
    245, 246, 246, 255, 246, 246, 246, 255, 244, 244, 244, 255, 244, 244, 244, 255,
    244, 244, 244, 255, 243, 243, 243, 255, 247, 247, 247, 255, 218, 218, 218, 255,
    191, 191, 191, 255, 251, 251, 251, 255, 243, 243, 243, 255, 245, 245, 245, 255,
    245, 245, 245, 255, 246, 246, 246, 255, 246, 246, 246, 255, 246, 246, 246, 255,
    246, 246, 246, 255, 247, 247, 247, 255, 248, 248, 248, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 251, 250, 249, 255, 239, 246, 245, 255,
    239, 246, 244, 255, 251, 250, 249, 255, 249, 249, 249, 255, 248, 248, 248, 255,
    248, 248, 249, 255, 248, 248, 249, 255, 245, 246, 247, 255, 250, 248, 248, 255,
    252, 250, 248, 255, 245, 246, 246, 255, 244, 244, 244, 255, 244, 244, 244, 255,
    244, 244, 244, 255, 243, 243, 243, 255, 247, 247, 247, 255, 218, 218, 218, 255,
    192, 192, 191, 255, 252, 252, 252, 255, 243, 243, 243, 255, 246, 246, 246, 255,
    246, 246, 246, 255, 246, 246, 246, 255, 246, 246, 246, 255, 247, 247, 247, 255,
    247, 247, 247, 255, 248, 248, 248, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 247, 248, 249, 255, 255, 253, 253, 255, 173, 227, 234, 255,
    158, 217, 225, 255, 255, 255, 253, 255, 246, 248, 248, 255, 250, 249, 249, 255,
    249, 249, 248, 255, 248, 248, 248, 255, 255, 253, 251, 255, 232, 246, 244, 255,
    215, 224, 228, 255, 251, 249, 249, 255, 244, 244, 244, 255, 244, 244, 244, 255,
    244, 244, 244, 255, 243, 243, 243, 255, 248, 248, 248, 255, 219, 219, 219, 255,
    191, 191, 191, 255, 253, 253, 253, 255, 244, 244, 244, 255, 246, 246, 246, 255,
    246, 246, 246, 255, 246, 246, 246, 255, 247, 247, 247, 255, 248, 248, 248, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    248, 249, 249, 255, 250, 248, 249, 255, 250, 251, 251, 255, 119, 211, 229, 255,
    75, 172, 198, 255, 251, 251, 251, 255, 249, 250, 249, 255, 250, 248, 249, 255,
    251, 250, 248, 255, 255, 249, 247, 255, 219, 249, 255, 255, 107, 166, 187, 255,
    213, 218, 223, 255, 253, 253, 251, 255, 244, 244, 245, 255, 244, 244, 244, 255,
    246, 246, 246, 255, 245, 245, 245, 255, 249, 249, 249, 255, 221, 221, 220, 255,
    192, 192, 192, 255, 253, 253, 253, 255, 244, 244, 244, 255, 246, 246, 246, 255,
    245, 245, 245, 255, 247, 247, 247, 255, 247, 247, 247, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    248, 249, 250, 255, 254, 253, 252, 255, 213, 238, 240, 255, 85, 188, 216, 255,
    29, 121, 160, 255, 202, 223, 229, 255, 254, 255, 254, 255, 255, 249, 250, 255,
    241, 248, 251, 255, 192, 245, 253, 255, 62, 163, 189, 255, 106, 140, 160, 255,
    255, 255, 255, 255, 245, 244, 245, 255, 247, 247, 247, 255, 245, 245, 245, 255,
    246, 246, 246, 255, 245, 245, 245, 255, 249, 249, 249, 255, 221, 221, 220, 255,
    192, 192, 192, 255, 253, 253, 253, 255, 244, 244, 244, 255, 246, 246, 246, 255,
    246, 246, 246, 255, 247, 247, 247, 255, 248, 248, 248, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 249, 250, 249, 255, 250, 250, 250, 255,
    250, 250, 251, 255, 255, 255, 255, 255, 149, 207, 223, 255, 60, 154, 194, 255,
    0, 72, 119, 255, 106, 147, 170, 255, 255, 255, 255, 255, 208, 240, 241, 255,
    137, 220, 233, 255, 59, 189, 206, 255, 20, 78, 113, 255, 218, 224, 227, 255,
    253, 253, 252, 255, 249, 246, 247, 255, 247, 247, 247, 255, 246, 246, 246, 255,
    246, 246, 246, 255, 245, 245, 245, 255, 248, 248, 248, 255, 223, 223, 223, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 244, 244, 244, 255, 247, 247, 247, 255,
    247, 247, 247, 255, 248, 248, 248, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 250, 250, 255, 250, 252, 251, 255, 252, 252, 252, 255,
    255, 254, 254, 255, 237, 245, 245, 255, 73, 162, 200, 255, 21, 105, 168, 255,
    33, 77, 123, 255, 152, 183, 200, 255, 171, 225, 236, 255, 90, 198, 218, 255,
    115, 224, 230, 255, 33, 105, 135, 255, 100, 120, 150, 255, 255, 255, 255, 255,
    246, 246, 246, 255, 250, 249, 249, 255, 247, 247, 247, 255, 247, 247, 247, 255,
    246, 246, 246, 255, 245, 245, 245, 255, 250, 250, 251, 255, 221, 221, 221, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 244, 244, 244, 255, 247, 247, 247, 255,
    247, 247, 247, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 250, 249, 255, 251, 254, 253, 255, 252, 254, 253, 255, 250, 251, 252, 255,
    255, 255, 255, 255, 160, 196, 217, 255, 22, 108, 174, 255, 99, 153, 191, 255,
    170, 211, 226, 255, 131, 218, 233, 255, 114, 207, 222, 255, 68, 198, 213, 255,
    151, 208, 217, 255, 59, 90, 126, 255, 203, 217, 223, 255, 254, 253, 253, 255,
    247, 247, 248, 255, 249, 249, 249, 255, 248, 248, 248, 255, 247, 247, 247, 255,
    246, 246, 246, 255, 245, 245, 245, 255, 250, 250, 251, 255, 221, 221, 221, 255,
    195, 195, 195, 255, 255, 255, 255, 255, 244, 244, 244, 255, 246, 246, 246, 255,
    248, 248, 248, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 251, 250, 255, 251, 253, 253, 255, 252, 253, 253, 255, 251, 252, 252, 255,
    249, 249, 250, 255, 105, 155, 189, 255, 129, 185, 210, 255, 149, 218, 235, 255,
    86, 190, 219, 255, 51, 173, 201, 255, 139, 218, 228, 255, 136, 194, 208, 255,
    20, 51, 99, 255, 92, 116, 148, 255, 255, 255, 255, 255, 249, 249, 246, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 248, 248, 248, 255, 248, 248, 248, 255,
    246, 246, 246, 255, 245, 245, 245, 255, 250, 250, 251, 255, 221, 221, 221, 255,
    199, 199, 199, 255, 254, 254, 254, 255, 244, 244, 244, 255, 247, 247, 247, 255,
    248, 248, 248, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    252, 252, 252, 255, 252, 252, 253, 255, 251, 252, 253, 255, 255, 255, 255, 255,
    240, 244, 244, 255, 149, 203, 218, 255, 100, 189, 214, 255, 53, 166, 202, 255,
    33, 162, 198, 255, 91, 190, 209, 255, 109, 193, 207, 255, 66, 129, 156, 255,
    9, 57, 101, 255, 207, 215, 223, 255, 255, 254, 255, 255, 249, 251, 250, 255,
    250, 249, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    247, 247, 247, 255, 245, 245, 245, 255, 250, 250, 250, 255, 225, 225, 225, 255,
    195, 195, 195, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    248, 248, 248, 255, 249, 249, 249, 255, 249, 249, 249, 255, 250, 250, 250, 255,
    249, 250, 250, 255, 255, 254, 254, 255, 255, 255, 255, 255, 204, 232, 235, 255,
    96, 177, 207, 255, 52, 158, 198, 255, 27, 147, 189, 255, 92, 175, 201, 255,
    133, 201, 218, 255, 103, 199, 214, 255, 87, 131, 160, 255, 23, 113, 145, 255,
    98, 145, 167, 255, 252, 252, 251, 255, 249, 252, 252, 255, 252, 253, 254, 255,
    251, 251, 251, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    247, 247, 247, 255, 245, 245, 245, 255, 250, 250, 251, 255, 223, 223, 223, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    248, 248, 248, 255, 249, 249, 249, 255, 247, 248, 248, 255, 248, 249, 249, 255,
    255, 255, 255, 255, 242, 250, 248, 255, 129, 188, 210, 255, 38, 136, 180, 255,
    43, 142, 189, 255, 51, 152, 194, 255, 18, 137, 182, 255, 193, 228, 237, 255,
    224, 246, 249, 255, 83, 135, 168, 255, 28, 85, 123, 255, 17, 74, 116, 255,
    194, 199, 206, 255, 255, 255, 255, 255, 248, 251, 251, 255, 253, 253, 254, 255,
    252, 251, 252, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    247, 247, 247, 255, 245, 245, 245, 255, 250, 250, 251, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    249, 249, 249, 255, 249, 248, 248, 255, 255, 255, 253, 255, 255, 255, 255, 255,
    177, 208, 222, 255, 52, 130, 181, 255, 28, 116, 168, 255, 85, 154, 190, 255,
    83, 157, 190, 255, 28, 142, 180, 255, 63, 156, 194, 255, 226, 235, 238, 255,
    160, 178, 188, 255, 46, 82, 113, 255, 19, 92, 130, 255, 22, 81, 125, 255,
    189, 198, 212, 255, 254, 254, 254, 255, 250, 250, 251, 255, 253, 253, 253, 255,
    252, 252, 252, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 250, 250, 250, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    248, 248, 248, 255, 250, 249, 249, 255, 211, 225, 232, 255, 94, 148, 187, 255,
    34, 107, 162, 255, 83, 147, 186, 255, 113, 165, 199, 255, 78, 143, 180, 255,
    40, 147, 181, 255, 17, 143, 186, 255, 159, 207, 227, 255, 123, 157, 175, 255,
    123, 153, 173, 255, 155, 186, 203, 255, 18, 101, 143, 255, 19, 86, 133, 255,
    97, 130, 155, 255, 255, 255, 255, 255, 250, 250, 250, 255, 251, 254, 253, 255,
    251, 254, 253, 255, 250, 250, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 250, 250, 250, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    250, 250, 250, 255, 241, 243, 244, 255, 165, 188, 205, 255, 171, 195, 216, 255,
    197, 211, 223, 255, 137, 171, 193, 255, 113, 161, 191, 255, 98, 176, 207, 255,
    48, 166, 205, 255, 41, 137, 180, 255, 187, 204, 216, 255, 159, 173, 185, 255,
    227, 234, 236, 255, 94, 118, 142, 255, 21, 71, 119, 255, 39, 141, 174, 255,
    46, 117, 146, 255, 230, 229, 233, 255, 255, 255, 254, 255, 250, 253, 254, 255,
    251, 253, 253, 255, 251, 250, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 250, 250, 250, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    241, 248, 247, 255, 245, 248, 251, 255, 187, 225, 231, 255, 36, 156, 198, 255,
    23, 126, 173, 255, 160, 192, 209, 255, 173, 176, 193, 255, 241, 246, 250, 255,
    109, 130, 154, 255, 150, 162, 176, 255, 75, 110, 142, 255, 27, 103, 148, 255,
    12, 82, 125, 255, 160, 173, 188, 255, 255, 255, 255, 255, 247, 249, 252, 255,
    252, 252, 254, 255, 251, 250, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 251, 251, 251, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 248, 248, 248, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 248, 248, 249, 255, 250, 251, 251, 255,
    252, 254, 254, 255, 255, 255, 255, 255, 93, 172, 199, 255, 8, 116, 167, 255,
    123, 167, 190, 255, 180, 188, 202, 255, 181, 199, 210, 255, 94, 114, 145, 255,
    87, 110, 142, 255, 44, 105, 146, 255, 21, 75, 120, 255, 29, 114, 151, 255,
    27, 104, 146, 255, 74, 105, 137, 255, 255, 255, 254, 255, 251, 252, 253, 255,
    253, 252, 252, 255, 249, 251, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 251, 251, 251, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 250, 250, 250, 255, 250, 252, 252, 255,
    254, 254, 255, 255, 219, 236, 244, 255, 15, 113, 171, 255, 98, 154, 191, 255,
    139, 163, 189, 255, 35, 75, 120, 255, 58, 82, 120, 255, 182, 196, 208, 255,
    172, 184, 197, 255, 21, 85, 123, 255, 13, 85, 132, 255, 28, 94, 138, 255,
    20, 82, 126, 255, 23, 75, 120, 255, 212, 220, 225, 255, 255, 255, 255, 255,
    250, 250, 252, 255, 249, 250, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 251, 251, 251, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 250, 250, 250, 255, 248, 251, 252, 255,
    255, 255, 255, 255, 128, 172, 206, 255, 52, 120, 172, 255, 112, 142, 166, 255,
    6, 43, 88, 255, 76, 105, 135, 255, 217, 222, 230, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 221, 225, 228, 255, 92, 117, 147, 255, 19, 79, 122, 255,
    20, 80, 125, 255, 5, 69, 118, 255, 131, 149, 170, 255, 255, 255, 255, 255,
    248, 249, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 251, 251, 251, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 245, 245, 245, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 248, 250, 250, 255, 253, 254, 254, 255,
    240, 247, 249, 255, 84, 135, 175, 255, 122, 155, 181, 255, 10, 45, 88, 255,
    74, 110, 138, 255, 255, 255, 255, 255, 255, 255, 254, 255, 251, 252, 253, 255,
    252, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 84, 115, 147, 255,
    8, 70, 120, 255, 16, 90, 139, 255, 65, 113, 151, 255, 250, 248, 249, 255,
    253, 253, 253, 255, 249, 249, 250, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    249, 249, 249, 255, 246, 246, 246, 255, 251, 251, 251, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 244, 244, 244, 255, 248, 248, 248, 255,
    249, 249, 249, 255, 249, 249, 249, 255, 247, 248, 248, 255, 255, 255, 255, 255,
    194, 209, 219, 255, 105, 137, 165, 255, 18, 52, 99, 255, 0, 26, 85, 255,
    177, 191, 207, 255, 254, 254, 254, 255, 249, 250, 249, 255, 255, 255, 255, 255,
    255, 255, 254, 255, 250, 250, 250, 255, 253, 254, 254, 255, 174, 186, 203, 255,
    0, 46, 103, 255, 5, 81, 131, 255, 0, 52, 106, 255, 181, 188, 199, 255,
    255, 255, 255, 255, 246, 247, 249, 255, 249, 249, 249, 255, 249, 249, 249, 255,
    248, 248, 248, 255, 246, 246, 246, 255, 250, 250, 250, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 244, 244, 244, 255, 247, 247, 247, 255,
    246, 246, 247, 255, 248, 249, 249, 255, 248, 248, 249, 255, 249, 249, 249, 255,
    225, 229, 234, 255, 138, 154, 177, 255, 95, 120, 155, 255, 117, 137, 166, 255,
    239, 241, 242, 255, 254, 254, 255, 255, 251, 252, 253, 255, 251, 252, 253, 255,
    254, 254, 255, 255, 253, 253, 253, 255, 253, 253, 253, 255, 241, 243, 244, 255,
    118, 146, 170, 255, 105, 137, 165, 255, 98, 119, 149, 255, 170, 186, 195, 255,
    250, 251, 251, 255, 246, 247, 248, 255, 247, 247, 248, 255, 246, 247, 247, 255,
    246, 246, 246, 255, 246, 246, 246, 255, 250, 250, 250, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 244, 244, 244, 255, 246, 246, 246, 255,
    255, 254, 253, 255, 251, 250, 250, 255, 250, 249, 250, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 251, 252, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 253, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 253, 252, 249, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    252, 251, 250, 255, 245, 245, 245, 255, 250, 250, 250, 255, 222, 222, 222, 255,
    194, 194, 194, 255, 255, 255, 255, 255, 243, 243, 244, 255, 250, 249, 249, 255,
    213, 219, 222, 255, 243, 243, 247, 255, 245, 247, 247, 255, 189, 199, 210, 255,
    171, 180, 197, 255, 213, 219, 227, 255, 207, 216, 221, 255, 171, 181, 195, 255,
    226, 231, 239, 255, 255, 255, 255, 255, 222, 226, 234, 255, 169, 178, 194, 255,
    243, 248, 248, 255, 242, 241, 244, 255, 219, 223, 228, 255, 253, 252, 253, 255,
    245, 247, 248, 255, 205, 214, 222, 255, 254, 255, 255, 255, 206, 213, 225, 255,
    169, 182, 200, 255, 237, 243, 245, 255, 207, 209, 222, 255, 173, 189, 201, 255,
    223, 228, 235, 255, 253, 250, 251, 255, 250, 249, 251, 255, 222, 222, 221, 255,
    194, 194, 194, 255, 254, 254, 255, 255, 247, 246, 245, 255, 244, 243, 247, 255,
    39, 66, 105, 255, 182, 197, 208, 255, 228, 233, 236, 255, 68, 95, 129, 255,
    140, 152, 177, 255, 193, 199, 210, 255, 89, 115, 141, 255, 105, 123, 153, 255,
    101, 120, 151, 255, 206, 212, 219, 255, 82, 109, 133, 255, 104, 122, 147, 255,
    111, 133, 157, 255, 179, 193, 205, 255, 41, 72, 107, 255, 255, 255, 255, 255,
    177, 185, 201, 255, 62, 91, 126, 255, 198, 206, 217, 255, 40, 73, 108, 255,
    51, 80, 116, 255, 134, 150, 173, 255, 65, 95, 130, 255, 55, 86, 118, 255,
    64, 92, 125, 255, 229, 231, 236, 255, 252, 252, 252, 255, 221, 221, 221, 255,
    194, 194, 194, 255, 252, 253, 253, 255, 254, 254, 252, 255, 184, 193, 205, 255,
    112, 138, 164, 255, 144, 166, 182, 255, 224, 226, 231, 255, 84, 111, 141, 255,
    173, 185, 196, 255, 218, 226, 228, 255, 115, 138, 165, 255, 255, 255, 255, 255,
    130, 148, 168, 255, 89, 116, 144, 255, 172, 188, 199, 255, 255, 255, 255, 255,
    131, 148, 172, 255, 131, 152, 173, 255, 5, 41, 92, 255, 177, 197, 208, 255,
    81, 109, 140, 255, 67, 100, 134, 255, 102, 123, 150, 255, 161, 171, 190, 255,
    255, 255, 255, 255, 225, 228, 232, 255, 70, 101, 134, 255, 242, 248, 254, 255,
    67, 100, 129, 255, 181, 192, 200, 255, 255, 255, 255, 255, 220, 220, 221, 255,
    193, 193, 193, 255, 253, 252, 253, 255, 255, 255, 255, 255, 109, 132, 155, 255,
    105, 126, 152, 255, 79, 109, 139, 255, 201, 209, 217, 255, 83, 112, 140, 255,
    163, 172, 187, 255, 226, 230, 235, 255, 88, 118, 146, 255, 63, 88, 123, 255,
    111, 134, 162, 255, 131, 155, 173, 255, 175, 190, 204, 255, 255, 255, 255, 255,
    142, 157, 178, 255, 116, 137, 161, 255, 86, 115, 146, 255, 48, 76, 113, 255,
    65, 95, 129, 255, 108, 135, 161, 255, 85, 110, 141, 255, 177, 192, 207, 255,
    255, 255, 255, 255, 244, 245, 243, 255, 59, 86, 125, 255, 42, 70, 110, 255,
    75, 99, 125, 255, 235, 238, 244, 255, 251, 251, 253, 255, 220, 221, 220, 255,
    193, 193, 193, 255, 255, 255, 255, 255, 227, 229, 233, 255, 86, 109, 142, 255,
    168, 176, 193, 255, 110, 130, 156, 255, 124, 144, 164, 255, 95, 117, 147, 255,
    165, 179, 193, 255, 191, 199, 206, 255, 107, 128, 157, 255, 177, 188, 204, 255,
    116, 132, 156, 255, 204, 213, 217, 255, 86, 107, 136, 255, 139, 152, 172, 255,
    103, 122, 146, 255, 159, 177, 192, 255, 113, 137, 161, 255, 138, 152, 172, 255,
    153, 166, 185, 255, 102, 124, 151, 255, 170, 183, 196, 255, 46, 82, 115, 255,
    89, 114, 148, 255, 133, 150, 173, 255, 72, 99, 134, 255, 177, 191, 206, 255,
    242, 242, 245, 255, 249, 248, 249, 255, 248, 248, 249, 255, 222, 222, 222, 255,
    181, 181, 181, 255, 255, 255, 255, 255, 223, 222, 225, 255, 195, 202, 209, 255,
    255, 255, 255, 255, 226, 230, 238, 255, 184, 194, 202, 255, 161, 173, 191, 255,
    118, 146, 168, 255, 175, 193, 201, 255, 200, 208, 214, 255, 255, 255, 255, 255,
    185, 196, 200, 255, 238, 242, 245, 255, 197, 202, 214, 255, 116, 132, 155, 255,
    222, 229, 234, 255, 225, 231, 235, 255, 189, 197, 208, 255, 255, 255, 253, 255,
    236, 242, 240, 255, 181, 190, 201, 255, 255, 255, 255, 255, 165, 178, 197, 255,
    116, 136, 163, 255, 219, 224, 232, 255, 176, 191, 204, 255, 244, 248, 245, 255,
    251, 250, 250, 255, 245, 244, 244, 255, 250, 251, 249, 255, 211, 211, 211, 255
    };

    GLFWimage img;
    img.width  = 32;
    img.height = 32;
    img.pixels = const_cast<unsigned char*>(pixels);
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
    const char* sel_label = sel == 0 ? "Fast" : sel == 1 ? "Thinking" : "Pro *";
    if (ImGui::BeginCombo("##model", sel_label)) {
        if (ImGui::Selectable("Fast",      sel == 0)) { sel = 0; g_gemini->setModel(GeminiModel::Flash);    g_config->set("model", "0"); }
        if (ImGui::Selectable("Thinking",  sel == 1)) { sel = 1; g_gemini->setModel(GeminiModel::Thinking); g_config->set("model", "1"); }
        if (ImGui::Selectable("Pro *",     sel == 2)) { sel = 2; g_gemini->setModel(GeminiModel::Pro);      g_config->set("model", "2"); }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("* Pro requires billing on your Gemini project\nFast/Thinking work on free tier");
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
