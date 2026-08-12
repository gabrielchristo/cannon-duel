#pragma once
#include <box2d/box2d.h>
#include <raylib.h>
#include "Config.h"

class Projectile {
public:
    void Spawn(b2WorldId world, Vector2 startPosPx, Vector2 dirUnit, float power01);
    void Destroy();

    Vector2 PositionPx() const;
    Vector2 VelocityPx() const;
    bool IsActive() const { return active; }

    void ApplyWind(float windForce);
    // Curva suavemente a velocidade em direção ao alvo (usado pelo power-up
    // "teleguiado") — gira o vetor velocidade, preservando a magnitude, a
    // uma taxa máxima de graus/segundo.
    void ApplyGuidance(Vector2 targetPx, float turnRateDegPerSec, float minSpeedPx, float dt);
    // Fase final do teleguiado: desce em linha reta sobre o alvo (ignora vento).
    void ApplyGuidedDive(Vector2 targetPx, float diveSpeedPxPerSec, float dt);

    b2BodyId body{};
    bool active = false;
};
