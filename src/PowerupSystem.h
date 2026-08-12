#pragma once

#include "Config.h"
#include "Powerup.h"
#include "Localization.h"
#include "Terrain.h"
#include "Cannon.h"

#include <raylib.h>
#include <functional>
#include <vector>

// Spawn, colisão, efeitos e UI de power-ups (versão Plus).
class PowerupSystem {
public:
    void Reset();

    void TickSpawnCounter();
    void ResetSpawnCounter();

    void MaybeSpawnRandom();
    void MaybeSpawnSeeded(unsigned seed, int completedTurns);

    void Draw(const Terrain& terrain) const;
    void DrawTooltip(const Terrain& terrain, Vector2 mouse, Lang lang) const;

    void ShowMessage(const char* text, Vector2 pos);
    void UpdateMessageTimer(float dt);
    void DrawMessage() const;

    void ApplyEffect(Cannon& picker, PowerupType type, Lang lang);
    void TickTurnEffects(Cannon& startingTurnCannon);

    // Retorna true se coletou um power-up neste segmento de trajetória.
    bool CheckProjectileCollision(Vector2 projFrom, Vector2 projTo, int currentPlayer,
                                  Cannon& player1, Cannon& player2, const Terrain& terrain,
                                  Lang lang,
                                  const std::function<void(int type, float x)>& onOnlinePickup);

    bool ApplyRemotePickup(int type, float x, int shooterPlayer,
                           Cannon& player1, Cannon& player2,
                           Lang lang, bool applyEffect, bool& remoteEffectApplied);

    const std::vector<Powerup>& Active() const { return active_; }

    int& ShotPickedType() { return shotPickedType_; }
    float& ShotPickedX() { return shotPickedX_; }
    bool& RemoteEffectApplied() { return remoteEffectApplied_; }

    int TurnsSinceSpawnCheck() const { return turnsSinceSpawnCheck_; }
    void ForceSpawnReady() { turnsSinceSpawnCheck_ = cfg::POWERUP_SPAWN_EVERY_TURNS; }

private:
    std::vector<Powerup> active_;
    int turnsSinceSpawnCheck_ = 0;
    int shotPickedType_ = -1;
    float shotPickedX_ = 0.0f;
    bool remoteEffectApplied_ = false;

    const char* messageText_ = nullptr;
    float messageTimer_ = 0.0f;
    Vector2 messagePos_{};

    PowerupType RollWeightedType(float roll01) const;
    void PushSpawn(float x, PowerupType type);
};
