# Architecture

## Visão geral

Cannon Duel é um jogo monolítico em C++17. Um único executável (`CannonDuel`) contém menu, gameplay local, lobby online e partida sincronizada. Não há servidor de jogo dedicado — o **Supabase** faz papel de backend (persistência + Realtime).

```
┌─────────────────────────────────────────────────────────┐
│                      Game (Game.h)                       │
│  state machine · roster · terrain · netMatch · lobby    │
├──────────────┬──────────────┬──────────────┬────────────┤
│ GameCore     │ GameCombat   │ GameDraw     │ GameOnline │
│ GameScreens  │ GameUI       │ GameTeamRoom │ GameDebug  │
├──────────────┴──────────────┴──────────────┴────────────┤
│ MatchRoster · Terrain · Cannon · Projectile · Powerups   │
│ ParticleSystem · ScreenEffects · AI · VirtualScreen      │
├─────────────────────────────────────────────────────────┤
│ PhysicsWorld (Box2D v3)                                  │
├─────────────────────────────────────────────────────────┤
│ net/: SupabaseClient · RealtimeClient · NetWorker        │
│       OnlineLobby · NetMatch · PlayerIdentity            │
├─────────────────────────────────────────────────────────┤
│ raylib 5.5 · nlohmann/json · libcurl (HTTP + WebSocket)  │
└─────────────────────────────────────────────────────────┘
```

## Entry point

`src/main.cpp` instancia `Game` e chama `Game::Run()` — loop clássico `Update(dt)` + `Draw()` a 60 FPS (`cfg::TARGET_FPS`).

## Classe `Game`

Centraliza todo o estado. Declaração em `Game.h`; implementação dividida por domínio:

| TU | Responsabilidade |
|----|------------------|
| `GameCore.cpp` | `Run`, `Update` (dispatch por `GameState`), `ResetRound`, `StartMatch`, vento |
| `GameCombat.cpp` | Mira, voo do projétil, `ResolveImpact`, fim de turno, `CheckRoundEnd` |
| `GameDraw.cpp` | Terreno, canhões, HUD, overlay de fim de partida |
| `GameScreens.cpp` | Menu, lobby online, banners de desafio/convite |
| `GameOnline.cpp` | `StartOnlineMatch`, replay/remoto, espectador, rematch |
| `GameTeamRoom.cpp` | UI da sala de composição |
| `GameUI.cpp` | Toggle Classic/Plus, bandeiras de idioma, diálogos |
| `GameDebug.cpp` | Painel dev (desktop, `CANNON_DUEL_DEBUG_MODE`) |

## Máquina de estados (`GameState`)

```
MainMenu
  ├─► FormatSelect ──► Aiming ◄──► TurnTransition
  │                      │
  │                      ├─► ProjectileFlying
  │                      ├─► RemoteShotReplay      (online)
  │                      └─► RemoteProjectileLive  (online)
  │
  ├─► OnlineLobby ──► OnlineTeamRoom ──► (partida online)
  │
  ├─► About / Instructions
  └─► RoundOver
```

Estados online adicionais durante partida:
- **RemoteProjectileLive** — stream de amostras do projétil via Realtime broadcast.
- **RemoteShotReplay** — fallback com arco interpolado quando não há stream.

## Modos e versões

| Enum | Valores | Notas |
|------|---------|-------|
| `GameMode` | `PvAI`, `PvP`, `Online` | |
| `GameVersion` | `Classic`, `Plus` | Plus = power-ups, shake, friendly fire em equipe |
| `MatchFormat` | `Duel1v1`, `Team2v2`, `Team3v3` | Local; online usa `MatchComposition` dinâmica |
| `MatchComposition` | `{ teamA, teamB }` 1–5 cada | Total até 10 jogadores |

## MatchRoster

Gerencia até **10 canhões** (`kMaxCannons = 10`, `kMaxPerTeam = 5`):

- Posicionamento por equipe (`PlaceCannons`) — margem lateral, espaçamento `TEAM_CANNON_PAIR_SPACING_PX`.
- Ordem de turnos **intercalada**: A0 → B0 → A1 → B1 → …
- `NextLivingPlayerNum` — pula canhões mortos (`health <= 0.5`).
- `IsTeamEliminated` / `IsTeamBuried` — condições de vitória por equipe.

## Terreno (`Terrain`)

Heightmap 1D com **1280 colunas** (1 px por coluna). Geração por midpoint displacement; o cenário Vale do Fim aplica um envelope de vale no miolo. Colisão do projétil é **manual** contra o heightmap (`IsPointInside`), não via shape Box2D.

`Terrain::RebuildPhysicsBody` é intencionalmente no-op — canhões não usam corpo físico de terreno.

## Renderização

- **VirtualScreen** — mundo lógico fixo 1280×720 (`SCREEN_*`); a janela desktop pode ser 1920×1080 (`WINDOW_*`). Física e acertos não mudam com a resolução da janela.
- **ScreenEffects** — screen shake (Plus) aplicado via `Camera2D` offset.
- Sprites em `assets/sprites/`; gerados por `tools/gen_sprites.py`.

## Rede (`src/net/`)

Camadas:

1. **SupabaseClient** — REST PostgREST (INSERT/SELECT/UPDATE/DELETE).
2. **RealtimeClient** — WebSocket Phoenix (`curl_ws_*`), postgres_changes + broadcast.
3. **NetWorker** — fila HTTP assíncrona com coalescing (heartbeats, live aim PATCH).
4. **OnlineLobby** — presença, desafios, salas de equipe, match start.
5. **NetMatch** — sync in-match: turnos, mira ao vivo, stream de projétil.

Ver [`networking.md`](networking.md).

## Build e dependências

CMake 3.16+, FetchContent para:
- raylib 5.5
- Box2D v3.1.0
- nlohmann/json 3.11.3
- libcurl 8.9.1 (+ mbedTLS no Android)

Flags de plataforma em `Platform.h`:
- `CANNON_DUEL_ANDROID_BUILD`
- `CANNON_DUEL_DEBUG_MODE` (painel F9)

## Banco de dados

Schema base + 11 migrations em `supabase/`. Tabelas principais: `players`, `lobby_presence`, `challenges`, `team_rooms`, `team_room_members`, `team_invites`, `matches`, `match_turns`.

## Assets e ferramentas

| Script | Saída |
|--------|-------|
| `tools/gen_sprites.py` | `assets/sprites/` |
| `tools/gen_sounds.py` | `assets/sounds/*.ogg` |
| `tools/gen_icon.py` | ícones Android |

Assets commitados no repo; scripts para regenerar.

## Princípios de design de código

1. **Autoridade no atirador (online)** — física roda no cliente do atirador; demais aplicam resultado.
2. **Determinismo parcial online** — vento, cenário e spawn de power-ups seeded por `terrain_seed`.
3. **Sem over-engineering** — helpers só quando reutilizados; diffs mínimos.
4. **Config centralizado** — `Config.h` para tuning.
