#include "Projectile.h"
#include <algorithm>
#include <cmath>

void Projectile::Spawn(b2WorldId world, Vector2 startPosPx, Vector2 dirUnit, float power01) {
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = { cfg::PxToM(startPosPx.x), cfg::PxToM(startPosPx.y) };
    bodyDef.isBullet = true; // CCD: evita atravessar o terreno em alta velocidade
    body = b2CreateBody(world, &bodyDef);

    b2Circle circle{};
    circle.radius = cfg::PxToM(cfg::PROJECTILE_RADIUS_PX);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = cfg::PROJECTILE_DENSITY;
    shapeDef.material.friction = 0.3f;
    shapeDef.material.restitution = 0.05f;
    b2CreateCircleShape(body, &shapeDef, &circle);

    float speed = cfg::MIN_POWER + power01 * (cfg::MAX_POWER - cfg::MIN_POWER);
    b2Vec2 vel = { dirUnit.x * speed, dirUnit.y * speed };
    b2Body_SetLinearVelocity(body, vel);

    active = true;
}

void Projectile::ApplyWind(float windAccelMps2) {
    if (!active) return;
    // Força = massa * aceleração, assim o vento tem o mesmo efeito
    // proporcional independente da massa/densidade do projétil.
    float mass = b2Body_GetMass(body);
    b2Vec2 f = { windAccelMps2 * mass, 0.0f };
    b2Body_ApplyForceToCenter(body, f, true);
}

Vector2 Projectile::PositionPx() const {
    b2Vec2 p = b2Body_GetPosition(body);
    return { cfg::MToPx(p.x), cfg::MToPx(p.y) };
}

Vector2 Projectile::VelocityPx() const {
    b2Vec2 v = b2Body_GetLinearVelocity(body);
    return { cfg::MToPx(v.x), cfg::MToPx(v.y) };
}

void Projectile::ApplyGuidance(Vector2 targetPx, float turnRateDegPerSec, float minSpeedPx, float dt) {
    if (!active) return;

    Vector2 vel = VelocityPx();
    float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    speed = std::max(speed, minSpeedPx);

    Vector2 pos = PositionPx();
    Vector2 toTarget = { targetPx.x - pos.x, targetPx.y - pos.y };

    float curAngle = std::atan2(vel.y, vel.x);
    float desiredAngle = std::atan2(toTarget.y, toTarget.x);

    float diff = desiredAngle - curAngle;
    while (diff > PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;

    float maxTurn = (turnRateDegPerSec * DEG2RAD) * dt;
    float turn = std::clamp(diff, -maxTurn, maxTurn);
    float newAngle = curAngle + turn;

    Vector2 newVelPx = { std::cos(newAngle) * speed, std::sin(newAngle) * speed };
    b2Vec2 newVelM = { cfg::PxToM(newVelPx.x), cfg::PxToM(newVelPx.y) };
    b2Body_SetLinearVelocity(body, newVelM);
}

void Projectile::ApplyGuidedDive(Vector2 targetPx, float diveSpeedPxPerSec, float dt) {
    if (!active) return;
    (void)dt;

    Vector2 pos = PositionPx();
    float dx = targetPx.x - pos.x;
    const float horizGain = 12.0f;
    Vector2 desired = { dx * horizGain, diveSpeedPxPerSec };
    float len = std::sqrt(desired.x * desired.x + desired.y * desired.y);
    if (len < 1.0f) {
        desired = { 0.0f, diveSpeedPxPerSec };
        len = diveSpeedPxPerSec;
    }
    desired.x = desired.x / len * diveSpeedPxPerSec;
    desired.y = desired.y / len * diveSpeedPxPerSec;

    b2Vec2 velM = { cfg::PxToM(desired.x), cfg::PxToM(desired.y) };
    b2Body_SetLinearVelocity(body, velM);
}

void Projectile::Destroy() {
    if (active) {
        b2DestroyBody(body);
        active = false;
    }
}
