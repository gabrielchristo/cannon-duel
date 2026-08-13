# Game Design

## Conceito

Artilharia por turnos inspirada no *Canhão* (Mega Drive, 2005). Dois lados se alternam atirando projéteis em arco parabólico sobre terreno deformável, com vento variável afetando a trajetória.

Objetivo: **eliminar todos os canhões inimigos** (vida zerada ou enterro completo).

## Controles

100% mouse, mecanismo clássico:

1. **Fase ângulo** — linha oscila de 0° a 90° na direção do oponente; clique trava.
2. **Fase força** — barra oscila verde (fraco) → vermelho (forte); clique dispara.

Períodos de oscilação: `ANGLE_OSC_PERIOD_SEC = 2.4s`, `POWER_OSC_PERIOD_SEC = 1.1s`.

## Modos de jogo

| Modo | Descrição |
|------|-----------|
| **1 Jogador (PvAI)** | Humano vs IA com erro configurável |
| **2 Jogadores (PvP)** | Local, mesmo teclado/mouse |
| **Online** | Lobby Supabase, salas de composição, sync por turno |

## Versões

| Versão | Diferenças |
|--------|------------|
| **Classic** | Sem power-ups; friendly fire desligado |
| **Plus** | Power-ups, screen shake, friendly fire em partidas de equipe |

Toggle Classic/Plus disponível no lobby online e na seleção de formato local.

## Composições

### Local

Formatos fixos via `MatchFormat`:
- **Duel 1×1**
- **Team 2×2**
- **Team 3×3**

### Online

Composição **assimétrica** via `MatchComposition`:
- Equipe A: 1–5 canhões
- Equipe B: 1–5 canhões
- Total máximo: **10 jogadores**

Exemplos: 1×1, 2×1, 3×2, 5×5.

Formato persistido no DB como `team_AxB` (ex.: `team_2x1`) ou `duel_1v1`.

### Posicionamento

- Canhões da equipe A à esquerda, B à direita.
- Parceiros de equipe lado a lado (`TEAM_CANNON_PAIR_SPACING_PX = 110px`).
- Margem mínima da borda: `CANNON_MARGIN_PX = 70px`.

## Turnos

Ordem **intercalada** entre equipes:

```
A0 → B0 → A1 → B1 → A2 → B2 → …
```

Canhões mortos (`health <= 0`) são **pulados** — permanecem visíveis na tela mas não atiram.

Vento sorteado (local) ou seeded (online) a cada turno.

## Vida e dano

| Parâmetro | Valor |
|-----------|-------|
| Vida máxima | 180 HP |
| Dano máximo (impacto direto) | 60 HP |
| Acertos para destruir | 3 (diretos) |
| Raio de dano em área | 60 px |
| Raio de cratera | 42 px (local); escalado online |

Dano com falloff linear pela distância ao centro da explosão.

## Condições de vitória

| Condição | Resultado |
|----------|-----------|
| Todos os canhões inimigos com vida ≤ 0 | Vitória por eliminação |
| Todos os canhões inimigos vivos enterrados | Vitória por enterro |
| Ambos enterrados simultaneamente | Empate por enterro |
| Oponente abandona (online) | Vitória por abandono |

Em partidas de equipe, vitória é por **equipe** (player 1 = time A, player 2 = time B no resultado online).

## Terreno destrutível

- Crateras semicirculares ao impacto.
- Coluna totalmente destruída → canhão sobre ela morre (enterro).
- Terreno regenerado a cada rodada com seed (online: `terrain_seed` compartilhado).

## Power-ups (Plus)

Spawn a cada **3 tiros de jogador** (`POWERUP_SPAWN_EVERY_TURNS = 3`), não por rodada completa de time.

Máximo 6 ativos simultaneamente. Spawn evita posição sobre canhões (`POWERUP_SPAWN_CANNON_CLEARANCE_PX`).

Pickup: projétil deve passar sobre o item (colisão no impacto ou durante voo).

| Tipo | Sigla | Efeito |
|------|-------|--------|
| **DoubleDamage** | 2X | Próximo **tiro** ×2 dano (consumido mesmo se errar) |
| **TrajectoryPreview** | TR | Trajetória prevista por 1 turno; mira mais lenta |
| **Guided** | GD | Próximo tiro teleguiado (sempre acerta, dano ×0.7) — mais raro |
| **Heal** | + | Cura 25–50% da vida máxima |
| **Shield** | SH | Imune a dano por 1 turno deste canhão (protege no próximo turno dele, inclusive enquanto atira) |

Pesos de spawn: Guided = 0.4, demais = 1.0.

## IA

- Estimativa balística com equação de alcance.
- Compensação de vento na potência.
- Erro humano configurável (imprecisão de ângulo/força).

## Online — fluxo do jogador

1. Menu → **Online** → lobby (lista de jogadores, toggle Classic/Plus).
2. Desafiar jogador ou aceitar desafio → **sala de equipes**.
3. Capitão convida parceiros (composição assimétrica).
4. Capitão inicia partida quando composição pronta.
5. Turnos alternados; mira do oponente visível em tempo real.
6. Fim de partida: **Jogar novamente** (mesma sala) ou **Voltar ao lobby**.

Espectadores podem assistir partidas em andamento pelo lobby.

## Localização

PT-BR e EN via `Localization.h` (`Lang` enum, chaves `TK::`).

Bandeiras no canto da tela para alternar idioma.

## Identidade do jogador

- Nome de exibição editável no lobby.
- UUID anônimo local (`player_identity.txt`).
- Wins/losses persistidos em `players` (Supabase).

## Referência original

Jogo TecToy/Mega Drive de artilharia por turnos com terreno deformável. Este projeto expande com power-ups (Plus), composições assimétricas até 5×5, e multiplayer online com lobby público.
