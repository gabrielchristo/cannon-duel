#include "Projectile.h"
#include <algorithm>

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

void Projectile::Destroy() {
    if (active) {
        b2DestroyBody(body);
        active = false;
    }
}
