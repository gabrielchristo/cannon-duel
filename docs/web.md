# Build Web (WebAssembly)

Contexto sobre a porta Web do Cannon Duel — o que existe, como funciona e o
que fica pra depois. Veja também [`web/README.md`](../web/README.md) (passo
a passo de build) e [`ANDROID.md`](../ANDROID.md) (porta irmã, mesmo
espírito de "sem dependências pesadas de IDE").

## Por quê

Web é o único jeito de jogar Cannon Duel sem instalar nada e sem publicar
em loja — inclusive em iOS, onde não há build nativa planejada.

## Como builda

```bash
./build_web.sh release
./build_web.sh debug    # + dev panel / log overlay
```

`emcmake`/`emmake` (emsdk) configuram e compilam via CMake normal
(`CMakeLists.txt`, bloco `if(EMSCRIPTEN)`). Saída em
`web/dist/CannonDuel.{html,js,wasm,data}`, estático — roda em qualquer host
HTTP simples, incluindo GitHub Pages.

## O que é diferente do desktop/Android

| Área | Desktop/Android | Web |
|------|------------------|-----|
| Rede | libcurl (`src/net/SupabaseClient.cpp`, `RealtimeClient.cpp`) | `fetch()`/`emscripten_websocket` (`web/net/*.web.cpp`) |
| Concorrência de rede | thread de fundo real | `-sASYNCIFY` (sem pthreads — GitHub Pages não seta COOP/COEP) |
| Persistência (`player_identity.txt`) | filesystem nativo | WASMFS + backend OPFS, montado em `/opfs` (`web/WebOpfs.cpp/h`) |
| Assets | copiados pro diretório de build | empacotados em `.data` via `--preload-file assets@assets` |
| Áudio | miniaudio nativo | miniaudio via Web Audio (`ScriptProcessorNode`) — exige `-sEXPORTED_RUNTIME_METHODS=HEAPF32` |

A API pública de `SupabaseClient`/`RealtimeClient` é a mesma nas duas
versões, então nenhum outro arquivo em `src/` precisa saber que está
rodando na web (só alguns pontos pontuais atrás de
`CANNON_DUEL_WEB_BUILD`, ver `src/Platform.h`).

## Persistência: WASMFS + OPFS (migrado do IDBFS)

A build usa **WASMFS com backend OPFS** (`-sWASMFS`, `web/WebOpfs.cpp/h`)
pra sobreviver a reloads: `WebOpfsMount()` monta `/opfs` uma vez
(`wasmfs_create_opfs_backend()` + `wasmfs_create_directory`), e daí em
diante `std::ifstream`/`std::ofstream`/`SaveFileText` gravam direto nesse
caminho — sem `EM_ASM`/callback manual, e sem uma etapa explícita de "sync
out" (o write/close já persiste no OPFS).

Isso substituiu o **IDBFS** original (`-lidbfs.js`, `FS.syncfs` via
`EM_ASYNC_JS`), removido porque exigia essa dança manual de sync em volta
de toda leitura/escrita e está em rota de deprecação no Emscripten.

Pontos que fazem essa troca funcionar, e que **não podem regredir**:
- **Ordem de inicialização**: `WebOpfsMount()` precisa rodar *antes* de
  `emscripten_set_main_loop_arg` (`PlayerIdentity::LoadOrCreate()` em
  `src/GameCore.cpp`, chamado antes do loop principal arrancar). WASMFS
  resolve a criação do backend suspendendo a call stack C++ via Asyncify
  enquanto a Promise do OPFS resolve no event loop do browser — se o main
  loop já tiver tomado conta desse event loop, a suspensão nunca retoma e
  o app trava no carregamento sem erro nenhum.
- **`-sASYNCIFY` continua obrigatório**: é o que permite
  `wasmfs_create_opfs_backend()` rodar na thread principal sem pthreads
  (o próprio Emscripten faz `assert` nisso — ver
  `system/lib/wasmfs/backends/opfs_backend.cpp` no source do emsdk). Sem
  Asyncify (nem JSPI, nem pthreads), a criação do backend falha.
- Suporte a OPFS no browser: navegadores modernos (Chromium/Firefox/Safari
  recentes) suportam; não há fallback automático pra browsers antigos —
  se isso importar, precisa de teste de feature antes do mount.

O código em `src/` (`PlayerIdentity.cpp`) não sabe do mecanismo por trás —
só chama `WebOpfsMount()` e lê/escreve `/opfs/player_identity.txt` como
qualquer outro caminho.

## Limitações conhecidas

- Sem pthreads reais (ver tabela acima): uma requisição HTTP em andamento
  pode pausar a renderização por um frame ou dois. Aceitável pra um jogo
  por turnos.
- `fetch()`/WebSocket exigem `http://`/`https://`; abrir o `.html` como
  `file://` não funciona (rodar com `./run_web.sh release` ou `debug`).
- Canvas HiDPI: backing store = CSS × `devicePixelRatio` (teto 3×). O
  mundo lógico continua 1280×720; sem DPR o browser estica CSS px e borra.

## Pendências

- Validar OPFS em browser real (UUID entre reloads).
- Teste web ↔ desktop online.
