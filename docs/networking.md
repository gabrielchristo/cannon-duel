# Networking

## Stack

| Componente | Tecnologia | Uso |
|------------|------------|-----|
| REST API | Supabase PostgREST via `SupabaseClient` | CRUD de partidas, turnos, lobby |
| Realtime | Phoenix WebSocket via `RealtimeClient` | CDC + broadcast in-match |
| HTTP async | `NetWorker` | Fila de requests, coalescing |
| Identidade | `PlayerIdentity` | UUID local em `player_identity.txt` |

**Sem login.** O cliente gera um UUID, registra em `players` na primeira entrada no lobby.

Configuração: `src/net/SupabaseConfig.h` (URL + anon key — não commitar credenciais).

## Fluxo do lobby

```
1. EnterLobby()
   └─ upsert lobby_presence (status: idle)
   └─ Realtime topic: lobby:<player_uuid>

2. SendChallenge(target, version)
   └─ INSERT challenges (status: pending, format: composition)

3. AcceptChallenge()
   └─ INSERT team_rooms (status: recruiting)
   └─ INSERT team_room_members (capitães, slot 0)
   └─ ambos entram em OnlineTeamRoom

4. Sala de equipes
   └─ capitão convida: INSERT team_invites
   └─ parceiro aceita: INSERT team_room_members + UPDATE invite accepted
   └─ capitão StartTeamMatch(): INSERT matches + UPDATE team_rooms started

5. TryResolveTeamRoomMatchStart() → PollMatchStart() → StartOnlineMatch()
```

**Toda partida online passa pela sala de equipes**, inclusive 1×1 (dois capitães, composição 1×1).

### Convites e desafios

- `RefreshIncomingTeamInvites()` filtra: status `pending`, jogador **não** membro da sala, sala `recruiting`.
- Convites aceitos/recusados são removidos da UI local imediatamente.
- Desafios aceitos atualizam `challenges.status = accepted`.

## Fluxo in-match (`NetMatch`)

### Princípio

O **atirador** simula fisicamente o tiro. Os **demais clientes** recebem o **resultado autoritativo** (posição de impacto, cratera, vida após tiro, próximo turno) — não re-simulam o projétil para resolver dano.

### Ciclo de um turno

```
Atirador                              Oponentes
   │                                      │
   ├─ PublishLiveAim (broadcast ~12Hz) ──►│ overlay de mira
   ├─ PublishShotFired + proj samples ───►│ RemoteProjectileLive
   ├─ ResolveImpact (local)               │
   ├─ SubmitMyTurn ──────────────────────►│ INSERT match_turns
   │   + broadcast turn_result            │   (health_after, damages, …)
   │                                      ├─ FinishRemoteTurn
   │                                      │   · terrain.Explode
   │                                      │   · SyncCannonHealthFromTurn
   └─ awaitingOpponentTurn = true         └─ próximo turno
```

### `SubmitMyTurn` payload

Gravado em `match_turns` e broadcast `turn_result`:

| Campo | Descrição |
|-------|-----------|
| `turn_number`, `shooter_player` | Ordem e atirador |
| `shoot_angle`, `shoot_power`, `wind_at_shot` | Mira e vento |
| `impact_x/y`, `crater_radius` | Impacto |
| `damage_p1`…`damage_p10` | Dano por slot |
| `health_after` (broadcast) | Vida absoluta por slot (autoritativo) |
| `next_wind`, `next_turn_player` | Próximo turno |
| `match_over`, `winner_player` | Fim de partida |
| `picked_powerup_type/x` | Power-up coletado no tiro (Plus) |

### Sync de vida

1. **Primário:** array `health_after` no broadcast `turn_result`.
2. **Fallback:** deltas `damage_pN` do INSERT em `match_turns`.
3. **Race fix:** `PollHealthResync` se broadcast chega após turno já consumido.
4. **`healthAfterCount`** — só aplica índices presentes no payload (evita zerar slots ausentes).

### Stream de projétil

- `shot_fired` — ângulo, força, vento, muzzle, shot_id.
- `proj` — amostras `{ seq, t, x, y }` ~25 Hz.
- `shot_end` — posição final de impacto.

Fallback: poll HTTP de `match_turns` a cada 1.2s (sem Realtime) ou 5s (com Realtime).

### Presença e disconnect

- Heartbeat in-match: `pN_last_seen` em `matches` a cada ~2.5s.
- Stale threshold: 8s (`PARTICIPANT_STALE_SEC`).
- Grace period no início: 20s.
- Abandono: `matches.status = abandoned`, `WinnerWhenPlayerLeaves()`.

### Canhão morto online

- `NextLivingPlayerNum` calcula próximo turno pulando mortos.
- `MaybeAdvancePastDeadOnlineTurn` — corrige turno preso em jogador morto via `SyncTurnTo`.
- Mortos não atiram (`IsLocalHumanTurn` verifica `IsPlayerAlive`).

## RealtimeClient

- Protocolo Phoenix sobre WebSocket (`libcurl`).
- **postgres_changes:** INSERT/UPDATE em tabelas subscritas.
- **broadcast:** eventos custom (`aim`, `proj`, `turn_result`, `powerup_picked`, …).
- Topics: `lobby:<uuid>`, `match:<match_uuid>`, `team:<uuid>`.

Tabelas com `REPLICA IDENTITY FULL` + publicação `supabase_realtime` (migration 005, 011).

## Espectador

- `myPlayerNumber = 0` em `NetMatch`.
- Carrega todos os `match_turns` e reaplica via `ApplyTurnSilently`.
- Realtime passivo para novos turnos e fim de partida.
- Botão "Assistir" no lobby (`lobby_presence.match_id`).

## Pós-partida

- `SnapshotRematchRoom()` salva `lastTeamRoomId_` ao iniciar partida.
- Fim de partida: **Jogar novamente** → `EnterTeamRoomForRematch()` (reset sala para `recruiting`).
- **Voltar ao lobby** → `ReturnToLobbyAfterMatch()` (presença idle, sem `LeaveLobby` completo).

## Schema e migrations

Aplicar em ordem:

1. `supabase/schema.sql`
2. `migration_001` … `migration_011`

Migration 011 adiciona composição dinâmica:
- `team_room_members`, `player5_id`…`player10_id`
- `team_a_count`, `team_b_count`
- `damage_p5`…`damage_p10`

## Tabelas principais

| Tabela | Papel |
|--------|-------|
| `players` | id, display_name, wins, losses |
| `lobby_presence` | quem está online, status, match_id |
| `challenges` | desafios 1×1 → sala |
| `team_rooms` | sala de composição, status, match_id |
| `team_room_members` | jogadores por equipe/slot |
| `team_invites` | convites pendentes |
| `matches` | partida ativa, seed, vento, turno atual, player1–10 |
| `match_turns` | histórico de turnos com dano e mira |

RLS: políticas permissivas (projeto de hobby); revisar antes de produção pública.
