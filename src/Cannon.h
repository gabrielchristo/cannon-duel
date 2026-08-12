#pragma once
#include <raylib.h>
#include "Config.h"

enum class CannonSide { Left, Right };

class Cannon {
public:
    void Init(float x, float groundY, CannonSide side);

    void SetAim(float angleDeg, float power01); // power01 em [0,1]
    void TakeDamage(float dmg);

    bool IsAlive() const { return health > 0.0f; }
    float HealthRatio() const { return health / cfg::CANNON_MAX_HEALTH; }

    Vector2 MuzzlePosition() const; // ponta do cano, em px, ponto de spawn do projétil
    Vector2 AimDirection() const;   // vetor unitário da direção de disparo
    Vector2 DirectionAtAngle(float angleDeg) const; // como AimDirection, mas com ângulo customizado (não altera o estado)

    void Draw(bool isCurrentTurn, Texture2D* sprite = nullptr) const;

    float x, groundY;
    float angleDeg = 45.0f; // 0 = horizontal apontando para fora da tela
    float power01  = 0.5f;  // 0..1, mapeado para MIN_POWER..MAX_POWER
    float health    = cfg::CANNON_MAX_HEALTH;
    CannonSide side = CannonSide::Left;

    // --- efeitos de power-up (versão Plus) ---
    bool pendingDoubleDamage = false; // ativo NO PRÓXIMO tiro
    bool queuedDoubleDamage  = false; // acabou de pegar; só vira "pending" após o tiro atual resolver
    bool pendingGuided       = false;
    int  trajectoryPreviewTurnsLeft = 0;
    int  shieldTurnsLeft            = 0;

    bool HasActiveEffectIndicator() const {
        return pendingDoubleDamage || queuedDoubleDamage || pendingGuided ||
               trajectoryPreviewTurnsLeft > 0 || shieldTurnsLeft > 0;
    }
};
