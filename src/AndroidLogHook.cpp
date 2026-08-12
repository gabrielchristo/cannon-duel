#include "Platform.h"

#if CANNON_DUEL_DEBUG_MODE && CANNON_DUEL_ANDROID_BUILD

#include "DebugLog.h"
#include <android/log.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace {

void PushFormatted(const char* prefix, const char* fmt, va_list args) {
    char body[380];
    vsnprintf(body, sizeof(body), fmt, args);
    body[sizeof(body) - 1] = '\0';

    char line[420];
    if (prefix && prefix[0]) {
        snprintf(line, sizeof(line), "%s %s", prefix, body);
    } else {
        snprintf(line, sizeof(line), "%s", body);
    }
    DebugLog::Push(line);
}

} // namespace

extern "C" int __wrap___android_log_print(int prio, const char* tag, const char* fmt, ...) {
    (void)prio;
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "[%s]", tag ? tag : "?");

    va_list args;
    va_start(args, fmt);
    PushFormatted(prefix, fmt, args);
    va_end(args);
    return static_cast<int>(strlen(prefix));
}

extern "C" int __wrap___android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
    (void)prio;
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "[%s]", tag ? tag : "?");
    PushFormatted(prefix, fmt, ap);
    return static_cast<int>(strlen(prefix));
}

extern "C" int __wrap_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    PushFormatted(nullptr, fmt, args);
    va_end(args);
    return 0;
}

extern "C" int __wrap_vprintf(const char* fmt, va_list ap) {
    PushFormatted(nullptr, fmt, ap);
    return 0;
}

extern "C" int __wrap_fprintf(FILE* stream, const char* fmt, ...) {
    (void)stream;
    va_list args;
    va_start(args, fmt);
    PushFormatted(nullptr, fmt, args);
    va_end(args);
    return 0;
}

#endif // CANNON_DUEL_DEBUG_MODE && CANNON_DUEL_ANDROID_BUILD
