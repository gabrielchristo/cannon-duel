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
  (`build_web.sh` tenta encontrar `~/emsdk` sozinho se `emcmake` não estiver
  no PATH da sessão atual — mas precisa ter sido instalado/ativado uma vez.)

## Build

```bash
./web/build_web.sh          # release
./web/build_web.sh debug    # com dev panel + log overlay (CANNON_DUEL_DEBUG_MODE)
```

Gera `web/dist/CannonDuel.{html,js,wasm,data}`.

## Rodar localmente

`fetch()`/WebSocket exigem `http://` ou `https://` — abrir o `.html` direto
como `file://` não funciona.

```bash
cd web/dist && python3 -m http.server 8080
# abra http://localhost:8080/CannonDuel.html
```

## Publicar (GitHub Pages)

Não é automatizado por este script de propósito (é uma ação que publica
pra fora do repo). Passos manuais:

1. Rode `./web/build_web.sh`.
2. Copie o conteúdo de `web/dist/` pra pasta que o GitHub Pages serve
   (ex.: `docs/` na branch publicada, ou uma branch `gh-pages`).
3. Configure o Pages do repositório pra apontar pra essa pasta/branch.

## O que é diferente da build desktop/Android

- **Rede**: `src/net/SupabaseClient.cpp` e `RealtimeClient.cpp` (libcurl)
  são trocados por `web/net/SupabaseClient.web.cpp` (fetch, via
  `EM_ASYNC_JS` + `-sASYNCIFY`) e `web/net/RealtimeClient.web.cpp`
  (`emscripten_websocket`). Mesma API pública — nenhum outro arquivo em
  `src/` precisou mudar por causa disso.
- **Identidade do jogador**: `player_identity.txt` é espelhado numa
  IndexedDB via IDBFS (`web/WebIdbfs.h/.cpp`), porque o filesystem em
  memória do WASM não sobrevive a um reload da página.
- **Assets**: empacotados no `.data` via `--preload-file assets@assets` —
  `AssetPath.h` não precisou mudar (mesmo prefixo `"assets/"` do desktop).
- **Sem pthreads**: GitHub Pages não seta os headers COOP/COEP que
  `SharedArrayBuffer`/pthreads exigem, então a build não usa threads reais.
  Isso significa que uma requisição HTTP em andamento pode pausar a
  renderização por um frame ou dois (o tempo do round-trip) — aceitável
  pra um jogo por turnos, mas é a principal diferença de comportamento em
  relação ao desktop (lá a rede roda numa thread de fundo de verdade).
