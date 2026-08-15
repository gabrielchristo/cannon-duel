#include "AmmoVisuals.h"
#include "ParticleSystem.h"
#include "Config.h"

#include <cmath>
#include <cstdlib>

namespace {

float RandRange(float lo, float hi) {
    return lo + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

Color PrideColor(int i) {
    static const Color kPride[] = {
        { 228, 28, 36, 255 }, { 250, 140, 20, 255 }, { 250, 220, 30, 255 },
        { 40, 170, 70, 255 }, { 50, 100, 220, 255 }, { 150, 40, 170, 255 }
    };
    return kPride[((i % 6) + 6) % 6];
}

void Burst(ParticleSystem& particles, Vector2 pos, int count, Color a, Color b,
           float speedMin, float speedMax, float sizeMin, float sizeMax,
           float gravity = 1.0f) {
    for (int i = 0; i < count; ++i) {
        const float ang = RandRange(0.0f, 2.0f * PI);
        const float speed = RandRange(speedMin, speedMax);
        particles.Emit(pos, { std::cos(ang) * speed, std::sin(ang) * speed - 40.0f },
                       RandRange(0.35f, 0.85f), RandRange(sizeMin, sizeMax),
                       (i % 2 == 0) ? a : b, gravity);
    }
}

void DrawTrendHand(Vector2 palm, float hs, float side) {
    const Color skin{ 255, 214, 176, 255 };
    const Color line{ 40, 26, 18, 255 };
    DrawEllipse(static_cast<int>(palm.x), static_cast<int>(palm.y),
                static_cast<int>(hs * 0.78f), static_cast<int>(hs * 0.92f), line);
    DrawEllipse(static_cast<int>(palm.x), static_cast<int>(palm.y),
                static_cast<int>(hs * 0.62f), static_cast<int>(hs * 0.74f), skin);
    for (int f = 0; f < 4; ++f) {
        const float ox = (static_cast<float>(f) - 1.5f) * hs * 0.36f;
        const float fy = palm.y - hs * 1.05f;
        DrawEllipse(static_cast<int>(palm.x + ox), static_cast<int>(fy),
                    static_cast<int>(hs * 0.20f), static_cast<int>(hs * 0.46f), line);
        DrawEllipse(static_cast<int>(palm.x + ox), static_cast<int>(fy),
                    static_cast<int>(hs * 0.14f), static_cast<int>(hs * 0.36f), skin);
    }
    DrawEllipse(static_cast<int>(palm.x + side * hs * 0.62f),
                static_cast<int>(palm.y - hs * 0.08f),
                static_cast<int>(hs * 0.24f), static_cast<int>(hs * 0.18f), skin);
}

void DrawLightningBolt(Vector2 from, float ang, float length, float jag, Color outer, Color inner) {
    Vector2 a = from;
    const int segs = 5;
    for (int i = 1; i <= segs; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(segs);
        const float wobble = std::sin(ang * 3.0f + u * 18.0f + GetTime() * 40.0f) * jag;
        const Vector2 b{
            from.x + std::cos(ang) * length * u + std::cos(ang + PI * 0.5f) * wobble,
            from.y + std::sin(ang) * length * u + std::sin(ang + PI * 0.5f) * wobble
        };
        DrawLineEx(a, b, 2.4f, outer);
        DrawLineEx(a, b, 1.1f, inner);
        a = b;
    }
}

void DrawSpriteOrCircle(Vector2 pos, const Texture2D* sprite, float radius, float rotation, Color tint) {
    if (sprite && sprite->id != 0) {
        const float d = radius * 2.4f;
        DrawTexturePro(*sprite,
                       { 0, 0, static_cast<float>(sprite->width), static_cast<float>(sprite->height) },
                       { pos.x, pos.y, d, d }, { d * 0.5f, d * 0.5f }, rotation, tint);
    } else {
        DrawCircleV(pos, radius, tint);
    }
}

} // namespace

void EmitAmmoTrail(ParticleSystem& particles, Vector2 pos, Vector2 vel,
                   AmmoStyle ammo, bool powerupDouble, bool powerupGuided) {
    if (powerupDouble) {
        particles.EmitTrail(pos, vel, Color{ 255, 130, 40, 255 });
        return;
    }
    if (powerupGuided) {
        particles.EmitTrail(pos, vel, Color{ 180, 120, 230, 255 });
        return;
    }

    const float t = static_cast<float>(GetTime());
    switch (ammo) {
        case AmmoStyle::Ice:
            particles.EmitTrail(pos, vel, Color{ 160, 230, 255, 255 });
            particles.Emit(pos, { RandRange(-20, 20), RandRange(-10, 6) },
                           0.28f, 2.2f, Color{ 230, 250, 255, 255 }, 0.4f);
            break;
        case AmmoStyle::Rasengan: {
            const float ang = t * 16.0f;
            for (int i = 0; i < 3; ++i) {
                const float a = ang + static_cast<float>(i) * 2.094f;
                const Vector2 p{ pos.x + std::cos(a) * 7.0f, pos.y + std::sin(a) * 7.0f };
                particles.Emit(p, { -vel.x * 0.05f, -vel.y * 0.05f }, 0.22f, 2.4f,
                               (i == 0) ? Color{ 70, 160, 255, 255 } : Color{ 180, 230, 255, 255 }, 0.15f);
            }
            break;
        }
        case AmmoStyle::Chidori:
            particles.EmitTrail(pos, vel, Color{ 140, 220, 255, 255 });
            for (int i = 0; i < 6; ++i) {
                particles.Emit({ pos.x + RandRange(-12, 12), pos.y + RandRange(-12, 12) },
                               { RandRange(-90, 90), RandRange(-90, 90) },
                               0.1f, RandRange(1.2f, 2.4f),
                               (i % 2 == 0) ? Color{ 230, 250, 255, 255 } : Color{ 80, 170, 255, 255 }, 0.0f);
            }
            break;
        case AmmoStyle::Shuriken:
            particles.EmitTrail(pos, vel, Color{ 170, 175, 185, 220 });
            break;
        case AmmoStyle::Kuromi:
            particles.EmitTrail(pos, vel, Color{ 210, 90, 230, 255 });
            break;
        case AmmoStyle::Pride:
            particles.EmitTrail(pos, vel, PrideColor(static_cast<int>(t * 8.0f)));
            break;
        case AmmoStyle::SixSeven:
            particles.EmitTrail(pos, vel, Color{ 255, 220, 70, 255 });
            break;
        case AmmoStyle::Tomato:
            particles.EmitTrail(pos, vel, Color{ 220, 50, 40, 255 });
            break;
        case AmmoStyle::Duck:
            particles.Emit(pos, { RandRange(-8, 8), RandRange(-18, -4) },
                           0.4f, 2.8f, Color{ 210, 240, 255, 220 }, -0.35f);
            break;
        case AmmoStyle::Nuclear:
            particles.EmitTrail(pos, vel, Color{ 80, 90, 70, 255 });
            particles.EmitTrail(pos, vel, Color{ 255, 200, 40, 200 });
            break;
        default:
            particles.EmitTrail(pos, vel);
            break;
    }
}

void EmitAmmoImpact(ParticleSystem& particles, Vector2 pos, AmmoStyle ammo) {
    switch (ammo) {
        case AmmoStyle::Ice:
            Burst(particles, pos, 36, Color{ 140, 220, 255, 255 }, Color{ 240, 250, 255, 255 },
                  70.0f, 240.0f, 1.6f, 4.2f, 0.55f);
            break;
        case AmmoStyle::Rasengan:
            Burst(particles, pos, 42, Color{ 50, 140, 255, 255 }, Color{ 180, 230, 255, 255 },
                  90.0f, 280.0f, 2.0f, 5.0f, 0.35f);
            break;
        case AmmoStyle::Chidori:
            Burst(particles, pos, 40, Color{ 120, 210, 255, 255 }, Color{ 255, 255, 255, 255 },
                  120.0f, 340.0f, 1.4f, 3.6f, 0.2f);
            break;
        case AmmoStyle::Shuriken:
            Burst(particles, pos, 28, Color{ 160, 165, 175, 255 }, Color{ 230, 230, 235, 255 },
                  80.0f, 220.0f, 1.5f, 3.4f, 0.8f);
            break;
        case AmmoStyle::Kuromi:
            Burst(particles, pos, 44, Color{ 160, 40, 210, 255 }, Color{ 255, 120, 220, 255 },
                  80.0f, 260.0f, 2.2f, 6.0f, 0.7f);
            break;
        case AmmoStyle::Pride:
            for (int i = 0; i < 42; ++i) {
                const float ang = RandRange(0.0f, 2.0f * PI);
                const float speed = RandRange(70.0f, 250.0f);
                particles.Emit(pos, { std::cos(ang) * speed, std::sin(ang) * speed - 50.0f },
                               RandRange(0.4f, 0.9f), RandRange(2.0f, 5.5f), PrideColor(i), 0.7f);
            }
            break;
        case AmmoStyle::SixSeven:
            Burst(particles, pos, 30, Color{ 255, 220, 60, 255 }, Color{ 255, 250, 180, 255 },
                  60.0f, 200.0f, 2.4f, 6.0f, 0.6f);
            break;
        case AmmoStyle::Tomato:
            Burst(particles, pos, 38, Color{ 210, 30, 30, 255 }, Color{ 255, 80, 60, 255 },
                  70.0f, 230.0f, 2.0f, 5.5f, 1.0f);
            particles.Emit(pos, { 0, -40 }, 0.5f, 3.0f, Color{ 50, 140, 40, 255 }, 0.8f);
            break;
        case AmmoStyle::Duck:
            for (int i = 0; i < 36; ++i) {
                particles.Emit({ pos.x + RandRange(-10, 10), pos.y + RandRange(-6, 6) },
                               { RandRange(-30, 30), RandRange(-90, -20) },
                               RandRange(0.5f, 1.1f), RandRange(2.5f, 6.5f),
                               Color{ 200, 235, 255, 230 }, -0.45f);
            }
            break;
        case AmmoStyle::Nuclear:
            Burst(particles, pos, 90, Color{ 255, 230, 80, 255 }, Color{ 255, 90, 20, 255 },
                  80.0f, 420.0f, 4.0f, 16.0f, 0.55f);
            Burst(particles, pos, 70, Color{ 255, 255, 240, 255 }, Color{ 255, 160, 40, 255 },
                  40.0f, 180.0f, 6.0f, 20.0f, 0.25f);
            for (int i = 0; i < 50; ++i) {
                particles.Emit(pos, { RandRange(-50, 50), RandRange(-380, -120) },
                               RandRange(0.7f, 1.4f), RandRange(5.0f, 14.0f),
                               Color{ 60, 55, 50, 220 }, 0.15f);
            }
            break;
        default:
            particles.EmitExplosion(pos, 50);
            break;
    }
}

void DrawAmmoProjectile(Vector2 pos, Vector2 vel, AmmoStyle ammo,
                        const Texture2D* sprite, bool powerupDouble, bool powerupGuided,
                        float visualScale) {
    const float r = cfg::PROJECTILE_RADIUS_PX * visualScale;
    const float t = static_cast<float>(GetTime());
    if (powerupDouble) {
        DrawCircleV(pos, r + 3.0f * visualScale, Fade(Color{ 255, 120, 30, 255 }, 0.45f));
        DrawCircleV(pos, r, Color{ 255, 90, 20, 255 });
        return;
    }
    if (powerupGuided) {
        DrawCircleV(pos, r + 3.0f * visualScale, Fade(Color{ 180, 100, 255, 255 }, 0.45f));
        DrawCircleV(pos, r, Color{ 150, 70, 220, 255 });
        return;
    }

    float rot = 0.0f;
    if (ammo == AmmoStyle::Shuriken) rot = t * 720.0f;
    else if (ammo == AmmoStyle::Rasengan) rot = t * 420.0f;
    else if (std::fabs(vel.x) + std::fabs(vel.y) > 1.0f) {
        rot = std::atan2(vel.y, vel.x) * RAD2DEG;
    }

    switch (ammo) {
        case AmmoStyle::Rasengan:
            DrawCircleV(pos, r + 5.0f * visualScale, Fade(Color{ 60, 150, 255, 255 }, 0.35f));
            DrawSpriteOrCircle(pos, sprite, r, rot, WHITE);
            DrawCircleLines(static_cast<int>(pos.x), static_cast<int>(pos.y),
                            r + 3.0f * visualScale + std::sin(t * 18.0f) * 1.5f * visualScale,
                            Color{ 180, 230, 255, 200 });
            break;
        case AmmoStyle::Chidori: {
            DrawCircleV(pos, r + 6.0f * visualScale, Fade(Color{ 80, 180, 255, 255 }, 0.35f));
            DrawSpriteOrCircle(pos, sprite, r, 0.0f, WHITE);
            const int bolts = 7;
            for (int i = 0; i < bolts; ++i) {
                const float flicker = std::floor(t * 18.0f);
                const float a = static_cast<float>(i) * (2.0f * PI / bolts) + flicker * 0.37f
                    + std::sin(t * 30.0f + static_cast<float>(i)) * 0.25f;
                const float len = (14.0f + std::fmod(flicker * 3.0f + static_cast<float>(i) * 5.0f, 10.0f))
                    * visualScale;
                DrawLightningBolt(pos, a, len, 3.2f * visualScale,
                                  Color{ 70, 160, 255, 230 }, Color{ 240, 252, 255, 255 });
            }
            break;
        }
        case AmmoStyle::SixSeven: {
            const float hs = r * 1.15f;
            for (int s = 0; s < 2; ++s) {
                const float side = (s == 0) ? -1.0f : 1.0f;
                const float phase = std::sin(t * 10.5f + static_cast<float>(s) * PI) * r * 0.85f;
                DrawTrendHand({ pos.x + side * r * 1.15f, pos.y + phase }, hs, side);
            }
            break;
        }
        case AmmoStyle::Nuclear:
            DrawCircleV(pos, r + 4.0f * visualScale, Fade(Color{ 255, 220, 40, 255 }, 0.3f));
            DrawSpriteOrCircle(pos, sprite, r + 1.0f * visualScale, rot, WHITE);
            break;
        default:
            DrawSpriteOrCircle(pos, sprite, r, rot, WHITE);
            break;
    }
}

void DrawAmmoShopPreview(Vector2 center, AmmoStyle ammo, const Texture2D* sprite) {
    const float t = static_cast<float>(GetTime());
    Vector2 vel{ std::cos(t) * 20.0f, std::sin(t) * 8.0f };
    DrawAmmoProjectile(center, vel, ammo, sprite, false, false, 4.6f);
}
