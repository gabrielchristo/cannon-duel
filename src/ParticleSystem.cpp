#include "ParticleSystem.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

static float RandRange(float lo, float hi) {
    return lo + static_cast<float>(rand()) / RAND_MAX * (hi - lo);
}

void ParticleSystem::EmitExplosion(Vector2 pos, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = RandRange(0.0f, 2.0f * PI);
        float speed = RandRange(60.0f, 260.0f);

        Particle p;
        p.pos = pos;
        p.vel = { std::cos(angle) * speed, std::sin(angle) * speed - 80.0f };
        p.maxLife = RandRange(0.4f, 0.9f);
        p.life = p.maxLife;
        p.size = RandRange(2.0f, 6.0f);

        bool debris = (i % 3 == 0);
        p.color = debris ? Color{90, 60, 30, 255} : Color{255, (unsigned char)RandRange(120, 200), 40, 255};

        particles.push_back(p);
    }
}

void ParticleSystem::EmitTrail(Vector2 pos, Vector2 velocityHint, Color color) {
    // Rastro sutil de fumaça (ou chamas, se 'color' for passada) atrás do
    // projétil — sempre visível independente do fundo ser dia ou noite.
    Particle p;
    p.pos = pos;
    // leve dispersão, com uma tendência a "ficar para trás" do movimento
    p.vel = { -velocityHint.x * 0.08f + RandRange(-12.0f, 12.0f),
              -velocityHint.y * 0.08f + RandRange(-8.0f, 4.0f) };
    p.maxLife = RandRange(0.25f, 0.4f);
    p.life = p.maxLife;
    p.size = RandRange(2.0f, 4.0f);
    p.color = color;
    particles.push_back(p);
}

void ParticleSystem::Update(float dt) {
    for (auto& p : particles) {
        p.vel.y += 320.0f * dt; // gravidade simples só para estética
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.life -= dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
                     [](const Particle& p) { return p.life <= 0.0f; }),
                     particles.end());
}

void ParticleSystem::Draw() const {
    for (const auto& p : particles) {
        float alpha = p.life / p.maxLife;
        Color c = p.color;
        c.a = static_cast<unsigned char>(255 * alpha);
        DrawCircleV(p.pos, p.size * alpha, c);
    }
}
