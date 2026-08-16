# Cannon Duel — build Web (WebAssembly)

Tudo relacionado à build de browser vive nesta pasta. O jogo em si (`src/`)
não sabe que está rodando na web além de alguns pontos pontuais guardados
por `CANNON_DUEL_WEB_BUILD` (ver `src/Platform.h`).

## Pré-requisitos

- [emsdk](https://emscripten.org/docs/getting_started/downloads.html) instalado e ativado:
  ```bash
  git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
  cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
  source ~/emsdk/emsdk_env.sh
  ```
  (`build_web.sh` tenta encontrar `$EMSDK`, `~/emsdk` ou `$HOME/emsdk` se
  `emcmake` não estiver no PATH da sessão atual — mas precisa ter sido
  instalado/ativado uma vez.)

## Build

```bash
./build_web.sh release
./build_web.sh debug    # dev panel + log overlay (CANNON_DUEL_DEBUG_MODE)
```

Gera `web/dist/release/` ou `web/dist/debug/`.

CMake compila sempre em **`web/build/`** (debug e release compartilham — ao
trocar o modo o script reconfigura; no Linux isso é obrigatório, ver
`run_cmake.sh`).

## Rodar localmente

`fetch()`/WebSocket exigem `http://` ou `https://` — abrir o `.html` direto
como `file://` não funciona.

```bash
./run_web.sh release
./run_web.sh debug
# abra http://localhost:8080/CannonDuel.html
```

### Orientação e tela (web)

- Escala **contain**: jogo inteiro visível (sem recortar bordas).
- Barras laterais/superiores usam a cor do céu (`#ebd6be`), não preto.
- **Mobile portrait:** overlay pede girar; canvas fica oculto até landscape.
- Rebuild após mudar `web/shell.html`: `./build_web.sh release` (ou `debug`).

## O que é diferente da build desktop/Android

- **Rede**: `src/net/SupabaseClient.cpp` e `RealtimeClient.cpp` (libcurl)
  são trocados por `web/net/SupabaseClient.web.cpp` (fetch, via
  `EM_ASYNC_JS` + `-sASYNCIFY`) e `web/net/RealtimeClient.web.cpp`
  (`emscripten_websocket`). Mesma API pública — nenhum outro arquivo em
  `src/` precisou mudar por causa disso.
- **Identidade do jogador**: `player_identity.txt` é persistido em
  `/opfs` via WASMFS + backend OPFS (`web/WebOpfs.h/.cpp`), porque o
  filesystem em memória do WASM não sobrevive a um reload da página.
  `std::ifstream`/`std::ofstream` funcionam direto contra esse caminho
  depois do mount (sem sync manual como o IDBFS antigo exigia) — ver
  `docs/web.md`.
- **Assets**: empacotados no `.data` via `--preload-file assets@assets` —
  `AssetPath.h` não precisou mudar (mesmo prefixo `"assets/"` do desktop).
- **Sem pthreads**: a build web não usa threads reais (Asyncify + fila HTTP
  no loop principal). Isso significa que uma requisição HTTP em andamento pode
  pausar a renderização por um frame ou dois — aceitável pra um jogo por
  turnos.
