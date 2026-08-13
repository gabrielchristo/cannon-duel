#pragma once
#include "Platform.h"

// Persistência do player_identity.txt no browser: o MEMFS do Emscripten é
// só em memória (some a cada reload), então usamos o backend OPFS do
// WASMFS (Origin Private File System) montado em "/opfs". Diferente do
// IDBFS antigo, não precisa de sync manual — std::ifstream/std::ofstream
// gravam direto no OPFS a cada write/close, via Asyncify (ver
// wasmfs_create_opfs_backend em WebOpfs.cpp).
#if CANNON_DUEL_WEB_BUILD

// Monta o backend OPFS em "/opfs" (uma vez — chamadas seguintes são
// no-op). Precisa rodar antes de emscripten_set_main_loop_arg (ver
// Game::Run em GameCore.cpp): a criação do backend suspende a call stack
// via Asyncify enquanto a Promise do OPFS resolve no event loop do
// browser, o que só acontece enquanto o loop principal ainda não tomou
// conta desse event loop — chamar depois trava sem erro nenhum.
void WebOpfsMount();

#endif
