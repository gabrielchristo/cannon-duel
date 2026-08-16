#include "WebOpfs.h"

#if CANNON_DUEL_WEB_BUILD
#include <emscripten/wasmfs.h>

namespace {
bool g_mounted = false;
}

void WebOpfsMount() {
    if (g_mounted) return;
    backend_t opfs = wasmfs_create_opfs_backend();
    wasmfs_create_directory("/opfs", 0777, opfs);
    g_mounted = true;
}

#endif
