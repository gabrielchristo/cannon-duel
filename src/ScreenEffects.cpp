#include "ScreenEffects.h"

#include "Config.h"
#include "GameRand.h"

#include <algorithm>
#include <cmath>

void ScreenEffects::Init() {
    dust_.clear();
    dust_.reserve(cfg::DUST_MOTE_COUNT);
    for (int i = 0; i < cfg::DUST_MOTE_COUNT; ++i) {
        DustMote m;
        m.pos = { RandF(0.0f, cfg::SCREEN_WIDTH), RandF(0.0f, cfg::SCREEN_HEIGHT * 0.75f) };
        m.depth = RandF(0.15f, 1.0f);
        m.size = 1.0f + m.depth * 2.5f;
        m.alpha = 0.15f + m.depth * 0.35f;
        dust_.push_back(m);
    }
}

void ScreenEffects::Update(float dt, float windForce) {
    for (auto& m : dust_) {
        float speed = cfg::DUST_BASE_DRIFT_SPEED_PX + windForce * cfg::DUST_WIND_SPEED_SCALE_PX;
        m.pos.x += speed * (0.4f + m.depth * 0.6f) * dt;
        m.pos.y += std::sin(static_cast<float>(GetTime()) * 0.6f + m.pos.x * 0.01f) * 4.0f * dt;

        if (m.pos.x > cfg::SCREEN_WIDTH + 5.0f) m.pos.x = -5.0f;
        if (m.pos.x < -5.0f) m.pos.x = cfg::SCREEN_WIDTH + 5.0f;
    }
}

void ScreenEffects::Draw() const {
    for (const auto& m : dust_) {
        Color c = { 255, 250, 235, static_cast<unsigned char>(m.alpha * 255) };
        DrawCircleV(m.pos, m.size, c);
    }
}

void ScreenEffects::TriggerShake(float magnitudePx, float durationSec) {
    shakeMagnitude_ = magnitudePx;
    shakeDuration_ = durationSec;
    shakeTimer_ = durationSec;
}

void ScreenEffects::UpdateShake(float dt) {
    if (shakeTimer_ > 0.0f) {
        shakeTimer_ = std::max(0.0f, shakeTimer_ - dt);
    }
}

Vector2 ScreenEffects::ShakeOffset() const {
    if (shakeTimer_ <= 0.0f) return { 0.0f, 0.0f };
    float ratio = shakeTimer_ / shakeDuration_;
    float mag = shakeMagnitude_ * ratio;
    return { RandF(-mag, mag), RandF(-mag, mag) };
}
