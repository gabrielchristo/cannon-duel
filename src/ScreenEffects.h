#pragma once

#include <raylib.h>
#include <vector>

// Poeira ambiente + screen shake (versão Plus).
class ScreenEffects {
public:
    void Init();
    void Update(float dt, float windForce);
    void Draw() const;

    void TriggerShake(float magnitudePx, float durationSec);
    void UpdateShake(float dt);
    Vector2 ShakeOffset() const;

private:
    struct DustMote {
        Vector2 pos;
        float depth = 0.5f;
        float size = 1.0f;
        float alpha = 0.3f;
    };

    std::vector<DustMote> dust_;
    float shakeTimer_ = 0.0f;
    float shakeDuration_ = 1.0f;
    float shakeMagnitude_ = 0.0f;
};
