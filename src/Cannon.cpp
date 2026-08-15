#include "Cannon.h"
#include "CosmeticShaders.h"
#include <cmath>
#include <algorithm>

namespace {

void CannonSpriteLayout(Vector2 base, const Texture2D& sprite, bool flipH,
                        Rectangle* src, Rectangle* dst, Vector2* origin) {
    const float scale = (cfg::CANNON_BODY_RADIUS_PX * 2.6f) / sprite.width;
    *origin = { sprite.width * scale * 0.5f, sprite.height * scale * 0.62f };
    *src = { 0, 0, static_cast<float>(sprite.width), static_cast<float>(sprite.height) };
    const float drawW = sprite.width * scale * (flipH ? -1.0f : 1.0f);
    *dst = { base.x, base.y, drawW, sprite.height * scale };
}

void DrawCannonSprite(Vector2 base, Texture2D* sprite, Color tint, bool flipH) {
    Rectangle src{}, dst{};
    Vector2 origin{};
    CannonSpriteLayout(base, *sprite, flipH, &src, &dst, &origin);
    DrawTexturePro(*sprite, src, dst, origin, 0.0f, tint);
}

} // namespace

void DrawCannonCosmeticEffect(Vector2 base, float bodyR, Color primary, Color accent,
                              CannonEffectStyle style) {
    if (style == CannonEffectStyle::None) return;

    const float t = static_cast<float>(GetTime());
    const auto mote = [&](float px, float py, float radius, Color col, float alpha) {
        DrawCircleV({ px, py }, radius, Fade(col, std::clamp(alpha, 0.0f, 1.0f)));
    };

    switch (style) {
        case CannonEffectStyle::AuraSoft: {
            // Coluna suave: sobe devagar, pouco drift.
            for (int i = 0; i < 10; ++i) {
                const float seed = i * 1.618f;
                const float life = fmodf(t * 0.28f + seed * 0.21f, 1.0f);
                const float px = base.x + std::sin(seed * 2.4f + t * 0.45f) * (bodyR * 0.55f);
                const float py = base.y - life * bodyR * 2.1f;
                const float alpha = std::sin(life * PI) * 0.48f;
                mote(px, py, 1.6f + (1.0f - life) * 1.8f, accent, alpha);
            }
            break;
        }
        case CannonEffectStyle::AuraFire: {
            // Línguas: sobem do casco e oscilam para os lados.
            for (int i = 0; i < 9; ++i) {
                const float seed = i * 2.13f;
                const float life = fmodf(t * 0.7f + seed * 0.17f, 1.0f);
                const float sway = std::sin(t * 7.0f + seed) * bodyR * 0.22f;
                const float px = base.x + (static_cast<float>(i) / 8.0f - 0.5f) * bodyR * 1.1f + sway;
                const float py = base.y + bodyR * 0.25f - life * life * bodyR * 2.8f;
                const float alpha = (1.0f - life) * 0.7f;
                mote(px, py, 2.4f + (1.0f - life) * 2.2f, Color{ 255, 90, 20, 255 }, alpha);
                mote(px, py - 3.0f, 1.2f, Color{ 255, 220, 80, 255 }, alpha * 0.75f);
            }
            break;
        }
        case CannonEffectStyle::ImbueHoly: {
            // Pulso no lugar + subida curta (auréola).
            for (int i = 0; i < 8; ++i) {
                const float seed = i * 1.9f;
                const float life = fmodf(t * 0.24f + seed * 0.19f, 1.0f);
                const float ang = seed * 0.9f;
                const float pulse = std::sin(life * PI);
                const float px = base.x + std::cos(ang) * bodyR * (0.15f + pulse * 0.55f);
                const float py = base.y - bodyR * 0.1f - life * bodyR * 1.4f;
                mote(px, py, 1.1f + pulse * 2.4f, accent, pulse * 0.7f);
                mote(px, py, 0.6f, WHITE, pulse * 0.45f);
            }
            break;
        }
        case CannonEffectStyle::DebuffGlow: {
            const Color sick{ 40, 210, 70, 255 };
            const Color slime{ 90, 255, 120, 255 };
            for (int i = 0; i < 8; ++i) {
                const float seed = i * 2.5f;
                const float life = fmodf(t * 0.38f + seed * 0.14f, 1.0f);
                const float px = base.x + std::sin(seed + life * 1.2f) * bodyR * 0.65f;
                const float py = base.y - bodyR * 0.7f + life * life * bodyR * 2.2f;
                const float alpha = (1.0f - life) * 0.6f;
                mote(px, py, 2.2f + life * 2.6f, sick, alpha);
                mote(px, py + 2.0f, 1.1f, slime, alpha * 0.7f);
            }
            break;
        }
        case CannonEffectStyle::ArcaneSpark: {
            // Twist em espiral (identidade aprovada).
            for (int i = 0; i < 14; ++i) {
                const float seed = i * 1.37f;
                const float life = fmodf(t * 0.32f + seed * 0.13f, 1.0f);
                const float spiral = seed + life * 4.0f;
                const float px = base.x + std::cos(spiral + t * 0.5f) * (bodyR * (0.45f + life * 0.55f));
                const float py = base.y - life * bodyR * 2.4f;
                const float alpha = std::sin(life * PI) * 0.6f;
                mote(px, py, 1.4f + (1.0f - life) * 1.6f, accent, alpha);
            }
            break;
        }
        case CannonEffectStyle::LiquidInferno: {
            // Labaredas: aceleram para cima a partir das esteiras.
            for (int i = 0; i < 11; ++i) {
                const float seed = i * 1.7f;
                const float life = fmodf(t * 0.85f + seed * 0.2f, 1.0f);
                const float px = base.x + std::sin(seed * 3.0f + t * 2.2f) * bodyR * (0.7f - life * 0.25f);
                const float py = base.y + bodyR * 0.45f - life * life * bodyR * 3.2f;
                const float alpha = (1.0f - life) * 0.65f;
                mote(px, py, 2.8f * (1.0f - life * 0.5f), Color{ 255, 70, 10, 255 }, alpha);
                mote(px, py - 4.0f, 1.3f, Color{ 255, 210, 70, 255 }, alpha * 0.8f);
            }
            break;
        }
        case CannonEffectStyle::LiquidFrost: {
            // Cristais: avançam na horizontal e piscam.
            for (int i = 0; i < 8; ++i) {
                const float seed = i * 2.2f;
                const float life = fmodf(t * 0.33f + seed * 0.18f, 1.0f);
                const float side = (i % 2 == 0) ? -1.0f : 1.0f;
                const float px = base.x + side * (bodyR * 0.2f + life * bodyR * 1.6f);
                const float py = base.y - bodyR * 0.15f + std::sin(seed + t * 1.4f) * bodyR * 0.35f;
                const float blink = 0.35f + 0.65f * std::abs(std::sin(t * 8.0f + seed));
                const float alpha = std::sin(life * PI) * 0.55f * blink;
                mote(px, py, 1.5f + blink, accent, alpha);
                mote(px, py, 0.6f, WHITE, alpha * 0.8f);
            }
            break;
        }
        case CannonEffectStyle::AuraCunt: {
            static const Color kPride[] = {
                { 228, 28, 36, 255 }, { 250, 140, 20, 255 }, { 250, 220, 30, 255 },
                { 40, 170, 70, 255 }, { 50, 100, 220, 255 }, { 150, 40, 170, 255 }
            };
            for (int i = 0; i < 16; ++i) {
                const float seed = static_cast<float>(i) * 0.47f;
                const float life = fmodf(t * 0.5f + seed * 0.12f, 1.0f);
                const float ang = seed + t * 1.05f + std::sin(t * 2.1f + seed) * 0.35f;
                const float rad = bodyR * (0.85f + 0.7f * std::sin(life * PI));
                const float px = base.x + std::cos(ang) * rad;
                const float py = base.y + std::sin(ang) * rad * 0.76f;
                mote(px, py, 1.7f + (1.0f - life) * 1.3f, kPride[i % 6], std::sin(life * PI) * 0.7f);
            }
            break;
        }
        case CannonEffectStyle::LiquidVoid: {
            // Implosão: orbitam e são sugadas para o centro.
            for (int i = 0; i < 12; ++i) {
                const float seed = i * 1.5f;
                const float life = fmodf(t * 0.4f + seed * 0.16f, 1.0f);
                const float ang = seed * 1.7f + t * 1.1f - life * 3.5f;
                const float rad = bodyR * (1.35f - life * 1.2f);
                const float px = base.x + std::cos(ang) * rad;
                const float py = base.y + std::sin(ang) * rad * 0.72f;
                const float alpha = std::sin(life * PI) * 0.55f;
                mote(px, py, 1.8f * (1.0f - life * 0.6f), accent, alpha);
            }
            break;
        }
        default:
            break;
    }
    (void)primary;
}

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
    colorIndex = 0;
    skinOverlayIndex = 0;
    cannonEffect = CannonEffectStyle::None;
    effectAccent = WHITE;
}

void Cannon::SetAim(float pAngleDeg, float pPower01) {
    angleDeg = std::clamp(pAngleDeg, -90.0f, 90.0f);
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
    float dirY = -std::sin(rad);
    if (side == CannonSide::Right) dirX = -dirX;
    return {dirX, dirY};
}

Vector2 Cannon::MuzzlePosition() const {
    Vector2 dir = AimDirection();
    float barrelLen = cfg::CANNON_BODY_RADIUS_PX + 22.0f;
    return { x + dir.x * barrelLen, groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f + dir.y * barrelLen };
}

void Cannon::Draw(bool isCurrentTurn, Texture2D* baseSprite, Texture2D* overlaySprite) const {
    Vector2 base = { x, groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
    const float r = cfg::CANNON_BODY_RADIUS_PX;
    const bool flipH = (side == CannonSide::Right);

    if (cannonEffect != CannonEffectStyle::None) {
        gCosmeticShaders.DrawCannonEnergyField(base, r, effectAccent, effectAccent, cannonEffect);
    }

    if (baseSprite && baseSprite->id != 0) {
        if (cannonEffect != CannonEffectStyle::None) {
            Rectangle src{}, dst{};
            Vector2 origin{};
            CannonSpriteLayout(base, *baseSprite, flipH, &src, &dst, &origin);
            gCosmeticShaders.DrawCannonOutline(*baseSprite, src, dst, origin,
                                               cannonEffect, effectAccent);
            gCosmeticShaders.DrawCannonCoating(*baseSprite, src, dst, origin,
                                               cannonEffect, effectAccent, effectAccent);
            DrawCannonSprite(base, baseSprite, WHITE, flipH);
        } else {
            DrawCannonSprite(base, baseSprite, WHITE, flipH);
        }
    } else {
        Color bodyColor = (side == CannonSide::Left) ? Color{248, 250, 252, 255} : Color{230, 232, 238, 255};
        Vector2 muzzle = MuzzlePosition();
        DrawLineEx(base, muzzle, 7.0f, DARKGRAY);
        DrawCircleV(base, r, bodyColor);
        DrawCircleLines(static_cast<int>(base.x), static_cast<int>(base.y), r, BLACK);
    }

    if (overlaySprite && overlaySprite->id != 0) {
        DrawCannonSprite(base, overlaySprite, WHITE, flipH);
    }

    DrawCannonCosmeticEffect(base, r, effectAccent, effectAccent, cannonEffect);

    Vector2 muzzleReal = MuzzlePosition();
    DrawLineEx(base, muzzleReal, 3.0f, Fade(BLACK, 0.55f));

    float ratioForSmoke = HealthRatio();
    if (ratioForSmoke < 0.6f) {
        float intensity = 1.0f - (ratioForSmoke / 0.6f);
        float t = static_cast<float>(GetTime());
        int puffCount = 1 + static_cast<int>(intensity * 3.0f);

        for (int i = 0; i < puffCount; ++i) {
            float phase = t * (0.8f + i * 0.35f) + i * 2.1f;
            float bob = std::sin(phase) * 4.0f;
            float rise = fmodf(phase * 6.0f, 30.0f);
            float px = base.x + std::sin(phase * 0.6f + i) * 8.0f;
            float py = base.y - cfg::CANNON_BODY_RADIUS_PX * 0.4f - rise + bob * 0.2f;
            float alpha = (1.0f - rise / 30.0f) * (0.25f + intensity * 0.45f);
            float radius = 4.0f + intensity * 6.0f + (rise / 30.0f) * 4.0f;
            Color smokeColor = { 60, 60, 60, static_cast<unsigned char>(alpha * 255) };
            DrawCircleV({ px, py }, radius, smokeColor);
        }
    }

    if (isCurrentTurn) {
        DrawCircleLines(static_cast<int>(base.x), static_cast<int>(base.y - cfg::CANNON_BODY_RADIUS_PX - 14),
                         5, YELLOW);
    }

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

    if (HasActiveEffectIndicator()) {
        float badgeX = barPos.x;
        float badgeY = barPos.y - 12;
        auto badge = [&](Color c) {
            DrawCircle(static_cast<int>(badgeX), static_cast<int>(badgeY), 5, c);
            DrawCircleLines(static_cast<int>(badgeX), static_cast<int>(badgeY), 5, BLACK);
            badgeX += 13;
        };
        if (pendingDoubleDamage) badge(Color{220, 60, 40, 255});
        if (queuedDoubleDamage)  badge(Color{240, 140, 60, 255});
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
