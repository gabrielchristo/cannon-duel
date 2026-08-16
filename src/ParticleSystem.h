#pragma once
#include <vector>
#include <raylib.h>

struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;     // segundos restantes
    float maxLife;
    float size;
    Color color;
    float gravityScale = 1.0f;
};

class ParticleSystem {
public:
    void EmitExplosion(Vector2 pos, int count = 40);
    void EmitTrail(Vector2 pos, Vector2 velocityHint = {0, 0}, Color color = {235, 230, 215, 255});
    void Emit(Vector2 pos, Vector2 vel, float life, float size, Color color, float gravityScale = 1.0f);
    void Update(float dt);
    void Draw() const;

private:
    std::vector<Particle> particles;
};
