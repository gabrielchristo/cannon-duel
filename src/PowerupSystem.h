#pragma once

#include "Config.h"
#include "Powerup.h"
#include "Localization.h"
#include "MatchRoster.h"
#include "Terrain.h"
#include "Cannon.h"

#include <raylib.h>
#include <functional>
#include <vector>

struct ShotPowerupPickup {
    int type = -1;
    float x = 0.0f;
};

// Spawn, colisão, efeitos e UI de power-ups (versão Plus).
class PowerupSystem {
public:
    void Reset();

    void TickSpawnCounter();
    void ResetSpawnCounter();
    void SetSpawnEveryTurns(int turns);

    void MaybeSpawnRandom(const MatchRoster& roster);
    void MaybeSpawnSeeded(unsigned seed, int completedTurns, const MatchRoster& roster,
                          int* outType = nullptr, float* outX = nullptr,
                          bool forceOnlineSpawn = false);

    void Draw(const Terrain& terrain) const;
    void DrawTooltip(const Terrain& terrain, Vector2 mouse, Lang lang) const;
    bool ConsumesPointerPress(const Terrain& terrain, Vector2 point, Lang lang);
    void UpdatePinnedTooltip(float dt);

    void ShowMessage(const char* text, Vector2 pos);
    void UpdateMessageTimer(float dt);
    void DrawMessage() const;

    void ApplyEffect(Cannon& picker, PowerupType type, Lang lang);
    void TickTurnEffects(Cannon& finishingTurnCannon);

    // Retorna true se coletou um power-up neste segmento de trajetória.
    bool CheckProjectileCollision(Vector2 projFrom, Vector2 projTo, int currentPlayer,
                                  Cannon& player1, Cannon& player2, const Terrain& terrain,
                                  Lang lang,
                                  const std::function<void(int type, float x)>& onOnlinePickup);

    bool CheckProjectileCollisionRoster(Vector2 projFrom, Vector2 projTo, int currentPlayer,
                                        MatchRoster& roster, const Terrain& terrain, Lang lang,
                                        const std::function<void(int type, float x)>& onOnlinePickup);

    // Coleta por impacto no solo (último frame da trajetória costuma ser curto demais).
    bool TryPickupAtImpact(Vector2 impactPos, Cannon& shooter, const Terrain& terrain, Lang lang,
                            const std::function<void(int type, float x)>& onOnlinePickup);

    bool ApplyRemotePickup(int type, float x, Cannon& shooter,
                           Lang lang, bool applyEffect, bool& remoteEffectApplied);

    const std::vector<Powerup>& Active() const { return active_; }

    int& ShotPickedType() { return shotPickedType_; }
    float& ShotPickedX() { return shotPickedX_; }
    const std::vector<ShotPowerupPickup>& ShotPickups() const { return shotPickups_; }
    void ClearShotPickups();
    void ResetRemotePickupShot() { remotePickupsAppliedThisShot_ = false; }
    bool RemotePickupsAppliedThisShot() const { return remotePickupsAppliedThisShot_; }
    bool& RemoteEffectApplied() { return remoteEffectApplied_; }

    int TurnsSinceSpawnCheck() const { return turnsSinceSpawnCheck_; }
    void ForceSpawnReady() { turnsSinceSpawnCheck_ = spawnEveryTurns_; }
    void SpawnAt(float x, PowerupType type, const MatchRoster& roster);
    void SpawnAtExact(float x, PowerupType type);

private:
    std::vector<Powerup> active_;
    int turnsSinceSpawnCheck_ = 0;
    int spawnEveryTurns_ = cfg::POWERUP_SPAWN_EVERY_TURNS;
    int shotPickedType_ = -1;
    float shotPickedX_ = 0.0f;
    std::vector<ShotPowerupPickup> shotPickups_;
    bool remotePickupsAppliedThisShot_ = false;
    bool remoteEffectApplied_ = false;

    const char* messageText_ = nullptr;
    float messageTimer_ = 0.0f;
    Vector2 messagePos_{};

    int pinnedTooltipIndex_ = -1;
    float pinnedTooltipTimer_ = 0.0f;

    PowerupType RollWeightedType(float roll01) const;
    float MinClearanceFromCannons() const;
    bool IsClearOfCannons(float x, const MatchRoster& roster) const;
    float ResolveSpawnX(float preferredX, const MatchRoster& roster,
                        const std::function<float(int attempt)>& candidateFn) const;
    void PushSpawn(float x, PowerupType type);
    int CountActivePowerups() const;
    void EvictLowestXActive();
    bool CollectPowerupAt(Powerup& pu, Cannon& shooter, Lang lang,
                            const std::function<void(int type, float x)>& onOnlinePickup);
    void CompactInactive();
    bool PointerOverPowerup(const Terrain& terrain, Vector2 point, int* outIndex) const;
    void DrawTooltipForPowerup(const Powerup& pu, Vector2 anchor, Lang lang) const;
};
