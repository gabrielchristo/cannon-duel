# AGENTS.md — Cannon Duel

Guia para agentes de IA (Cursor, Copilot, etc.) trabalhando neste repositório.

## O que é este projeto

**Cannon Duel** é uma recriação moderna em C++17 do jogo *Canhão* (Mega Drive / TecToy, 2005). Artilharia por turnos com terreno destrutível, vento e — na versão Plus — power-ups.

- **Render/input/áudio:** raylib 5.5
- **Física:** Box2D v3.1.0 (apenas projéteis)
- **Multiplayer:** Supabase (PostgREST + Realtime WebSocket), sem login — UUID local

Documentação detalhada em [`docs/`](docs/).

## Build rápido

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/CannonDuel
```

Android: ver [`ANDROID.md`](ANDROID.md) e `./build_android.sh`.

Dependências Linux: `build-essential`, `cmake`, `libcurl4-openssl-dev`, libs X11/Wayland/ALSA (ver [`README.md`](README.md)).

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
├── GameUI.cpp            # HUD, toggles Classic/Plus
├── GameDebug.cpp         # painel dev (F9, desktop)
├── Config.h              # constantes de balanceamento (namespace cfg)
├── GameTypes.h           # enums: GameMode, GameState, MatchComposition
├── MatchRoster.cpp/h     # até 10 canhões, ordem intercalada de turnos
├── Terrain.cpp/h         # heightmap 1D, crateras
├── Cannon.cpp/h          # canhão, vida, efeitos Plus
├── Projectile.cpp/h        # corpo Box2D dinâmico
├── PowerupSystem.cpp/h     # spawn, colisão, efeitos (Plus)
├── AI.cpp/h                # IA balística
└── net/                    # Supabase, Realtime, lobby, sync de partida
```

## Convenções

- **Constantes de jogo:** sempre em `src/Config.h` (`namespace cfg`), não magic numbers espalhados.
- **Balanceamento Plus vs Classic:** `GameVersion::Classic` / `GameVersion::Plus`; power-ups só em Plus.
- **Composição online:** `MatchComposition { teamA, teamB }` até 5×5; slots mapeados para `player1_id`…`player10_id`.
- **Turnos online:** atirador autoritativo; oponentes aplicam **resultado** (`health_after`, cratera, dano) — não re-simulam física.
- **Canhão morto:** permanece visível, não atira; turnos pulam via `NextLivingPlayerNum` / `MaybeAdvancePastDeadOnlineTurn`.
- **Idioma:** strings em `Localization.h` (`TK` enum, PT-BR + EN).
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
| Android | `android/`, `Platform.h`, `build_android.sh` |

## Multiplayer — checklist antes de testar

1. Preencher `src/net/SupabaseConfig.h` (URL + anon key).
2. Aplicar `supabase/schema.sql` + migrations `001`–`011` no Supabase.
3. Habilitar Realtime nas tabelas (migration 005 + 011).

Ver [`docs/networking.md`](docs/networking.md).

## Testes manuais recomendados

- **Local 1×1 Plus:** mira, tiro, cratera, power-up pickup, burial.
- **Online 2×1 / 2×2:** vida sincronizada em todos os clientes após cada tiro.
- **Canhão morto:** não recebe turno; partida continua até eliminar equipe.
- **Rematch:** botões ao fim da partida → sala ou lobby.
- **Convites:** aceitar convite de equipe não reexibe banner no lobby.

## Documentação

| Arquivo | Conteúdo |
|---------|----------|
| [`docs/architecture.md`](docs/architecture.md) | Módulos, state machine, dependências |
| [`docs/networking.md`](docs/networking.md) | Supabase, Realtime, sync de turnos |
| [`docs/physics.md`](docs/physics.md) | Box2D, terreno, vento, projétil |
| [`docs/game-design.md`](docs/game-design.md) | Regras, modos, power-ups, composições |
| [`docs/roadmap.md`](docs/roadmap.md) | Features planejadas |
| [`docs/current-state.md`](docs/current-state.md) | Estado atual da branch e trabalho recente |

## Referência original

Jogo de artilharia por turnos: ângulo + força com oscilação (mouse), vento variável, terreno deformável. Multiplayer original era local; este projeto adiciona lobby online com salas de composição assimétricas (ex.: 2×1, 3×2, 5×5).
