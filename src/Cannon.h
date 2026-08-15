#pragma once
#include <raylib.h>
#include "Config.h"
#include "ShopCatalog.h"

enum class CannonSide { Left, Right };

// Partículas de efeito cosmético (desenhadas em Cannon.cpp).
void DrawCannonCosmeticEffect(Vector2 base, float bodyRadius, Color primary, Color accent,
                              CannonEffectStyle style);

class Cannon {
public:
    void Init(float x, float groundY, CannonSide side);

    void SetAim(float angleDeg, float power01);
    void TakeDamage(float dmg);

    bool IsAlive() const { return health > 0.5f; }
    float HealthRatio() const { return health / cfg::CANNON_MAX_HEALTH; }

    Vector2 MuzzlePosition() const;
    Vector2 AimDirection() const;
    Vector2 DirectionAtAngle(float angleDeg) const;

    // --- cosméticos da loja ---
    int colorIndex = 0;           // 0 = branco; 1–10 = cannon_N.png
    int skinOverlayIndex = 0;     // 0 = nenhum; 1+ = skin_*.png
    CannonEffectStyle cannonEffect = CannonEffectStyle::None;
    Color effectAccent = WHITE;

    void Draw(bool isCurrentTurn, Texture2D* baseSprite, Texture2D* overlaySprite) const;

    float x, groundY;
    float angleDeg = 45.0f;
    float power01  = 0.5f;
    float health    = cfg::CANNON_MAX_HEALTH;
    CannonSide side = CannonSide::Left;

    bool pendingDoubleDamage = false;
    bool queuedDoubleDamage  = false;
    bool pendingGuided       = false;
    bool queuedGuided        = false;
    int  trajectoryPreviewTurnsLeft = 0;
    int  queuedTrajectoryPreviewTurns = 0;
    int  shieldTurnsLeft            = 0;
    bool shieldPickedThisTurn       = false;

    bool HasActiveEffectIndicator() const {
        return pendingDoubleDamage || queuedDoubleDamage || pendingGuided || queuedGuided ||
               trajectoryPreviewTurnsLeft > 0 || queuedTrajectoryPreviewTurns > 0 || shieldTurnsLeft > 0;
    }

    void OnShotFired();
    void OnShotResolved();
    void OnTurnEnded();
};
