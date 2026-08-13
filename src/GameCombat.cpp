#include "Game.h"
#include "AssetPath.h"
#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"
#include "MatchRoster.h"
#include "VirtualScreen.h"
#include "net/NetWorker.h"
#include "net/SupabaseClient.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace {

bool FriendlyFireEnabled(MatchFormat format, GameVersion ver) {
    return IsTeamMode(format) && ver == GameVersion::Plus;
}

} // namespace

bool Game::IsLocalHumanTurn() const {
    if (state != GameState::Aiming) return false;
    if (mode == GameMode::Online) {
        if (!netMatch.IsMyTurn()) return false;
        if (currentPlayer != netMatch.MyPlayerNumber()) return false;
    }
    return roster.IsHumanSlot(ActiveSlot(), mode);
}

void Game::UpdateAiming() {
    if (mode == GameMode::Online && !netMatch.IsMyTurn()) {
        return;
    }

    const int activeSlot = (mode == GameMode::Online) ? (netMatch.MyPlayerNumber() - 1) : ActiveSlot();
    Cannon& active = roster.At(activeSlot);

    const bool isAITurn = roster.IsAISlot(ActiveSlot(), mode);

    if (isAITurn) {
        const int targetSlot = roster.LowestHpEnemySlot(ActiveSlot());
        Cannon& primaryEnemy = roster.At(targetSlot);
        float targetX = primaryEnemy.x;
        float targetY = primaryEnemy.groundY;

        if (version == GameVersion::Plus && !powerups.Active().empty()) {
            float healthRatio = active.health / cfg::CANNON_MAX_HEALTH;
            int chosenIdx = -1;
            float bestPriority = 0.0f;

            for (size_t i = 0; i < powerups.Active().size(); ++i) {
                const Powerup& pu = powerups.Active()[i];
                float priority = 0.25f;

                if (healthRatio < 0.45f &&
                    (pu.type == PowerupType::Heal || pu.type == PowerupType::Shield)) {
                    priority = 0.85f;
                } else if (pu.type == PowerupType::DoubleDamage || pu.type == PowerupType::Guided) {
                    priority = 0.4f;
                }

                if (priority > bestPriority) {
                    bestPriority = priority;
                    chosenIdx = static_cast<int>(i);
                }
            }

            if (chosenIdx >= 0 && RandF(0.0f, 1.0f) < bestPriority) {
                targetX = powerups.Active()[static_cast<size_t>(chosenIdx)].x;
                targetY = terrain.HeightAt(targetX);
            }
        }

        ai.ComputeShot(active, targetX, targetY, windForce);

        Vector2 muzzle = active.MuzzlePosition();
        Vector2 dir = (version == GameVersion::Plus && active.pendingGuided)
            ? active.DirectionAtAngle(std::max(active.angleDeg, cfg::POWERUP_GUIDED_MIN_ANGLE_DEG))
            : active.AimDirection();
        projectile.Spawn(physics.Id(), muzzle, dir, active.power01);
        prevProjectilePos = muzzle;
        if (version == GameVersion::Plus && active.pendingGuided) {
            guidedTargetPlayerNum = targetSlot + 1;
            BeginGuidedFlight(muzzle, roster.At(targetSlot));
            projectile.SetKinematicPositionPx(muzzle);
        }
        if (version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0) {
            active.OnShotFired();
        }
        if (audioReady) PlaySound(sndFire);
        aimPhase = AimPhase::Angle;
        state = GameState::ProjectileFlying;
        return;
    }

    aimOscTimer += GetFrameTime();

#if CANNON_DUEL_ANDROID_BUILD
    bool confirmPressed = false;
    bool resetPressed = false;
#else
    bool confirmPressed = IsKeyPressed(KEY_SPACE);
    bool resetPressed = IsKeyPressed(KEY_B);
#endif

    if (aimPhase == AimPhase::Angle) {
        float period = cfg::ANGLE_OSC_PERIOD_SEC *
            ((version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0)
                ? cfg::POWERUP_TRAJECTORY_AIM_SLOWDOWN : 1.0f);
        float t = fmodf(aimOscTimer, period) / period;
        float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
        float angle = -90.0f + tri * 180.0f;
        active.SetAim(angle, active.power01);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || confirmPressed) {
            aimPhase = AimPhase::Power;
            aimOscTimer = 0.0f;
        }
        if (resetPressed) {
            aimOscTimer = 0.0f;
        }
    } else {
        float period = cfg::POWER_OSC_PERIOD_SEC *
            ((version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0)
                ? cfg::POWERUP_TRAJECTORY_AIM_SLOWDOWN : 1.0f);
        float t = fmodf(aimOscTimer, period) / period;
        float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
        active.SetAim(active.angleDeg, tri);

        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || resetPressed) {
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            return;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || confirmPressed) {
            Vector2 muzzle = active.MuzzlePosition();
            Vector2 dir = (version == GameVersion::Plus && active.pendingGuided)
                ? active.DirectionAtAngle(std::max(active.angleDeg, cfg::POWERUP_GUIDED_MIN_ANGLE_DEG))
                : active.AimDirection();
            projectile.Spawn(physics.Id(), muzzle, dir, active.power01);
            prevProjectilePos = muzzle;
            if (version == GameVersion::Plus && active.pendingGuided) {
                const int targetSlot = roster.LowestHpEnemySlot(ActiveSlot());
                guidedTargetPlayerNum = targetSlot + 1;
                BeginGuidedFlight(muzzle, roster.At(targetSlot));
                projectile.SetKinematicPositionPx(muzzle);
            }
            if (version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0) {
                active.trajectoryPreviewTurnsLeft--;
            }
            if (audioReady) PlaySound(sndFire);
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            state = GameState::ProjectileFlying;
            powerups.ShotPickedType() = -1;
            powerups.ShotPickedX() = 0.0f;
            if (mode == GameMode::Online && netMatch.InMatch()) {
                Vector2 muzz = active.MuzzlePosition();
                netMatch.PublishShotFired(active.angleDeg, active.power01, windForce, muzz.x, muzz.y);
            }
        }
    }

    if (mode == GameMode::Online && netMatch.InMatch()) {
        const char* phaseStr = (aimPhase == AimPhase::Angle) ? "angle" : "power";
        netMatch.PublishLiveAim(GetFrameTime(), active.angleDeg, active.power01, phaseStr);
    }
}

void Game::BeginGuidedFlight(Vector2 muzzle, const Cannon& target) {
    guidedPathT = 0.0f;
    guidedPathStart = muzzle;
    const float groundAtOpp = terrain.HeightAt(target.x);
    float apexY = std::min(cfg::POWERUP_GUIDED_APEX_Y_PX,
                             groundAtOpp - cfg::POWERUP_GUIDED_APEX_CLEARANCE_PX);
    apexY = std::max(24.0f, apexY);
    guidedPathApex = { target.x, apexY };
    guidedPathTarget = { target.x, target.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
}

Vector2 Game::SampleGuidedPath(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    const float split = cfg::POWERUP_GUIDED_ASCENT_FRAC;
    if (t <= split) {
        float u = (split > 0.0f) ? (t / split) : 1.0f;
        u = u * u * (3.0f - 2.0f * u);
        return {
            guidedPathStart.x + (guidedPathApex.x - guidedPathStart.x) * u,
            guidedPathStart.y + (guidedPathApex.y - guidedPathStart.y) * u
        };
    }
    const float diveSpan = std::max(0.0001f, 1.0f - split);
    float u = (t - split) / diveSpan;
    return {
        guidedPathApex.x + (guidedPathTarget.x - guidedPathApex.x) * u,
        guidedPathApex.y + (guidedPathTarget.y - guidedPathApex.y) * u
    };
}

void Game::UpdateProjectileFlight(float dt) {
    Cannon& shooter = GetCannon(currentPlayer);
    const bool guidedActive = (version == GameVersion::Plus && shooter.pendingGuided);
    const bool friendlyFire = FriendlyFireEnabled(matchFormat, version);

    if (guidedActive) {
        if (!projectile.IsActive()) return;

        guidedPathT += dt / cfg::POWERUP_GUIDED_FLIGHT_SEC;
        if (guidedPathT > 1.0f) guidedPathT = 1.0f;

        Vector2 pos = SampleGuidedPath(guidedPathT);
        Vector2 prev = projectile.PositionPx();
        projectile.SetKinematicPositionPx(pos);

        if (mode == GameMode::Online && netMatch.InMatch()) {
            netMatch.PublishProjectileSample(dt, pos.x, pos.y);
        }

        Vector2 vel = { pos.x - prev.x, pos.y - prev.y };
        particles.EmitTrail(pos, vel, Color{180, 120, 230, 255});

        if (version == GameVersion::Plus) {
            powerups.CheckProjectileCollisionRoster(prevProjectilePos, pos, currentPlayer,
                                                    roster, terrain, language,
                                                    [this](int type, float x) {
                                                        if (mode == GameMode::Online && netMatch.InMatch()) {
                                                            netMatch.PublishPowerupPicked(type, x);
                                                        }
                                                    });
        }
        prevProjectilePos = pos;

        if (guidedPathT >= 1.0f) {
            Cannon& target = GetCannon(guidedTargetPlayerNum);
            ResolveImpact(guidedPathTarget, true, &target);
        }
        return;
    }

    projectile.ApplyWind(windForce);
    physics.Step(dt);

    if (!projectile.IsActive()) return;

    Vector2 pos = projectile.PositionPx();

    if (mode == GameMode::Online && netMatch.InMatch()) {
        netMatch.PublishProjectileSample(dt, pos.x, pos.y);
    }

    Color trailColor = (version == GameVersion::Plus && shooter.pendingDoubleDamage)
        ? Color{255, 130, 40, 255}
        : Color{235, 230, 215, 255};
    particles.EmitTrail(pos, projectile.VelocityPx(), trailColor);

    if (version == GameVersion::Plus) {
        powerups.CheckProjectileCollisionRoster(prevProjectilePos, pos, currentPlayer,
                                              roster, terrain, language,
                                              [this](int type, float x) {
                                                  if (mode == GameMode::Online && netMatch.InMatch()) {
                                                      netMatch.PublishPowerupPicked(type, x);
                                                  }
                                              });
    }
    prevProjectilePos = pos;

    if (pos.x < -50 || pos.x > cfg::SCREEN_WIDTH + 50 || pos.y > cfg::SCREEN_HEIGHT + 200) {
        ResolveImpact(pos, false, nullptr);
        return;
    }

    const int shooterSlot = ActiveSlot();
    for (int i = 0; i < roster.CannonCount(); ++i) {
        if (i == shooterSlot) continue;
        if (roster.AreAllies(shooterSlot, i) && !friendlyFire) continue;

        Cannon& target = roster.At(i);
        Vector2 targetBase = { target.x, target.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
        float distToTarget = std::sqrt(std::pow(pos.x - targetBase.x, 2) + std::pow(pos.y - targetBase.y, 2));
        if (distToTarget <= cfg::CANNON_BODY_RADIUS_PX + cfg::PROJECTILE_RADIUS_PX) {
            ResolveImpact(pos, true, &target);
            return;
        }
    }

    if (terrain.IsPointInside(pos.x, pos.y)) {
        ResolveImpact(pos, false, nullptr);
        return;
    }
}

void Game::ResolveImpact(Vector2 impactPos, bool hitCannon, Cannon* hitTarget) {
    projectile.Destroy();
    if (audioReady) PlaySound(sndExplosion);

    Cannon& shooter = GetCannon(currentPlayer);
    float damageMult = 1.0f;
    float radiusMult = 1.0f;

    if (version == GameVersion::Plus) {
        if (shooter.pendingDoubleDamage) { damageMult *= cfg::POWERUP_DOUBLE_DAMAGE_MULT; }
        if (shooter.pendingGuided) { damageMult *= cfg::POWERUP_GUIDED_DAMAGE_MULT; }
        shooter.OnShotResolved();
    }

    float craterRadius = cfg::CRATER_RADIUS_PX * radiusMult;
    float explosionRadius = cfg::EXPLOSION_RADIUS_PX * radiusMult;

    particles.EmitExplosion(impactPos, 50);
    terrain.Explode(impactPos.x, impactPos.y, craterRadius);

    if (version == GameVersion::Plus) {
        effects.TriggerShake(hitCannon ? cfg::SHAKE_MAGNITUDE_DIRECT_PX : cfg::SHAKE_MAGNITUDE_TERRAIN_PX,
                             hitCannon ? cfg::SHAKE_DURATION_DIRECT_SEC : cfg::SHAKE_DURATION_TERRAIN_SEC);
    }

    const int shooterSlot = ActiveSlot();
    const bool friendlyFire = FriendlyFireEnabled(matchFormat, version);
    float dmgAppliedP1 = 0.0f, dmgAppliedP2 = 0.0f;

    for (int i = 0; i < roster.CannonCount(); ++i) {
        if (roster.AreAllies(shooterSlot, i) && !friendlyFire) continue;

        Cannon& c = roster.At(i);
        Vector2 base = { c.x, c.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
        float dist = std::sqrt(std::pow(impactPos.x - base.x, 2) + std::pow(impactPos.y - base.y, 2));
        if (dist > explosionRadius) continue;

        float falloff = 1.0f - (dist / explosionRadius);
        float dmg = cfg::EXPLOSION_DAMAGE_MAX * falloff * damageMult;
        if (hitCannon && hitTarget && &c == hitTarget) {
            dmg = cfg::EXPLOSION_DAMAGE_MAX * damageMult;
        }
        if (c.shieldTurnsLeft > 0) dmg = 0.0f;

        c.TakeDamage(dmg);
        if (i == 0) dmgAppliedP1 = dmg;
        else if (i == 1) dmgAppliedP2 = dmg;
    }

    for (int i = 0; i < roster.CannonCount(); ++i) {
        Cannon& c = roster.At(i);
        c.groundY = terrain.HeightAt(c.x);
    }

    if (IsTeamMode(matchFormat)) {
        const bool team0Buried = roster.IsTeamBuried(0, terrain);
        const bool team1Buried = roster.IsTeamBuried(1, terrain);
        if (team0Buried || team1Buried) {
            if (team0Buried) {
                for (int i = 0; i < roster.PerTeam(); ++i) roster.At(i).health = 0.0f;
            }
            if (team1Buried) {
                for (int i = 0; i < roster.PerTeam(); ++i) {
                    roster.At(roster.PerTeam() + i).health = 0.0f;
                }
            }
            roundOutcome = (team0Buried && team1Buried) ? RoundOutcome::DrawBuried
                           : (team0Buried ? RoundOutcome::TeamBWinsBuried : RoundOutcome::TeamAWinsBuried);
            state = GameState::RoundOver;
            stateTimer = 1.0f;
        } else {
            CheckRoundEnd();
        }
    } else {
        bool p1Buried = terrain.IsFullyGone(GetCannon(1).x);
        bool p2Buried = terrain.IsFullyGone(GetCannon(2).x);
        if (p1Buried || p2Buried) {
            if (p1Buried) GetCannon(1).health = 0.0f;
            if (p2Buried) GetCannon(2).health = 0.0f;

            roundOutcome = (p1Buried && p2Buried) ? RoundOutcome::DrawBuried
                           : (p1Buried ? RoundOutcome::P2WinsBuried : RoundOutcome::P1WinsBuried);
            state = GameState::RoundOver;
            stateTimer = 1.0f;
        } else {
            CheckRoundEnd();
        }
    }

    if (mode == GameMode::Online) {
        const float windAtShot = windForce;
        bool matchOver = (state == GameState::RoundOver);
        int winnerPlayer = 0;
        if (matchOver) {
            switch (roundOutcome) {
                case RoundOutcome::P1Wins:
                case RoundOutcome::P1WinsBuried:
                case RoundOutcome::TeamAWins:
                case RoundOutcome::TeamAWinsBuried:
                    winnerPlayer = 1; break;
                case RoundOutcome::P2Wins:
                case RoundOutcome::P2WinsBuried:
                case RoundOutcome::TeamBWins:
                case RoundOutcome::TeamBWinsBuried:
                    winnerPlayer = 2; break;
                default: winnerPlayer = 0; break;
            }
        }

        int nextTurnIndex = netMatch.TurnsCompleted() + 1;
        float nextWind = matchOver ? windForce : SeededWind(nextTurnIndex);

        netMatch.PublishShotEnded(impactPos.x, impactPos.y);
        netMatch.SubmitMyTurn(shooter.angleDeg, shooter.power01, windAtShot,
                               impactPos.x, impactPos.y, craterRadius,
                               dmgAppliedP1, dmgAppliedP2, nextWind, matchOver, winnerPlayer,
                               powerups.ShotPickedType(), powerups.ShotPickedX());
        powerups.ShotPickedType() = -1;
        powerups.ShotPickedX() = 0.0f;
        currentPlayer = netMatch.SyncedCurrentTurnPlayer();

        if (matchOver) {
            if (winnerPlayer != 0) {
                onlineLobby.ReportMatchResult(winnerPlayer == netMatch.MyPlayerNumber());
            }
        } else {
            windForce = nextWind;
        }
    }
}

void Game::CheckRoundEnd() {
    if (IsTeamMode(matchFormat)) {
        const bool team0Dead = roster.IsTeamEliminated(0);
        const bool team1Dead = roster.IsTeamEliminated(1);
        if (team0Dead || team1Dead) {
            roundOutcome = (team0Dead && team1Dead) ? RoundOutcome::Draw
                           : (team0Dead ? RoundOutcome::TeamBWins : RoundOutcome::TeamAWins);
            state = GameState::RoundOver;
            stateTimer = 1.0f;
            return;
        }
    } else if (!GetCannon(1).IsAlive() || !GetCannon(2).IsAlive()) {
        roundOutcome = (!GetCannon(1).IsAlive() && !GetCannon(2).IsAlive()) ? RoundOutcome::Draw
                       : (!GetCannon(1).IsAlive() ? RoundOutcome::P2Wins : RoundOutcome::P1Wins);
        state = GameState::RoundOver;
        stateTimer = 1.0f;
        return;
    }
    EndTurn();
}

void Game::EndTurn() {
    if (state == GameState::RoundOver) return;
    if (mode != GameMode::Online) {
        currentPlayer = roster.NextSlotInterleaved(ActiveSlot()) + 1;
    }
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;

    if (mode == GameMode::Online) {
        if (version == GameVersion::Plus) {
            const int nextPlayer = roster.NextSlotInterleaved(ActiveSlot()) + 1;
            OnOnlineTurnCompleted(nextPlayer);
        } else {
            onlineCompletedTurns++;
        }
    } else {
        windForce = RandF(-1.0f, 1.0f) *
                    std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());

        if (version == GameVersion::Plus) {
            GetCannon(currentPlayer).OnTurnStarted();
            powerups.TickSpawnCounter();
            powerups.MaybeSpawnRandom();
        }
    }

    stateTimer = 0.4f;
    state = GameState::TurnTransition;
}
