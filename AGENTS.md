# AGENTS.md — Cannon Duel

Guia para agentes de IA (Cursor, Copilot, etc.) trabalhando neste repositório.

## O que é este projeto

**Cannon Duel** é uma recriação moderna em C++17 do jogo *Canhão* (Mega Drive / TecToy, 2005). Artilharia por turnos com terreno destrutível, vento e — na versão Plus — power-ups.

- **Render/input/áudio:** raylib 5.5
- **Física:** Box2D v3.1.0 (apenas projéteis)
- **Multiplayer:** Supabase (PostgREST + Realtime WebSocket), sem login — UUID local

Documentação detalhada em [`docs/`](docs/).

## Build

### Linux (Ubuntu/Debian) — dependências via `apt`

O CMake baixa raylib, Box2D, nlohmann/json (e curl+mbedTLS no caso do
Android) automaticamente via `FetchContent` — mas algumas bibliotecas de
sistema (compilador, ferramentas de janela/áudio do Linux que o raylib
precisa, e a libcurl usada pelo multiplayer online) precisam estar
instaladas antes:

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake git pkg-config \
    libcurl4-openssl-dev \
    libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev \
    libxcursor-dev libxinerama-dev libxkbcommon-dev \
    libwayland-dev libasound2-dev
```

### Compilando (Linux/macOS)

Pré-requisitos: CMake >= 3.16, um compilador C++17 e conexão com a internet
na primeira configuração (o CMake baixa raylib, Box2D e nlohmann/json via
`FetchContent`).

```bash
./run_cmake.sh debug    # ou: ./run_cmake.sh release
./build_pc.sh
./run_pc.sh
```

Debug e release **compartilham `build/`**. No Linux (Make/Ninja), o tipo de
build fica fixo na configuração — para trocar debug ↔ release, rode
`./run_cmake.sh` de novo com o outro argumento.

Equivalente manual (debug):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCANNON_DUEL_DEBUG_MODE=ON \
  -DCMAKE_C_FLAGS="-Wno-error=maybe-uninitialized"
cmake --build build -j
./build/CannonDuel
```

Release: `-DCMAKE_BUILD_TYPE=Release -DCANNON_DUEL_DEBUG_MODE=OFF`

A flag `-DCMAKE_C_FLAGS="-Wno-error=maybe-uninitialized"` evita que um
falso-positivo do GCC dentro do próprio código do Box2D (não é bug seu)
quebre a build — ver detalhes na seção [Notas de build](#notas-de-build)
abaixo.

Windows (Visual Studio — mesma pasta `build/`, configs Debug/Release):

```bash
cmake -S . -B build
cmake --build build --config Debug
build\Debug\CannonDuel.exe

cmake --build build --config Release
build\Release\CannonDuel.exe
```

### Android

Veja [ANDROID.md](ANDROID.md) — inclui o passo a passo completo de como
instalar o Android SDK/NDK via linha de comando (sem precisar do Android
Studio) e como gerar o `.apk`. Resumo rápido, se você já sabe o que está
fazendo:

- **Android SDK**: `build-tools`, `platform-tools` e `platforms` de alguma
  API (ex: `android-30`) instalados via `sdkmanager`.
- **Android NDK**: instalado via `sdkmanager "ndk;<versão>"` (ex:
  `ndk;27.0.12077973`), apontado pela variável `ANDROID_NDK_HOME`.
- **JDK**: usado pelo `apksigner`/`keytool` na hora de assinar o APK.

```bash
export ANDROID_HOME=/caminho/pro/seu/sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/<versão instalada>
./run_cmake_android.sh debug   # ou: ./run_cmake_android.sh release
./build_android.sh
```

### Web (WebAssembly + Netlify)

Requer [emsdk](https://emscripten.org/) ativado. Build local; deploy manual no Netlify.

```bash
./build_web.sh release          # → web/dist/release/
./build_web.sh debug            # → web/dist/debug/ (dev panel F9)
```

### Notas de build

- **Erro do Box2D com `-Werror=maybe-uninitialized`**: algumas versões do
  GCC disparam um falso-positivo desse warning dentro do próprio código do
  Box2D (não é bug seu). Resolvido tanto pela flag `-DCMAKE_C_FLAGS=...`
  acima quanto por um `target_compile_options(box2d PRIVATE -Wno-error)`
  que já vem no `CMakeLists.txt` — a flag no comando é uma camada extra de
  segurança caso o seu toolchain específico ainda reclame.
- **`libcurl4-openssl-dev`** é necessário porque o multiplayer online
  (`src/net/`) usa `libcurl` pra falar com o Supabase — ver a seção
  [Multiplayer — checklist antes de testar](#multiplayer--checklist-antes-de-testar) abaixo.

## Como jogar

- Todo o jogo é jogável **apenas com o mouse**, no mecanismo clássico do jogo original:
  1. Uma linha oscila continuamente de 0° a 90° na direção do oponente — **clique** para travar o ângulo.
  2. Uma barra oscila de verde (fraco) a vermelho (forte) — **clique** de novo para travar a força e disparar.
- No menu principal, clique em "1 JOGADOR" (vs IA) ou "2 JOGADORES".

## Estrutura do código

```
src/
├── main.cpp              # entry point
├── Game.h                # classe central (declaração)
├── GameCore.cpp          # loop, ResetRound, menu dispatch
├── GameCombat.cpp        # mira, projétil, impacto, turnos
├── GameDraw.cpp          # renderização
├── GameScreens.cpp       # lobby, menus
├── GameOnline.cpp        # partida online, replay remoto
├── GameTeamRoom.cpp      # sala de composição
├── GameUI.cpp            # HUD, toggles Classic/Plus, nomes na partida
├── GameShop.cpp          # UI da loja
├── GameDebug.cpp         # painel dev (F9, desktop)
├── Config.h              # constantes de balanceamento (namespace cfg)
├── ShopCatalog.h         # IDs, preços, categorias da loja
├── AmmoVisuals.cpp/h     # tiro / rastro / impacto / preview da munição
├── CoinPopup.cpp/h       # "+N" flutuante ao ganhar moedas
├── GameTypes.h           # enums: GameMode, GameState, MatchComposition
├── MatchRoster.cpp/h     # até 10 canhões, ordem intercalada de turnos
├── Terrain.cpp/h         # heightmap 1D, crateras
├── Cannon.cpp/h          # canhão, vida, efeitos Plus, cosméticos
├── Projectile.cpp/h        # corpo Box2D dinâmico
├── PowerupSystem.cpp/h     # spawn, colisão, efeitos (Plus)
├── AI.cpp/h                # IA balística
└── net/                    # Supabase, Realtime, lobby, sync, PlayerWallet
```

## Arquitetura (visão rápida)

| Arquivo | Responsabilidade |
|---|---|
| `Config.h` | Constantes globais (escala física, tela, balanceamento) |
| `PhysicsWorld` | Wrapper fino sobre o mundo Box2D v3 |
| `Terrain` | Heightmap 1D, geração procedural (midpoint displacement), colisão por raycast 1D, destruição (crateras) e "chain shape" reconstruída no Box2D para os canhões |
| `Cannon` | Estado do canhão (posição, ângulo, força, vida) e desenho |
| `Projectile` | Corpo dinâmico do Box2D (bala/bullet com CCD) |
| `ParticleSystem` | Partículas de explosão (fogo + destroços) |
| `AI` | Estimativa balística (equação de alcance) + erro humano configurável |
| `Game` | Máquina de estados (menu → mira → voo → resolução → próximo turno) |

### Terreno destrutível

O terreno é um heightmap (uma altura por coluna de pixel). A colisão do
projétil usa esse heightmap diretamente (muito mais barato que regenerar um
polígono complexo do Box2D a cada impacto). Uma "chain shape" simplificada
(amostrada a cada poucos pixels) é reconstruída no Box2D só para dar suporte
físico aos canhões sobre o terreno.

Ao explodir, `Terrain::Explode` subtrai altura das colunas dentro do raio de
impacto (formato de meia-lua), simulando a cratera em tempo real.

### Vento

Sorteado a cada turno dentro de `[-WIND_MAX_FORCE, WIND_MAX_FORCE]` e aplicado
como força horizontal contínua no projétil (`b2Body_ApplyForceToCenter`) a
cada passo de física. A IA compensa a potência do tiro considerando o vento.

## Assets

Sprites, sons e música ficam em `assets/`, todos gerados/incluídos no
repositório — nada pendente pra rodar o jogo do zero. Os scripts em
`tools/` (`gen_sprites.py`, `gen_skins.py`, `gen_ammo.py`, `gen_sounds.py`,
`gen_icon.py`) regeneram esses assets caso você queira ajustar cores/formas/efeitos.

A música de fundo (`assets/sounds/music.ogg` e `music2.ogg`) é sorteada
aleatoriamente a cada partida.

## Convenções

- **Constantes de jogo:** sempre em `src/Config.h` (`namespace cfg`), não magic numbers espalhados.
- **Balanceamento Plus vs Classic:** `GameVersion::Classic` / `GameVersion::Plus`; power-ups só em Plus.
- **Composição online:** `MatchComposition { teamA, teamB }` até 5×5; slots mapeados para `player1_id`…`player10_id`.
- **Turnos online:** atirador autoritativo; oponentes aplicam **resultado** (`health_after`, cratera, dano) — não re-simulam física.
- **Canhão morto:** permanece visível, não atira; turnos pulam via `NextLivingPlayerNum` / `MaybeAdvancePastDeadOnlineTurn`.
- **Idioma:** strings em `Localization.h` (`TK` enum, PT-BR + EN).
- **Loja:** catálogo, IDs, preços e visual em [`docs/shop.md`](docs/shop.md). Não alterar um ID listado sem pedido explícito. Tudo é cosmético, exceto `ammo_nuclear` (cratera/área fixas em `Config.h`).
- **Cosméticos locais:** slots humanos usam a carteira do jogador (o jogador 2 pode repetir a mesma skin); slots de IA usam itens padrão. Nomes sempre visíveis no local (debug e release).
- **Moedas:** +10 power-up, +15 acerto em inimigo, +50 vitória, +5 derrota, 0 empate (`cfg::COINS_ROUND_*`). Popup de acerto ~3,1s; a tela de fim mostra o ganho.
- **Commits:** só quando o usuário pedir explicitamente.
- **Não commitar:** `SupabaseConfig.h` com credenciais reais, `player_identity.txt`.

## Onde mexer para tarefas comuns

| Tarefa | Arquivos principais |
|--------|---------------------|
| Balanceamento (dano, vento, cratera) | `Config.h`, `GameCombat.cpp` |
| Power-ups | `Powerup.h`, `PowerupSystem.cpp`, `GameCombat.cpp` |
| Sync online de turno/dano | `NetMatch.cpp`, `GameOnline.cpp`, `GameCombat.cpp` |
| Lobby / desafios / convites | `OnlineLobby.cpp`, `OnlineLobbyTeam.cpp`, `GameScreens.cpp` |
| Sala de equipes | `GameTeamRoom.cpp`, `OnlineLobbyTeam.cpp` |
| Schema DB | `supabase/schema.sql`, `supabase/migration_*.sql` |
| UI pós-partida / rematch | `GameOnline.cpp`, `GameDraw.cpp`, `GameCore.cpp` |
| Loja / catálogo / cosméticos | `ShopCatalog.h`, `GameShop.cpp`, `docs/shop.md`, `Localization.h` |
| Visual de munição | `AmmoVisuals.cpp`, `tools/gen_ammo.py` |
| Carteira / compra / equip | `net/PlayerWallet.cpp`, `supabase/migration_013_shop.sql`+ |
| Android | `android/`, `Platform.h`, `run_cmake_android.sh`, `build_android.sh` |

## Multiplayer online

Lobby público + partidas sincronizadas via Supabase (sem sistema de
login/conta — só um identificador leve gerado localmente). Veja
`src/net/` pro código e `supabase/schema.sql` pro schema do banco.

### Checklist antes de testar

1. Preencher `src/net/SupabaseConfig.h` com a URL e a chave do seu próprio
   projeto Supabase.
2. Aplicar `supabase/schema.sql` + migrations `001`–`016` no SQL Editor do
   painel Supabase (loja: 013–016; `equipped_ammo` é a 016).
3. Habilitar Realtime nas tabelas (migration 005 + 011).

Ver [`docs/networking.md`](docs/networking.md).

## Testes manuais recomendados

- **Local 1×1 Plus:** mira, tiro, cratera, power-up pickup, burial.
- **Online 2×1 / 2×2:** vida sincronizada em todos os clientes após cada tiro.
- **Canhão morto:** não recebe turno; partida continua até eliminar equipe.
- **Rematch:** botões ao fim da partida → sala ou lobby.
- **Convites:** aceitar convite de equipe não reexibe banner no lobby.
- **Loja local:** IA com itens padrão; jogador 2 humano pode repetir a skin; nomes visíveis.
- **Fim de partida:** mensagem mostra moedas da vitória (50) ou derrota (5).

## Documentação

| Arquivo | Conteúdo |
|---------|----------|
| [`docs/architecture.md`](docs/architecture.md) | Módulos, state machine, dependências |
| [`docs/networking.md`](docs/networking.md) | Supabase, Realtime, sync de turnos |
| [`docs/physics.md`](docs/physics.md) | Box2D, terreno, vento, projétil |
| [`docs/game-design.md`](docs/game-design.md) | Regras, modos, power-ups, composições |
| [`docs/shop.md`](docs/shop.md) | Catálogo da loja (IDs, preços, visual) |
| [`docs/web.md`](docs/web.md) | Build Web (WebAssembly), OPFS, limitações |

## Referência original

Jogo de artilharia por turnos: ângulo + força com oscilação (mouse), vento variável, terreno deformável. Multiplayer original era local; este projeto adiciona lobby online com salas de composição assimétricas (ex.: 2×1, 3×2, 5×5).
