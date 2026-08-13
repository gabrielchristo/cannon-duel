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
    pinnedTooltipIndex_ = -1;
    pinnedTooltipTimer_ = 0.0f;
}

void PowerupSystem::TickSpawnCounter() {
    turnsSinceSpawnCheck_++;
}

void PowerupSystem::ResetSpawnCounter() {
    turnsSinceSpawnCheck_ = 0;
}

void PowerupSystem::SetSpawnEveryTurns(int turns) {
    spawnEveryTurns_ = std::max(1, turns);
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

bool PowerupSystem::CollectPowerupAt(Powerup& pu, Cannon& shooter, Lang lang,
                                     const std::function<void(int type, float x)>& onOnlinePickup) {
    if (!pu.active || shotPickedType_ >= 0) return false;

    const PowerupType type = pu.type;
    const float px = pu.x;
    pu.active = false;

    if (onOnlinePickup) {
        shotPickedType_ = static_cast<int>(type);
        shotPickedX_ = px;
        onOnlinePickup(shotPickedType_, shotPickedX_);
    }

    ApplyEffect(shooter, type, lang);

    active_.erase(std::remove_if(active_.begin(), active_.end(),
                                 [](const Powerup& p) { return !p.active; }),
                  active_.end());
    return true;
}

float PowerupSystem::MinClearanceFromCannons() const {
    return cfg::CANNON_BODY_RADIUS_PX + cfg::POWERUP_RADIUS_PX + cfg::POWERUP_SPAWN_CANNON_CLEARANCE_PX;
}

bool PowerupSystem::IsClearOfCannons(float x, const MatchRoster& roster) const {
    const float minDist = MinClearanceFromCannons();
    for (int i = 0; i < roster.CannonCount(); ++i) {
        if (std::fabs(x - roster.At(i).x) < minDist) return false;
    }
    return true;
}

float PowerupSystem::ResolveSpawnX(float preferredX, const MatchRoster& roster,
                                   const std::function<float(int attempt)>& candidateFn) const {
    constexpr float margin = 160.0f;
    const float minX = margin;
    const float maxX = cfg::SCREEN_WIDTH - margin;
    const float minDist = MinClearanceFromCannons();

    auto clampX = [&](float x) { return std::clamp(x, minX, maxX); };

    if (IsClearOfCannons(preferredX, roster)) return clampX(preferredX);

    int nearestSlot = 0;
    float nearestDx = 1e9f;
    for (int i = 0; i < roster.CannonCount(); ++i) {
        const float dx = std::fabs(preferredX - roster.At(i).x);
        if (dx < nearestDx) {
            nearestDx = dx;
            nearestSlot = i;
        }
    }
    const float cannonX = roster.At(nearestSlot).x;
    float shifted = (preferredX >= cannonX) ? (cannonX + minDist) : (cannonX - minDist);
    if (IsClearOfCannons(shifted, roster)) return clampX(shifted);
    shifted = (preferredX >= cannonX) ? (cannonX - minDist) : (cannonX + minDist);
    if (IsClearOfCannons(shifted, roster)) return clampX(shifted);

    for (int attempt = 0; attempt < 16; ++attempt) {
        const float x = candidateFn(attempt);
        if (IsClearOfCannons(x, roster)) return clampX(x);
    }

    float bestX = cfg::SCREEN_WIDTH * 0.5f;
    float bestScore = -1.0f;
    for (int sample = 0; sample < 32; ++sample) {
        const float x = minX + (maxX - minX) * static_cast<float>(sample) / 31.0f;
        float score = 1e9f;
        for (int i = 0; i < roster.CannonCount(); ++i) {
            score = std::min(score, std::fabs(x - roster.At(i).x));
        }
        if (score > bestScore) {
            bestScore = score;
            bestX = x;
        }
    }
    return clampX(bestX);
}

void PowerupSystem::MaybeSpawnRandom(const MatchRoster& roster) {
    if (static_cast<int>(active_.size()) >= cfg::POWERUP_MAX_ACTIVE) return;
    if (turnsSinceSpawnCheck_ < spawnEveryTurns_) return;
    turnsSinceSpawnCheck_ = 0;

    constexpr float margin = 160.0f;
    const float preferred = RandF(margin, cfg::SCREEN_WIDTH - margin);
    const float x = ResolveSpawnX(preferred, roster, [&](int attempt) {
        (void)attempt;
        return RandF(margin, cfg::SCREEN_WIDTH - margin);
    });
    PushSpawn(x, RollWeightedType(RandF(0.0f, 1.0f)));
}

void PowerupSystem::SpawnAt(float x, PowerupType type, const MatchRoster& roster) {
    if (static_cast<int>(active_.size()) >= cfg::POWERUP_MAX_ACTIVE) return;
    const float resolved = ResolveSpawnX(x, roster, [&](int attempt) {
        return x + static_cast<float>(attempt - 8) * (MinClearanceFromCannons() * 0.5f);
    });
    PushSpawn(resolved, type);
}

void PowerupSystem::MaybeSpawnSeeded(unsigned seed, int completedTurns, const MatchRoster& roster) {
    if (static_cast<int>(active_.size()) >= cfg::POWERUP_MAX_ACTIVE) return;

    const unsigned salt = static_cast<unsigned>(completedTurns * 104729u + 17u);
    constexpr float margin = 160.0f;
    const float span = cfg::SCREEN_WIDTH - 2.0f * margin;
    const float preferred = margin + SeededUnitFloat(seed, salt) * span;
    const float x = ResolveSpawnX(preferred, roster, [&](int attempt) {
        return margin + SeededUnitFloat(seed, salt + 2u + static_cast<unsigned>(attempt)) * span;
    });
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

void PowerupSystem::DrawTooltipForPowerup(const Powerup& pu, Vector2 anchor, Lang lang) const {
    const char* desc = PowerupDescription(pu.type, lang);
    int fs = 15;
    int tw = MeasureText(desc, fs);
    float boxW = tw + 16.0f, boxH = fs + 12.0f;

    float bx = anchor.x + 16.0f;
    float by = anchor.y - boxH - 10.0f;
    bx = std::clamp(bx, 4.0f, static_cast<float>(cfg::SCREEN_WIDTH) - boxW - 4.0f);
    by = std::clamp(by, 4.0f, static_cast<float>(cfg::SCREEN_HEIGHT) - boxH - 4.0f);

    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(boxW), static_cast<int>(boxH),
                  Fade(Color{20, 20, 20, 255}, 0.9f));
    DrawRectangleLines(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(boxW), static_cast<int>(boxH),
                       PowerupColor(pu.type));
    DrawText(desc, static_cast<int>(bx + 8), static_cast<int>(by + 6), fs, WHITE);
}

bool PowerupSystem::PointerOverPowerup(const Terrain& terrain, Vector2 point, int* outIndex) const {
    for (size_t i = 0; i < active_.size(); ++i) {
        const Powerup& pu = active_[i];
        if (!pu.active) continue;

        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };

        float dist = std::sqrt(std::pow(point.x - center.x, 2) + std::pow(point.y - center.y, 2));
        if (dist > cfg::POWERUP_RADIUS_PX + 10.0f) continue;

        if (outIndex) *outIndex = static_cast<int>(i);
        return true;
    }
    return false;
}

void PowerupSystem::DrawTooltip(const Terrain& terrain, Vector2 mouse, Lang lang) const {
    int hoverIndex = -1;
    if (PointerOverPowerup(terrain, mouse, &hoverIndex)) {
        DrawTooltipForPowerup(active_[static_cast<size_t>(hoverIndex)], mouse, lang);
        return;
    }

    if (pinnedTooltipIndex_ >= 0 &&
        pinnedTooltipIndex_ < static_cast<int>(active_.size()) &&
        active_[static_cast<size_t>(pinnedTooltipIndex_)].active) {
        const Powerup& pu = active_[static_cast<size_t>(pinnedTooltipIndex_)];
        float y = terrain.HeightAt(pu.x);
        float bob = std::sin(static_cast<float>(GetTime()) * 3.0f + pu.x) * 4.0f;
        Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f + bob };
        DrawTooltipForPowerup(pu, center, lang);
    }
}

bool PowerupSystem::ConsumesPointerPress(const Terrain& terrain, Vector2 point, Lang lang) {
    int hitIndex = -1;
    if (!PointerOverPowerup(terrain, point, &hitIndex)) return false;

    pinnedTooltipIndex_ = hitIndex;
    pinnedTooltipTimer_ = 2.5f;
    (void)lang;
    return true;
}

void PowerupSystem::UpdatePinnedTooltip(float dt) {
    if (pinnedTooltipTimer_ > 0.0f) {
        pinnedTooltipTimer_ = std::max(0.0f, pinnedTooltipTimer_ - dt);
        if (pinnedTooltipTimer_ <= 0.0f) {
            pinnedTooltipIndex_ = -1;
        }
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
            picker.shieldPickedThisTurn = true;
            break;
        default: break;
    }

    Vector2 base = { picker.x, picker.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f - 40.0f };
    ShowMessage(PowerupDescription(type, lang), base);
}

void PowerupSystem::TickTurnEffects(Cannon& finishingTurnCannon) {
    finishingTurnCannon.OnTurnEnded();
}

bool PowerupSystem::CheckProjectileCollision(Vector2 projFrom, Vector2 projTo, int currentPlayer,
                                             Cannon& player1, Cannon& player2, const Terrain& terrain,
                                             Lang lang,
                                             const std::function<void(int type, float x)>& onOnlinePickup) {
    Vector2 seg = { projTo.x - projFrom.x, projTo.y - projFrom.y };
    float segLenSq = seg.x * seg.x + seg.y * seg.y;
    float hitRadius = cfg::POWERUP_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX + cfg::POWERUP_HIT_TOLERANCE_PX;
    const float segLen = std::sqrt(segLenSq);
    Cannon& shooter = (currentPlayer == 1) ? player1 : player2;
    if (segLen < cfg::POWERUP_MIN_PICKUP_TRAVEL_PX) {
        return TryPickupAtImpact(projTo, shooter, terrain, lang, onOnlinePickup);
    }

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

        picked = CollectPowerupAt(pu, shooter, lang, onOnlinePickup);
        break;
    }

    if (!picked) {
        picked = TryPickupAtImpact(projTo, shooter, terrain, lang, onOnlinePickup);
    }
    return picked;
}

bool PowerupSystem::CheckProjectileCollisionRoster(Vector2 projFrom, Vector2 projTo, int currentPlayer,
                                                 MatchRoster& roster, const Terrain& terrain, Lang lang,
                                                 const std::function<void(int type, float x)>& onOnlinePickup) {
    Vector2 seg = { projTo.x - projFrom.x, projTo.y - projFrom.y };
    float segLenSq = seg.x * seg.x + seg.y * seg.y;
    float hitRadius = cfg::POWERUP_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX + cfg::POWERUP_HIT_TOLERANCE_PX;
    const float segLen = std::sqrt(segLenSq);
    Cannon& shooter = roster.AtPlayerNum(currentPlayer);
    if (segLen < cfg::POWERUP_MIN_PICKUP_TRAVEL_PX) {
        return TryPickupAtImpact(projTo, shooter, terrain, lang, onOnlinePickup);
    }

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

        picked = CollectPowerupAt(pu, shooter, lang, onOnlinePickup);
        break;
    }

    if (!picked) {
        picked = TryPickupAtImpact(projTo, shooter, terrain, lang, onOnlinePickup);
    }
    return picked;
}

bool PowerupSystem::TryPickupAtImpact(Vector2 impactPos, Cannon& shooter, const Terrain& terrain,
                                      Lang lang,
                                      const std::function<void(int type, float x)>& onOnlinePickup) {
    if (shotPickedType_ >= 0) return true;

    const float hitRadius = cfg::POWERUP_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX + cfg::POWERUP_HIT_TOLERANCE_PX;
    for (auto& pu : active_) {
        if (!pu.active) continue;

        const float y = terrain.HeightAt(pu.x);
        const Vector2 center = { pu.x, y - cfg::POWERUP_RADIUS_PX - 6.0f };
        const float dist = std::sqrt(std::pow(impactPos.x - center.x, 2) + std::pow(impactPos.y - center.y, 2));
        if (dist > hitRadius) continue;

        return CollectPowerupAt(pu, shooter, lang, onOnlinePickup);
    }
    return false;
}

bool PowerupSystem::ApplyRemotePickup(int type, float x, Cannon& shooter,
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
        ApplyEffect(shooter, puType, lang);
        remoteEffectApplied = true;
    }

    if (removed) {
        DebugLogf(LOG_INFO, "GAME: power-up remoto removido type=%d x=%.1f effect=%d",
                  type, x, applyEffect ? 1 : 0);
    }
    return removed;
}
