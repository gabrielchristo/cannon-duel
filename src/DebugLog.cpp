#include "DebugLog.h"
#include <raylib.h>
#include <cstdio>
#include <cstdarg>

namespace {
std::vector<std::string> g_lines;
constexpr size_t kMaxLines = 800;
} // namespace

namespace DebugLog {
void Push(const std::string& lineIn) {
    std::string line = lineIn;
    if (line.size() > 90) line = line.substr(0, 87) + "...";
    g_lines.push_back(line);
    if (g_lines.size() > kMaxLines) {
        g_lines.erase(g_lines.begin());
    }
}

const std::vector<std::string>& Lines() {
    return g_lines;
}
} // namespace DebugLog

void DebugLogf(int level, const char* fmt, ...) {
    char buf[400];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // ainda tenta o TraceLog padrão também (stdout/logcat, quando
    // disponível) — mas o overlay não depende dele funcionar.
    TraceLog(level, "%s", buf);
    DebugLog::Push(buf);
}
