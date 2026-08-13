#include "WebIdbfs.h"

#if CANNON_DUEL_WEB_BUILD
#include <emscripten.h>

EM_ASYNC_JS(void, web_idbfs_mount_and_sync_in, (), {
    if (!Module.__idbfsMounted) {
        FS.mkdir('/idbfs');
        FS.mount(IDBFS, {}, '/idbfs');
        Module.__idbfsMounted = true;
    }
    await new Promise((resolve) => { FS.syncfs(true, () => resolve()); });
});

EM_ASYNC_JS(void, web_idbfs_sync_out, (), {
    await new Promise((resolve) => { FS.syncfs(false, () => resolve()); });
});

void WebIdbfsMountAndSyncIn() { web_idbfs_mount_and_sync_in(); }
void WebIdbfsSyncOut() { web_idbfs_sync_out(); }

#endif
