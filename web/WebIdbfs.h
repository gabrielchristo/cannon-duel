#pragma once
#include "Platform.h"

// Persistência do player_identity.txt no browser: o MEMFS do Emscripten é
// só em memória (some a cada reload), então espelhamos "/idbfs" numa
// IndexedDB via IDBFS. Usa EM_ASYNC_JS (Asyncify) pra expor isso como
// chamadas de aparência síncrona pro C++ — ver web/WebIdbfs.cpp.
#if CANNON_DUEL_WEB_BUILD

// Monta IDBFS em "/idbfs" (uma vez) e puxa o conteúdo salvo do browser.
// Chame antes de qualquer leitura de PlayerIdentity::SavePath().
void WebIdbfsMountAndSyncIn();

// Persiste "/idbfs" de volta pra IndexedDB. Chame depois de qualquer escrita.
void WebIdbfsSyncOut();

#endif
