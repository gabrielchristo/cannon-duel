#pragma once

namespace cfg {

// ---- Janela ----
// HD em builds de debug (iteração mais rápida), Full HD em builds de release.
#if CANNON_DUEL_DEBUG_MODE
constexpr int   SCREEN_WIDTH   = 1280;
constexpr int   SCREEN_HEIGHT  = 720;
#else
constexpr int   SCREEN_WIDTH   = 1920;
constexpr int   SCREEN_HEIGHT  = 1080;
#endif
constexpr int   TARGET_FPS     = 60;

// ---- Escala física (pixels por metro) ----
constexpr float PPM = 30.0f; // Box2D trabalha melhor com unidades ~1-10m

inline float PxToM(float px) { return px / PPM; }
inline float MToPx(float m)  { return m * PPM; }

// ---- Terreno ----
constexpr int   TERRAIN_COLUMNS      = SCREEN_WIDTH; // 1 coluna por pixel
constexpr float TERRAIN_MIN_HEIGHT   = 120.0f;        // altura mínima (px a partir do chão)
constexpr float TERRAIN_MAX_HEIGHT   = 380.0f;
constexpr float CRATER_RADIUS_PX     = 42.0f;         // raio da cratera de explosão

// Online: crateras menores conforme mais jogadores (2→100%, 10→55%).
inline float OnlineCraterRadiusMult(int playerCount) {
    const int n = (playerCount < 2) ? 2 : (playerCount > 10 ? 10 : playerCount);
    const float t = static_cast<float>(n - 2) / 8.0f;
    return 1.0f - t * 0.45f;
}

// ---- Canhão ----
constexpr float CANNON_MAX_HEALTH    = 180.0f; // = 3x EXPLOSION_DAMAGE_MAX (3 acertos diretos derrubam o canhão)
constexpr float CANNON_BODY_RADIUS_PX = 18.0f;
constexpr float CANNON_MARGIN_PX     = 70.0f;   // distância mínima da borda da tela
constexpr float TEAM_CANNON_PAIR_SPACING_PX = 110.0f; // parceiros de equipe lado a lado

// ---- Projétil ----
constexpr float PROJECTILE_RADIUS_PX = 5.0f;
constexpr float PROJECTILE_DENSITY   = 1.0f;
constexpr float MIN_POWER            = 2.0f;    // m/s — bem baixo, pra dar tiros curtos (ex: pegar um power-up bem próximo)
constexpr float MAX_POWER            = 24.0f;   // m/s — teto da barra de força (power01=1)
constexpr float EXPLOSION_DAMAGE_MAX = 60.0f;   // dano no impacto direto
constexpr float EXPLOSION_RADIUS_PX  = 60.0f;   // raio de dano em área

// ---- Mira (mecanismo original) ----
// Fase 1: uma linha oscila continuamente entre 0° e 90° na direção do
// oponente; clique trava o ângulo. Fase 2: uma barra de força oscila de
// verde (fraco) a vermelho (forte); clique trava a força e dispara.
constexpr float ANGLE_OSC_PERIOD_SEC = 2.4f; // tempo para ir de -90 a 90 e voltar (mais lento)
constexpr float POWER_OSC_PERIOD_SEC = 1.1f;
// Enquanto a "trajetória prevista" está ativa, mira oscila bem mais devagar.
constexpr float POWERUP_TRAJECTORY_AIM_SLOWDOWN = 5.0f;

// ---- Power-ups (versão Plus) ----
constexpr float POWERUP_RADIUS_PX          = 16.0f;
constexpr float POWERUP_HIT_TOLERANCE_PX   = 14.0f; // folga extra pra facilitar o acerto
constexpr float POWERUP_MIN_PICKUP_TRAVEL_PX = 12.0f; // evita coleta fantasma no spawn do projetil
constexpr int   POWERUP_MAX_ACTIVE         = 6;     // limite pra não acumular infinitamente
constexpr int   POWERUP_SPAWN_EVERY_TURNS  = 3;    // a cada N turnos de jogador (tiros)
constexpr float POWERUP_SPAWN_CANNON_CLEARANCE_PX = 40.0f; // folga extra além dos raios do canhão/power-up
constexpr float POWERUP_HEAL_MIN_RATIO     = 0.25f;
constexpr float POWERUP_HEAL_MAX_RATIO     = 0.5f;
constexpr int   POWERUP_SHIELD_TURNS       = 1;
constexpr int   POWERUP_TRAJECTORY_TURNS   = 1;
constexpr float POWERUP_GUIDED_DAMAGE_MULT = 0.7f;
constexpr float POWERUP_GUIDED_MIN_ANGLE_DEG = 75.0f;    // evita disparo para baixo no teleguiado
constexpr float POWERUP_GUIDED_FLIGHT_SEC    = 1.85f;    // duração do arco cinemático (sempre acerta)
constexpr float POWERUP_GUIDED_ASCENT_FRAC   = 0.45f;    // fração do tempo subindo até o ápice
constexpr float POWERUP_GUIDED_TURN_RATE_DEG = 420.0f;   // fase subida (arco)
constexpr float POWERUP_GUIDED_MIN_SPEED_PX  = 480.0f;   // ignora força baixa na subida
constexpr float POWERUP_GUIDED_APEX_Y_PX       = 130.0f; // teto visual do arco
constexpr float POWERUP_GUIDED_APEX_CLEARANCE_PX = 72.0f; // acima do terreno no X do alvo
constexpr float POWERUP_GUIDED_DIVE_ENTRY_HORIZ_PX = 22.0f;
constexpr float POWERUP_GUIDED_DIVE_SPEED_PX     = 560.0f; // descida reta (px/s)
constexpr float POWERUP_GUIDED_SNAP_HORIZ_PX     = 120.0f;
constexpr float POWERUP_DOUBLE_DAMAGE_MULT = 2.0f;
// pesos relativos de sorteio (Guiado é mais raro, conforme pedido)
constexpr float POWERUP_WEIGHT_DOUBLE_DMG  = 1.0f;
constexpr float POWERUP_WEIGHT_TRAJECTORY  = 0.55f;
constexpr float POWERUP_WEIGHT_GUIDED      = 0.4f;
constexpr float POWERUP_WEIGHT_HEAL        = 1.0f;
constexpr float POWERUP_WEIGHT_SHIELD      = 1.0f;
constexpr float POWERUP_MESSAGE_DURATION_SEC = 2.0f;

// ---- Screen shake (versão Plus) ----
constexpr float SHAKE_DURATION_DIRECT_SEC  = 0.35f;
constexpr float SHAKE_DURATION_TERRAIN_SEC = 0.22f;
constexpr float SHAKE_MAGNITUDE_DIRECT_PX  = 14.0f;
constexpr float SHAKE_MAGNITUDE_TERRAIN_PX = 7.0f;

// ---- Vento ----
// Vento é tratado como ACELERAÇÃO (m/s²), não força bruta — assim o efeito
// não depende da massa do projétil (evita curvas absurdas na trajetória).
// Mantido bem menor que a gravidade (9.8 m/s²) para ter influência
// perceptível sem dominar a trajetória.
constexpr float WIND_MAX_ACCEL       = 1.7f;

// ---- Poeira ambiente ----
constexpr int   DUST_MOTE_COUNT           = 45;
constexpr float DUST_BASE_DRIFT_SPEED_PX  = 6.0f;   // deriva mínima mesmo sem vento
constexpr float DUST_WIND_SPEED_SCALE_PX  = 90.0f;  // px/s por unidade de aceleração de vento

// ---- Física ----
constexpr float GRAVITY_MPS2         = 9.8f;
constexpr int   VELOCITY_ITERATIONS  = 4; // (v3 usa subStepCount)
constexpr int   SUB_STEP_COUNT       = 4;

} // namespace cfg
