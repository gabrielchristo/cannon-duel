# Physics

## Filosofia

Cannon Duel usa **dois sistemas de colisão complementares**:

1. **Box2D v3** — apenas para o projétil (corpo dinâmico com CCD).
2. **Heightmap 1D** — colisão projétil↔terreno e posicionamento dos canhões.

Canhões **não** são corpos Box2D. O terreno **não** tem shape física no Box2D (evita projétil grudar antes do teste de heightmap).

## Escala

Definida em `Config.h`:

| Constante | Valor | Significado |
|-----------|-------|-------------|
| `PPM` | 30 | pixels por metro |
| `GRAVITY_MPS2` | 9.8 | gravidade m/s² |
| `SUB_STEP_COUNT` | 4 | sub-steps Box2D por frame |

Conversões: `cfg::PxToM(px)`, `cfg::MToPx(m)`.

## PhysicsWorld

Wrapper fino em `PhysicsWorld.h`:

- `b2CreateWorld` com gravidade `{0, 9.8}`.
- `Step(dt)` — avança simulação.

## Projétil (`Projectile`)

### Criação

- Forma circular, raio `PROJECTILE_RADIUS_PX` (5 px).
- Densidade 1.0.
- `isBullet = true` — Continuous Collision Detection (CCD).
- Velocidade inicial: `MIN_POWER + power01 * (MAX_POWER - MIN_POWER)` → **2–24 m/s**.

### Vento

Força horizontal aplicada a cada step:

```
F = mass * windAccel
```

`windAccel` ∈ `[-WIND_MAX_ACCEL, WIND_MAX_ACCEL]` (1.7 m/s² máx.).

Massa cancela na aceleração — vento afeta todos os projéteis igualmente independente da potência.

### Vento seguro

`Game::ComputeSafeMaxWindAccel()` garante que, mesmo no pior vento contra, ainda há alcance para acertar com folga de 25% (`safetyFactor`).

Online: vento determinístico via `SeededWind(turnIndex)` a partir de `terrain_seed`.

### Modo guiado (Plus)

Power-up **Guided** desativa física Box2D durante o voo:

- Arco cinemático (`SampleGuidedPath`) com subida + mergulho.
- Dano × `POWERUP_GUIDED_DAMAGE_MULT` (0.7).
- Sempre acerta o alvo escolhido (`LowestHpEnemySlot`).

## Terreno (`Terrain`)

### Representação

- Array `heights[1280]` — altura Y da superfície por coluna.
- `HeightAt(x)` — interpola coluna de `worldX`.
- Geração: **midpoint displacement**, normalizado entre `TERRAIN_MIN_HEIGHT` (120 px) e `TERRAIN_MAX_HEIGHT` (380 px) a partir do chão.

### Colisão

```cpp
bool IsPointInside(float worldX, float worldY) {
    return worldY >= HeightAt(worldX);
}
```

Verificado a cada frame em `UpdateProjectileFlight` **antes** de confiar no Box2D.

### Destruição

`Terrain::Explode(x, y, radius)`:

- Para cada coluna no raio, calcula profundidade em formato semicircular.
- `heights[x]` aumenta (superfície desce).
- Teto: `SCREEN_HEIGHT` — coluna totalmente destruída.

Raio base: `CRATER_RADIUS_PX = 42`.

**Online:** multiplicador por contagem de jogadores:

```cpp
OnlineCraterRadiusMult(n)  // 2 jogadores → 100%, 10 jogadores → 55%
```

### Enterro (burial)

`IsFullyGone(x)` — coluna atingiu o fundo da tela (`height >= SCREEN_HEIGHT - 1`).

Canhão sobre coluna destruída → vida zerada (equipe inteira se todos os vivos enterrados em team game).

## Canhão (`Cannon`)

- Posição `(x, groundY)` — `groundY = terrain.HeightAt(x)`.
- **Sem corpo Box2D** — colisão projétil↔canhão é distância euclidiana manual.
- Vida: `CANNON_MAX_HEALTH = 180` (3 acertos diretos de `EXPLOSION_DAMAGE_MAX = 60`).
- `IsAlive()` — `health > 0.5`.

### Dano em área

Loop em `ResolveImpact`:

- Raio: `EXPLOSION_RADIUS_PX = 60`.
- Falloff linear com distância.
- Acerto direto no canhão: dano máximo.
- Escudo (`shieldTurnsLeft`) anula dano.
- Friendly fire: desligado exceto Plus + team mode.

## Detecção projétil ↔ canhão

Distância entre projétil e base do canhão ≤ `CANNON_BODY_RADIUS_PX + PROJECTILE_RADIUS_PX`.

Canhões mortos ignorados na detecção.

## Online vs local

| Aspecto | Local | Online |
|---------|-------|--------|
| Simulação do tiro | Ambos clientes (PvP) ou IA + humano | Só atirador |
| Cratera no oponente | N/A | `terrain.Explode` em `FinishRemoteTurn` |
| Vento | Aleatório por turno | Seeded por turn index |
| Cratera raio | 100% | Escalado por N jogadores |

## Constantes de referência rápida

```cpp
// Config.h (namespace cfg)
SCREEN_WIDTH          = 1280
SCREEN_HEIGHT         = 720
CRATER_RADIUS_PX      = 42
EXPLOSION_RADIUS_PX   = 60
EXPLOSION_DAMAGE_MAX  = 60
CANNON_MAX_HEALTH     = 180
CANNON_BODY_RADIUS_PX = 18
MIN_POWER             = 2.0f   // m/s
MAX_POWER             = 24.0f  // m/s
WIND_MAX_ACCEL        = 1.7f   // m/s²
ANGLE_OSC_PERIOD_SEC  = 2.4f
POWER_OSC_PERIOD_SEC  = 1.1f
```
