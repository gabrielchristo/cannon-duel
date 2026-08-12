#pragma once

namespace cfg {

// ---- Janela ----
constexpr int   SCREEN_WIDTH   = 1280;
constexpr int   SCREEN_HEIGHT  = 720;
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

// ---- Canhão ----
constexpr float CANNON_MAX_HEALTH    = 220.0f;
constexpr float CANNON_BODY_RADIUS_PX = 18.0f;
constexpr float CANNON_MARGIN_PX     = 70.0f;   // distância mínima da borda da tela

// ---- Projétil ----
constexpr float PROJECTILE_RADIUS_PX = 5.0f;
constexpr float PROJECTILE_DENSITY   = 1.0f;
constexpr float MIN_POWER            = 6.0f;    // m/s
constexpr float MAX_POWER            = 26.0f;   // m/s
constexpr float EXPLOSION_DAMAGE_MAX = 60.0f;   // dano no impacto direto
constexpr float EXPLOSION_RADIUS_PX  = 60.0f;   // raio de dano em área

// ---- Mira (mecanismo original) ----
// Fase 1: uma linha oscila continuamente entre 0° e 90° na direção do
// oponente; clique trava o ângulo. Fase 2: uma barra de força oscila de
// verde (fraco) a vermelho (forte); clique trava a força e dispara.
constexpr float ANGLE_OSC_PERIOD_SEC = 2.4f; // tempo para ir de 0 a 90 e voltar (mais lento)
constexpr float POWER_OSC_PERIOD_SEC = 1.1f;

// ---- Vento ----
// Vento é tratado como ACELERAÇÃO (m/s²), não força bruta — assim o efeito
// não depende da massa do projétil (evita curvas absurdas na trajetória).
// Mantido bem menor que a gravidade (9.8 m/s²) para ter influência
// perceptível sem dominar a trajetória.
constexpr float WIND_MAX_ACCEL       = 1.7f;

// ---- Física ----
constexpr float GRAVITY_MPS2         = 9.8f;
constexpr int   VELOCITY_ITERATIONS  = 4; // (v3 usa subStepCount)
constexpr int   SUB_STEP_COUNT       = 4;

} // namespace cfg
