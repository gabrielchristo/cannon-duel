#include "Cannon.h"
#include <cmath>
#include <algorithm>

void Cannon::Init(float px, float pGroundY, CannonSide pSide) {
    x = px;
    groundY = pGroundY;
    side = pSide;
    health = cfg::CANNON_MAX_HEALTH;
    angleDeg = 45.0f;
    power01 = 0.5f;
    pendingDoubleDamage = false;
    queuedDoubleDamage = false;
    pendingGuided = false;
    queuedGuided = false;
    trajectoryPreviewTurnsLeft = 0;
    queuedTrajectoryPreviewTurns = 0;
    shieldTurnsLeft = 0;
    shieldPickedThisTurn = false;
    tintColor = WHITE;
}

void Cannon::SetAim(float pAngleDeg, float pPower01) {
    angleDeg = std::clamp(pAngleDeg, -90.0f, 90.0f); // de reto pra baixo a reto pra cima
    power01  = std::clamp(pPower01, 0.0f, 1.0f);
}

void Cannon::TakeDamage(float dmg) {
    health = std::max(0.0f, health - dmg);
    if (health <= 0.5f) health = 0.0f;
}

Vector2 Cannon::AimDirection() const {
    return DirectionAtAngle(angleDeg);
}

Vector2 Cannon::DirectionAtAngle(float customAngleDeg) const {
    float rad = customAngleDeg * DEG2RAD;
    float dirX = std::cos(rad);
    float dirY = -std::sin(rad); // y cresce para baixo na tela
    if (side == CannonSide::Right) dirX = -dirX;
    return {dirX, dirY};
}

Vector2 Cannon::MuzzlePosition() const {
    Vector2 dir = AimDirection();
    float barrelLen = cfg::CANNON_BODY_RADIUS_PX + 22.0f;
    return { x + dir.x * barrelLen, groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f + dir.y * barrelLen };
}

void Cannon::Draw(bool isCurrentTurn, Texture2D* sprite) const {
    Vector2 base = { x, groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
    const bool customColor = !(tintColor.a == 255 && tintColor.r == 255 &&
                               tintColor.g == 255 && tintColor.b == 255);

    if (sprite && sprite->id != 0 && !customColor) {
        // Sprite estático (o cano é desenhado separadamente por cima,
        // rotacionado, já que a arte placeholder tem o cano em ângulo fixo).
        float scale = (cfg::CANNON_BODY_RADIUS_PX * 2.6f) / sprite->width;
        Vector2 origin = { sprite->width * scale * 0.5f, sprite->height * scale * 0.62f };
        Rectangle src = { 0, 0, (float)sprite->width, (float)sprite->height };
        Rectangle dst = { base.x, base.y, sprite->width * scale, sprite->height * scale };
        DrawTexturePro(*sprite, src, dst, origin, 0.0f, WHITE);
    } else {
        Color bodyColor = customColor
            ? tintColor
            : ((side == CannonSide::Left) ? Color{60, 120, 220, 255} : Color{220, 70, 60, 255});
        // cano
        Vector2 muzzle = MuzzlePosition();
        DrawLineEx(base, muzzle, 7.0f, DARKGRAY);

        // corpo (tanque)
        DrawCircleV(base, cfg::CANNON_BODY_RADIUS_PX, bodyColor);
        DrawCircleLines(static_cast<int>(base.x), static_cast<int>(base.y),
                         cfg::CANNON_BODY_RADIUS_PX, BLACK);
        if (customColor) {
            DrawCircleLines(static_cast<int>(base.x), static_cast<int>(base.y),
                            cfg::CANNON_BODY_RADIUS_PX + 3, Fade(bodyColor, 0.85f));
        }
    }

    // Indicador fino do ângulo real (sobreposto ao sprite, já que a arte
    // placeholder tem o cano desenhado num ângulo fixo em vez de rotacionar
    // dinamicamente com o corpo do tanque).
    Vector2 muzzleReal = MuzzlePosition();
    DrawLineEx(base, muzzleReal, 3.0f, Fade(BLACK, 0.55f));

    // Fumaça de dano: quanto menor a vida, mais/maior as "baforadas" de
    // fumaça saindo do corpo do tanque (puramente estético, animado com o
    // tempo, sem depender de sprite extra).
    float ratioForSmoke = HealthRatio();
    if (ratioForSmoke < 0.6f) {
        float intensity = 1.0f - (ratioForSmoke / 0.6f); // 0 (saudável) .. 1 (crítico)
        float t = static_cast<float>(GetTime());
        int puffCount = 1 + static_cast<int>(intensity * 3.0f);

        for (int i = 0; i < puffCount; ++i) {
            float phase = t * (0.8f + i * 0.35f) + i * 2.1f;
            float bob = std::sin(phase) * 4.0f;
            float rise = fmodf(phase * 6.0f, 30.0f); // sobe e "reinicia"
            float px = base.x + std::sin(phase * 0.6f + i) * 8.0f;
            float py = base.y - cfg::CANNON_BODY_RADIUS_PX * 0.4f - rise + bob * 0.2f;
            float alpha = (1.0f - rise / 30.0f) * (0.25f + intensity * 0.45f);
            float radius = 4.0f + intensity * 6.0f + (rise / 30.0f) * 4.0f;

            Color smokeColor = { 60, 60, 60, static_cast<unsigned char>(alpha * 255) };
            DrawCircleV({ px, py }, radius, smokeColor);
        }
    }

    // indicador de turno
    if (isCurrentTurn) {
        DrawCircleLines(static_cast<int>(base.x), static_cast<int>(base.y - cfg::CANNON_BODY_RADIUS_PX - 14),
                         5, YELLOW);
    }

    // barra de vida acima do canhão
    float barW = 50.0f, barH = 7.0f;
    Vector2 barPos = { x - barW / 2, groundY - cfg::CANNON_BODY_RADIUS_PX - 26 };
    DrawRectangle(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                  static_cast<int>(barW), static_cast<int>(barH), Color{40, 40, 40, 220});
    float ratio = HealthRatio();
    Color hpColor = ratio > 0.5f ? GREEN : (ratio > 0.25f ? ORANGE : RED);
    DrawRectangle(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                  static_cast<int>(barW * ratio), static_cast<int>(barH), hpColor);
    DrawRectangleLines(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                        static_cast<int>(barW), static_cast<int>(barH), BLACK);

    // pequenos indicadores de efeitos ativos (power-ups) acima da barra de vida
    if (HasActiveEffectIndicator()) {
        float badgeX = barPos.x;
        float badgeY = barPos.y - 12;
        auto badge = [&](Color c) {
            DrawCircle(static_cast<int>(badgeX), static_cast<int>(badgeY), 5, c);
            DrawCircleLines(static_cast<int>(badgeX), static_cast<int>(badgeY), 5, BLACK);
            badgeX += 13;
        };
        if (pendingDoubleDamage) badge(Color{220, 60, 40, 255});
        if (queuedDoubleDamage)  badge(Color{240, 140, 60, 255}); // "vai ativar no próximo tiro"
        if (pendingGuided)       badge(Color{150, 70, 200, 255});
        if (queuedGuided)        badge(Color{190, 120, 220, 255});
        if (trajectoryPreviewTurnsLeft > 0) badge(Color{60, 130, 220, 255});
        if (shieldTurnsLeft > 0) badge(Color{60, 200, 210, 255});
    }
}

void Cannon::OnShotFired() {
    if (trajectoryPreviewTurnsLeft > 0) trajectoryPreviewTurnsLeft--;
}

void Cannon::OnShotResolved() {
    pendingDoubleDamage = false;
    pendingGuided = false;

    if (queuedDoubleDamage) {
        pendingDoubleDamage = true;
        queuedDoubleDamage = false;
    }
    if (queuedGuided) {
        pendingGuided = true;
        queuedGuided = false;
    }
    if (queuedTrajectoryPreviewTurns > 0) {
        trajectoryPreviewTurnsLeft = queuedTrajectoryPreviewTurns;
        queuedTrajectoryPreviewTurns = 0;
    }
}

void Cannon::OnTurnEnded() {
    if (shieldPickedThisTurn) {
        shieldPickedThisTurn = false;
        return;
    }
    if (shieldTurnsLeft > 0) shieldTurnsLeft--;
}
