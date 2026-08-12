#include "PowerupSystem.h"

#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"

#include <algorithm>
#include <cmath>

void PowerupSystem::Reset() {
    active_.clear();
    turnsSinceSpawnCheck_ = 0;
    shotPickedType_ = -1;
    shotPickedX_ = 0.0f;
    remoteEffectApplied_ = false;
    messageTimer_ = 0.0f;
    messageText_ = nullptr;
}

void PowerupSystem::TickSpawnCounter() {
    turnsSinceSpawnCheck_++;
}

void PowerupSystem::ResetSpawnCounter() {
    turnsSinceSpawnCheck_ = 0;
}

PowerupType PowerupSystem::RollWeightedType(float roll01) const {
    struct Entry { PowerupType type; float weight; };
    const Entry entries[] = {
        { PowerupType::DoubleDamage,      cfg::POWERUP_WEIGHT_DOUBLE_DMG },
        { PowerupType::TrajectoryPreview, cfg::POWERUP_WEIGHT_TRAJECTORY },
        { PowerupType::Guided,            cfg::POWERUP_WEIGHT_GUIDED },
        { PowerupType::Heal,              cfg::POWERUP_WEIGHT_HEAL },
        { PowerupType::Shield,            cfg::POWERUP_WEIGHT_SHIELD },
    };
    float totalWeight = 0.0f;
    for (const auto& e : entries) totalWeight += e.weight;
    float roll = roll01 * totalWeight;
    for (const auto& e : entries) {
        if (roll < e.weight) return e.type;
        roll -= e.weight;
    }
    return entries[0].type;
}

void PowerupSystem::PushSpawn(float x, PowerupType type) {
    Powerup p;
    p.active = true;
    p.type = type;
    p.x = x;
    active_.push_back(p);
}

void PowerupSystem::MaybeSpawnRandom() {
    if (static_cast<int>(active_.size()) >= cfg::POWERUP_MAX_ACTIVE) return;
    if (turnsSinceSpawnCheck_ < cfg::POWERUP_SPAWN_EVERY_TURNS) return;
    turnsSinceSpawnCheck_ = 0;

    float margin = 160.0f;
    float x = RandF(margin, cfg::SCREEN_WIDTH - margin);
    PushSpawn(x, RollWeightedType(RandF(0.0f, 1.0f)));
}

void PowerupSystem::MaybeSpawnSeeded(unsigned seed, int completedTurns) {
    if (static_cast<int>(active_.size()) >= cfg::POWERUP_MAX_ACTIVE) return;

    unsigned salt = static_cast<unsigned>(completedTurns * 104729u + 17u);
    float margin = 160.0f;
    float x = margin + SeededUnitFloat(seed, salt) * (cfg::SCREEN_WIDTH - 2.0f * margin);
    PushSpawn(x, RollWeightedType(SeededUnitFloat(seed, salt + 1u)));
}

void PowerupSystem::Draw(const Terrain& terrain) const {
    for (const auto& pu : active_) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };

        Color c = PowerupColor(pu.type);
        DrawCircleV(center, cfg::POWERUP_RADIUS_PX, c);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        static_cast<int>(cfg::POWERUP_RADIUS_PX), Color{20, 20, 20, 255});
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
                        static_cast<int>(cfg::POWERUP_RADIUS_PX) + 3, Fade(c, 0.4f));

        const char* label = PowerupLabel(pu.type);
        int fs = 14;
        int tw = MeasureText(label, fs);
        DrawText(label, static_cast<int>(center.x - tw / 2), static_cast<int>(center.y - fs / 2), fs, WHITE);
    }
}

void PowerupSystem::DrawTooltip(const Terrain& terrain, Vector2 mouse, Lang lang) const {
    for (const auto& pu : active_) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };

        float dist = std::sqrt(std::pow(mouse.x - center.x, 2) + std::pow(mouse.y - center.y, 2));
        if (dist > cfg::POWERUP_RADIUS_PX + 10.0f) continue;

        const char* desc = PowerupDescription(pu.type, lang);
        int fs = 15;
        int tw = MeasureText(desc, fs);
        float boxW = tw + 16.0f, boxH = fs + 12.0f;

        float bx = mouse.x + 16.0f;
        float by = mouse.y - boxH - 10.0f;
        bx = std::clamp(bx, 4.0f, static_cast<float>(cfg::SCREEN_WIDTH) - boxW - 4.0f);
        by = std::clamp(by, 4.0f, static_cast<float>(cfg::SCREEN_HEIGHT) - boxH - 4.0f);

        DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(boxW), static_cast<int>(boxH),
                      Fade(Color{20, 20, 20, 255}, 0.9f));
        DrawRectangleLines(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(boxW), static_cast<int>(boxH),
                           PowerupColor(pu.type));
        DrawText(desc, static_cast<int>(bx + 8), static_cast<int>(by + 6), fs, WHITE);
        break;
    }
}

void PowerupSystem::ShowMessage(const char* text, Vector2 pos) {
    messageText_ = text;
    messagePos_ = pos;
    messageTimer_ = cfg::POWERUP_MESSAGE_DURATION_SEC;
}

void PowerupSystem::UpdateMessageTimer(float dt) {
    if (messageTimer_ > 0.0f) {
        messageTimer_ = std::max(0.0f, messageTimer_ - dt);
    }
}

void PowerupSystem::DrawMessage() const {
    if (messageTimer_ <= 0.0f || !messageText_) return;

    float alpha = std::min(1.0f, messageTimer_ / 0.4f);
    int fs = 18;
    int tw = MeasureText(messageText_, fs);

    float boxPad = 6.0f;
    float boxW = tw + boxPad * 2.0f;
    float px = messagePos_.x - tw / 2.0f;

    float minX = boxPad;
    float maxX = static_cast<float>(cfg::SCREEN_WIDTH) - boxW + boxPad;
    px = std::clamp(px, minX, maxX);

    float py = std::clamp(messagePos_.y, 4.0f, static_cast<float>(cfg::SCREEN_HEIGHT) - fs - 8.0f);

    DrawRectangle(static_cast<int>(px) - static_cast<int>(boxPad), static_cast<int>(py) - 4,
                  static_cast<int>(boxW), fs + 8, Fade(Color{20, 20, 20, 255}, alpha * 0.75f));
    DrawText(messageText_, static_cast<int>(px), static_cast<int>(py), fs,
             Fade(Color{255, 230, 140, 255}, alpha));
}

void PowerupSystem::ApplyEffect(Cannon& picker, PowerupType type, Lang lang) {
    switch (type) {
        case PowerupType::DoubleDamage:
            picker.queuedDoubleDamage = true;
            break;
        case PowerupType::TrajectoryPreview:
            picker.queuedTrajectoryPreviewTurns = cfg::POWERUP_TRAJECTORY_TURNS;
            break;
        case PowerupType::Guided:
            picker.pendingGuided = true;
            break;
        case PowerupType::Heal: {
            float amount = RandF(cfg::POWERUP_HEAL_MIN_RATIO, cfg::POWERUP_HEAL_MAX_RATIO) * cfg::CANNON_MAX_HEALTH;
            picker.health = std::min(cfg::CANNON_MAX_HEALTH, picker.health + amount);
            break;
        }
        case PowerupType::Shield:
            picker.shieldTurnsLeft = cfg::POWERUP_SHIELD_TURNS;
            break;
        default: break;
    }

    Vector2 base = { picker.x, picker.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f - 40.0f };
    ShowMessage(PowerupDescription(type, lang), base);
}

void PowerupSystem::TickTurnEffects(Cannon& startingTurnCannon) {
    if (startingTurnCannon.shieldTurnsLeft > 0) startingTurnCannon.shieldTurnsLeft--;
}

bool PowerupSystem::CheckProjectileCollision(Vector2 projFrom, Vector2 projTo, int currentPlayer,
                                             Cannon& player1, Cannon& player2, const Terrain& terrain,
                                             Lang lang,
                                             const std::function<void(int type, float x)>& onOnlinePickup) {
    Vector2 seg = { projTo.x - projFrom.x, projTo.y - projFrom.y };
    float segLenSq = seg.x * seg.x + seg.y * seg.y;
    float hitRadius = cfg::POWERUP_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX + cfg::POWERUP_HIT_TOLERANCE_PX;

    bool picked = false;
    for (auto& pu : active_) {
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f };

        float t = 0.0f;
        if (segLenSq > 0.0001f) {
            t = ((center.x - projFrom.x) * seg.x + (center.y - projFrom.y) * seg.y) / segLenSq;
            t = std::clamp(t, 0.0f, 1.0f);
        }
        Vector2 closest = { projFrom.x + seg.x * t, projFrom.y + seg.y * t };
        float dist = std::sqrt(std::pow(closest.x - center.x, 2) + std::pow(closest.y - center.y, 2));

        if (dist > hitRadius) continue;

        Cannon& shooter = (currentPlayer == 1) ? player1 : player2;
        PowerupType type = pu.type;
        float px = pu.x;
        pu.active = false;
        picked = true;

        if (onOnlinePickup) {
            shotPickedType_ = static_cast<int>(type);
            shotPickedX_ = px;
            onOnlinePickup(shotPickedType_, shotPickedX_);
        }

        ApplyEffect(shooter, type, lang);
        break;
    }

    active_.erase(std::remove_if(active_.begin(), active_.end(),
                                 [](const Powerup& p) { return !p.active; }),
                  active_.end());
    return picked;
}

bool PowerupSystem::ApplyRemotePickup(int type, float x, int shooterPlayer,
                                      Cannon& player1, Cannon& player2,
                                      Lang lang, bool applyEffect, bool& remoteEffectApplied) {
    if (type < 0 || type >= static_cast<int>(PowerupType::COUNT)) return false;

    const PowerupType puType = static_cast<PowerupType>(type);
    bool removed = false;
    for (auto& pu : active_) {
        if (!pu.active) continue;
        if (pu.type == puType && std::fabs(pu.x - x) < 8.0f) {
            pu.active = false;
            removed = true;
            break;
        }
    }
    if (!removed) {
        int best = -1;
        float bestDist = 1e9f;
        for (int i = 0; i < static_cast<int>(active_.size()); ++i) {
            if (!active_[i].active) continue;
            if (active_[i].type != puType) continue;
            float d = std::fabs(active_[i].x - x);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best >= 0) {
            active_[best].active = false;
            removed = true;
        }
    }

    active_.erase(std::remove_if(active_.begin(), active_.end(),
                                 [](const Powerup& p) { return !p.active; }),
                  active_.end());

    if (applyEffect && !remoteEffectApplied) {
        Cannon& shooter = (shooterPlayer == 1) ? player1 : player2;
        ApplyEffect(shooter, puType, lang);
        remoteEffectApplied = true;
    }

    if (removed) {
        DebugLogf(LOG_INFO, "GAME: power-up remoto removido type=%d x=%.1f effect=%d",
                  type, x, applyEffect ? 1 : 0);
    }
    return removed;
}
