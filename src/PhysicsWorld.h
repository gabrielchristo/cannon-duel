#pragma once
#include <box2d/box2d.h>
#include "Config.h"

// Fina camada sobre a API em C do Box2D v3, para nos poupar de espalhar
// b2WorldId / b2BodyId "crus" pelo resto do código.
class PhysicsWorld {
public:
    PhysicsWorld() {
        b2WorldDef def = b2DefaultWorldDef();
        def.gravity = {0.0f, cfg::GRAVITY_MPS2}; // y+ aponta para baixo no nosso sistema de tela
        worldId = b2CreateWorld(&def);
    }

    ~PhysicsWorld() {
        b2DestroyWorld(worldId);
    }

    void Step(float dt) {
        b2World_Step(worldId, dt, cfg::SUB_STEP_COUNT);
    }

    b2WorldId Id() const { return worldId; }

private:
    b2WorldId worldId{};
};
