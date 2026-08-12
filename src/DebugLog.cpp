#include "DebugLog.h"
#include <raylib.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace {
std::vector<std::string> g_lines;
constexpr size_t kMaxLines = 800;

const char* LevelPrefix(int level) {
    switch (level) {
        case LOG_TRACE: return "TRACE";
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default: return "LOG";
    }
}

void OverlayTraceLogCallback(int level, const char* text, va_list args) {
    char body[380];
    vsnprintf(body, sizeof(body), text, args);
    body[sizeof(body) - 1] = '\0';

    char line[420];
    snprintf(line, sizeof(line), "[%s] %s", LevelPrefix(level), body);
    DebugLog::Push(line);
}
} // namespace

namespace DebugLog {
void Push(const std::string& lineIn) {
    std::string line = lineIn;
    if (line.size() > 200) line = line.substr(0, 197) + "...";
    g_lines.push_back(line);
    if (g_lines.size() > kMaxLines) {
        g_lines.erase(g_lines.begin());
    }
}

const std::vector<std::string>& Lines() {
    return g_lines;
}

void InstallOverlayCapture() {
    SetTraceLogLevel(LOG_TRACE);
    SetTraceLogCallback(OverlayTraceLogCallback);
}
} // namespace DebugLog

void DebugLogf(int level, const char* fmt, ...) {
    char buf[400];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // TraceLog repassa pro callback acima, que grava só no overlay.
    TraceLog(level, "%s", buf);
}
