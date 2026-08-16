#include "CoinPopup.h"

#include <algorithm>
#include <cstdio>

void CoinPopupSystem::Spawn(Vector2 pos, int amount) {
    popups_.push_back({ pos, amount, 0.0f });
}

void CoinPopupSystem::Update(float dt) {
    for (Popup& p : popups_) {
        p.age += dt;
        const float lifeT = p.age / kLifetime;
        // sobe devagar a maior parte da vida, acelera nos últimos instantes
        const float riseSpeed = (lifeT < kFastPhaseStart) ? 22.0f : 90.0f;
        p.pos.y -= riseSpeed * dt;
    }
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(),
        [](const Popup& p) { return p.age >= kLifetime; }), popups_.end());
}

void CoinPopupSystem::Draw() const {
    for (const Popup& p : popups_) {
        const float lifeT = p.age / kLifetime;
        float alpha = 1.0f;
        if (lifeT > kFastPhaseStart) {
            // fade rápido na reta final
            alpha = 1.0f - (lifeT - kFastPhaseStart) / (1.0f - kFastPhaseStart);
        }
        alpha = std::max(0.0f, std::min(1.0f, alpha));

        char buf[16];
        std::snprintf(buf, sizeof(buf), "+%d", p.amount);
        const int fs = 20;
        int tw = MeasureText(buf, fs);
        int tx = static_cast<int>(p.pos.x) - tw / 2;
        int ty = static_cast<int>(p.pos.y);

        Color gold = { 255, 210, 60, static_cast<unsigned char>(alpha * 255) };
        Color outline = { 60, 40, 0, static_cast<unsigned char>(alpha * 200) };
        DrawText(buf, tx - 1, ty - 1, fs, outline);
        DrawText(buf, tx + 1, ty - 1, fs, outline);
        DrawText(buf, tx - 1, ty + 1, fs, outline);
        DrawText(buf, tx + 1, ty + 1, fs, outline);
        DrawText(buf, tx, ty, fs, gold);
    }
}
