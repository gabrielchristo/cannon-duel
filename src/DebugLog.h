#pragma once
#include <raylib.h>
#include <cstdarg>
#include <string>
#include <vector>
#include "Platform.h"

#if CANNON_DUEL_DEBUG_MODE

namespace DebugLog {
    void Push(const std::string& line);
    const std::vector<std::string>& Lines();
    void InstallOverlayCapture();
}

void DebugLogf(int level, const char* fmt, ...);

#else

namespace DebugLog {
    inline void Push(const std::string&) {}
    inline const std::vector<std::string>& Lines() {
        static const std::vector<std::string> kEmpty;
        return kEmpty;
    }
    inline void InstallOverlayCapture() {}
}

inline void DebugLogf(int, const char*, ...) {}

#endif
