#include "Game.h"
#include "AmmoVisuals.h"
#include "AssetPath.h"
#include "CannonUILayout.h"
#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"
#include "ShopCatalog.h"
#include "VirtualScreen.h"
#include "net/NetWorker.h"
#include "net/SupabaseClient.h"
#include "net/JsonHelpers.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

void Game::SyncCannonHealthFromTurn(const RemoteTurnResult& turn) {
    const int count = std::min(roster.CannonCount(), MatchRoster::kMaxCannons);
    if (turn.hasHealthAfter && turn.healthAfterCount > 0) {
        const int hpCount = std::min(turn.healthAfterCount, count);
        for (int i = 0; i < hpCount; ++i) {
            GetCannon(i + 1).health = std::max(0.0f, turn.healthAfter[i]);
        }
    } else {
        for (int i = 0; i < count; ++i) {
            GetCannon(i + 1).TakeDamage(turn.damages[i]);
        }
    }
    for (int i = 0; i < roster.CannonCount(); ++i) {
        Cannon& c = GetCannon(i + 1);
        c.groundY = terrain.HeightAt(c.x);
    }
}

void Game::ApplyRemoteTurnDamage(const RemoteTurnResult& turn) {
    SyncCannonHealthFromTurn(turn);
}

int Game::ResolveOnlineTurnPlayer(int proposedPlayer) const {
    const int player = ClampPlayerNum(proposedPlayer);
    if (roster.IsPlayerAlive(player)) return player;
    return roster.NextLivingPlayerNum(player);
}

void Game::MaybeAdvancePastDeadOnlineTurn(float dt) {
    if (onlineDeadTurnSkipCooldown_ > 0.0f) {
        onlineDeadTurnSkipCooldown_ = std::max(0.0f, onlineDeadTurnSkipCooldown_ - dt);
    }

    if (mode != GameMode::Online || !netMatch.InMatch() || isSpectating) return;
    if (state == GameState::RoundOver || state == GameState::RemoteShotReplay ||
        state == GameState::RemoteProjectileLive || state == GameState::ProjectileFlying) {
        return;
    }

    const int turnPlayer = netMatch.SyncedCurrentTurnPlayer();
    if (!roster.IsPlayerAlive(turnPlayer)) {
        const int nextLiving = roster.NextLivingPlayerNum(turnPlayer);
        if (!roster.IsPlayerAlive(nextLiving) || nextLiving == turnPlayer) return;
        if (onlineDeadTurnSkipCooldown_ > 0.0f) return;

        onlineDeadTurnSkipCooldown_ = 1.0f;
        DebugLogf(LOG_INFO, "GAME: turno P%d morto — avancando para P%d", turnPlayer, nextLiving);
        netMatch.SyncTurnTo(nextLiving);
    }
}

void Game::ApplyOnlineNamesFromMatchStart(const MatchStart& ms) {
    onlinePlayerNames.fill("");
    onlineEquippedCannonColors.fill(kDefaultCannonColorId);
    onlineEquippedCannonSkins.fill(kDefaultCannonSkinId);
    onlineEquippedCannonEffects.fill(kDefaultCannonEffectId);
    onlineEquippedNameEffects.fill(kDefaultNameEffectId);
    onlineEquippedAmmo.fill(kDefaultAmmoId);
    const int count = ms.composition.TotalPlayers();
    for (int i = 0; i < count && i < MatchRoster::kMaxCannons; ++i) {
        onlinePlayerNames[static_cast<size_t>(i)] = ms.playerNames[i];
        if (!ms.equippedCannonColors[i].empty()) {
            onlineEquippedCannonColors[static_cast<size_t>(i)] = ms.equippedCannonColors[i];
        }
        if (!ms.equippedCannonSkins[i].empty()) {
            onlineEquippedCannonSkins[static_cast<size_t>(i)] = ms.equippedCannonSkins[i];
        }
        if (!ms.equippedCannonEffects[i].empty()) {
            onlineEquippedCannonEffects[static_cast<size_t>(i)] = ms.equippedCannonEffects[i];
        }
        if (!ms.equippedNameEffects[i].empty()) {
            onlineEquippedNameEffects[static_cast<size_t>(i)] = ms.equippedNameEffects[i];
        }
        if (!ms.equippedAmmo[i].empty()) {
            onlineEquippedAmmo[static_cast<size_t>(i)] = ms.equippedAmmo[i];
        }
    }
}

void Game::StartOnlineMatch(const MatchStart& ms) {
    const int maxPlayer = ms.composition.TotalPlayers();
    if (ms.myPlayerNumber < 1 || ms.myPlayerNumber > maxPlayer) {
        DebugLogf(LOG_ERROR, "GAME: StartOnlineMatch abortado — P%d invalido (max=%d)",
                  ms.myPlayerNumber, maxPlayer);
        return;
    }

    mode = GameMode::Online;
    isSpectating = false;
    matchComposition = ms.composition;
    matchFormat = (ms.composition.teamA == 1 && ms.composition.teamB == 1)
        ? MatchFormat::Duel1v1
        : MatchFormat::Team2v2;
    version = ms.isPlus ? GameVersion::Plus : GameVersion::Classic;
    onlineWinByDisconnect = false;
    onlineDeadTurnSkipCooldown_ = 0.0f;
    onlineLobby.MarkInMatch(ms.matchId);
    onlineLobby.PauseRealtime();

    ApplyOnlineNamesFromMatchStart(ms);

    onlineLobby.SnapshotRematchRoom();
    netMatch.Begin(ms.matchId, ms.myPlayerNumber, ms.opponentId, ms.opponentName, ms.composition);
    ResetRound(ms.terrainSeed);
    currentPlayer = ResolveOnlineTurnPlayer(netMatch.SyncedCurrentTurnPlayer());
    opponentAimForTurn = netMatch.TurnsCompleted() + 1;
    if (!netMatch.IsMyTurn()) {
        ResetOpponentAimSim(currentPlayer);
    }

    if (audioReady && musicTracks[currentMusicIndex].frameCount > 0) {
        PlayMusicStream(musicTracks[currentMusicIndex]);
    }
}

void Game::ShutdownOnlinePresence() {
    if (mode != GameMode::Online) return;

    if (netMatch.InMatch()) {
        if (!isSpectating && netMatch.MyPlayerNumber() != 0) {
            const std::string mid = netMatch.MatchId();
            onlineLobby.MarkIdle();
            onlineLobby.AbandonActiveMatch(mid, netMatch.AbandonWinnerPlayer());
        }
        netMatch.LeaveMatch();
    } else if (state == GameState::OnlineLobby) {
        onlineLobby.LeaveLobby();
    }
}

void Game::EndOnlineMatchOpponentLeft() {
    onlineWinByDisconnect = true;
    onlineLobby.ClearRematchRoom();
    int winner = netMatch.TakeAbandonWinner();
    if (winner == 0) {
        winner = WinnerWhenPlayerLeaves(netMatch.MyPlayerNumber(), matchComposition);
    }
    roundOutcome = RoundOutcomeFromOnlineWinner(winner, matchComposition, false);
    onlineLobby.ReportMatchResult(OnlineDidIWin(winner, netMatch.MyPlayerNumber(), matchComposition));
    onlineLobby.MarkIdle();
    netMatch.LeaveMatch();
    state = GameState::RoundOver;
    stateTimer = 1.0f;
}

float Game::SeededWind(int turnIndex) const {
    float u = SeededUnitFloat(onlineSeed, static_cast<unsigned>(turnIndex * 7919u + 12345u));
    float signed01 = u * 2.0f - 1.0f;
    return signed01 * std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());
}

void Game::ResetOpponentAimSim(int shooterPlayer) {
    opponentAimPlayer = shooterPlayer;
    opponentAimOscTimer = 0.0f;
    opponentAimAngle = -90.0f;
    opponentAimPower = 0.0f;
    opponentAimTargetAngle = -90.0f;
    opponentAimTargetPower = 0.0f;
    opponentAimHasLiveTarget = false;
    opponentAimSimActive = shooterPlayer != 0;
}

void Game::UpdateOpponentAimSim(float dt) {
    if (!opponentAimSimActive || opponentAimPlayer == 0) return;
    // Feedback visual local: mesma faixa de ângulo do jogo (-90°..90°),
    // onda triangular — não precisa bater com a mira real do adversário.
    opponentAimOscTimer += dt;
    float period = cfg::ANGLE_OSC_PERIOD_SEC;
    float t = fmodf(opponentAimOscTimer, period) / period; // 0..1
    float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
    opponentAimAngle = -90.0f + tri * 180.0f;

    float powerPeriod = cfg::POWER_OSC_PERIOD_SEC;
    float pt = fmodf(opponentAimOscTimer, powerPeriod) / powerPeriod;
    float ptri = (pt < 0.5f) ? (pt * 2.0f) : (2.0f - pt * 2.0f);
    opponentAimPower = ptri;
}

void Game::DrawOpponentAim(int shooterPlayer, float angleDeg, float power01) const {
    const int safeShooter = ClampPlayerNum(shooterPlayer);
    const Cannon& active = GetCannon(safeShooter);
    Vector2 base = { active.x, active.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };
    Color aimColor = Fade(SKYBLUE, 0.75f);

    Vector2 dir = active.DirectionAtAngle(angleDeg);
    Vector2 tip = { base.x + dir.x * 90.0f, base.y + dir.y * 90.0f };
    DrawLineEx(base, tip, 3.0f, aimColor);
    DrawCircleV(tip, 4.0f, SKYBLUE);

    float barW = 100.0f, barH = 12.0f;
    Vector2 barPos = { active.x - barW / 2, cannon_ui::PowerBarY(active, barH) };
    DrawRectangle(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                  static_cast<int>(barW), static_cast<int>(barH), Color{30, 30, 30, 200});
    float fill = std::clamp(power01, 0.0f, 1.0f) * barW;
    DrawRectangle(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                  static_cast<int>(fill), static_cast<int>(barH), Fade(SKYBLUE, 0.85f));
    DrawRectangleLines(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                       static_cast<int>(barW), static_cast<int>(barH), BLACK);
}

void Game::BeginRemoteShotReplay(const RemoteTurnResult& remote) {
    pendingRemoteTurn = remote;
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    remoteLiveActive = false;
    remoteLiveHasPendingResult = false;

    Cannon& shooter = GetCannon(remote.shooterPlayer);
    shooter.SetAim(remote.shootAngle, remote.shootPower);
    windForce = remote.windAtShot;
    currentPlayer = ClampPlayerNum(remote.shooterPlayer);

    Vector2 start = shooter.MuzzlePosition();
    remoteReplayPos = start;
    remoteReplayT = 0.0f;
    remoteReplayAimTimer = 0.08f;
    prevProjectilePos = start;
    guidedPathT = 0.0f;

    if (audioReady) PlaySound(sndFire);
    if (version == GameVersion::Plus) shooter.OnShotFired();
    powerups.ResetRemotePickupShot();
    state = GameState::RemoteShotReplay;
}

void Game::BeginRemoteProjectileLive(const LiveShotStart& shot) {
    remoteLiveActive = true;
    remoteLiveShotId = shot.shotId;
    remoteLiveLastSeq = -1;
    remoteLivePlayT = 0.0f;
    remoteLivePlayStarted = false;
    remoteLiveHasPendingResult = false;
    remoteLiveWatchTimer = 0.0f;
    remoteLiveSamples.clear();
    remoteLivePos = { shot.muzzleX, shot.muzzleY };
    remoteLiveSamples.push_back(ProjSample{ 0, 0.0f, shot.muzzleX, shot.muzzleY });
    remoteLiveLastSeq = 0;

    Cannon& shooter = GetCannon(shot.shooterPlayer);
    shooter.SetAim(shot.angleDeg, shot.power01);
    windForce = shot.wind;
    currentPlayer = ClampPlayerNum(shot.shooterPlayer);
    {
        Vector2 dir = shooter.DirectionAtAngle(shot.angleDeg);
        const float speed = cfg::MIN_POWER + shot.power01 * (cfg::MAX_POWER - cfg::MIN_POWER);
        remoteLivePredVel = { dir.x * speed * cfg::PPM, dir.y * speed * cfg::PPM };
    }
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    opponentAimHasLiveTarget = false;

    if (audioReady) PlaySound(sndFire);
    if (version == GameVersion::Plus) shooter.OnShotFired();
    state = GameState::RemoteProjectileLive;
    powerups.ResetRemotePickupShot();
    DebugLogf(LOG_INFO, "GAME: stream projétil ao vivo P%d", shot.shooterPlayer);
}

void Game::UpdateRemoteProjectileLive(float dt) {
    remoteLiveWatchTimer += dt;

    // Puxa amostras novas do NetMatch
    std::vector<ProjSample> fresh;
    if (netMatch.PullProjectileSamples(remoteLiveLastSeq, fresh) > 0) {
        for (const auto& s : fresh) {
            remoteLiveSamples.push_back(s);
            if (s.seq > remoteLiveLastSeq) remoteLiveLastSeq = s.seq;
        }
    }

    // Resultado autoritativo enquanto ainda assistimos o stream.
    if (!remoteLiveHasPendingResult) {
        RemoteTurnResult remote;
        if (netMatch.PollOpponentTurn(remote)) {
            pendingRemoteTurn = remote;
            remoteLiveHasPendingResult = true;
        }
    }

    if (remoteLiveSamples.size() < 2) {
        if (remoteLiveHasPendingResult) {
            const auto& r = pendingRemoteTurn;
            ProjSample fin;
            fin.seq = remoteLiveLastSeq + 1;
            fin.t = std::max(0.12f, remoteLiveWatchTimer);
            fin.x = r.impactX;
            fin.y = r.impactY;
            remoteLiveSamples.push_back(fin);
            remoteLiveLastSeq = fin.seq;
        } else if (remoteLiveWatchTimer > 6.0f) {
            DebugLogf(LOG_WARNING, "GAME: stream live timeout sem amostras — volta a Aiming");
            remoteLiveActive = false;
            netMatch.ClearLiveShot();
            state = GameState::Aiming;
            return;
        }
    }

    if (!remoteLivePlayStarted) {
        remoteLivePlayT = std::max(0.0f, remoteLiveSamples.front().t);
        remoteLivePlayStarted = true;
    }

    const float maxT = remoteLiveSamples.back().t;
    float targetPlay = remoteLivePlayT + dt;

    float endX = 0, endY = 0;
    const bool streamEnded = netMatch.PeekShotEnded(endX, endY);
    const bool canFinishPath = streamEnded || remoteLiveHasPendingResult;
    if (!canFinishPath && remoteLiveSamples.size() >= 2) {
        const float cap = std::max(0.0f, maxT - remoteLiveBufferDelay);
        if (targetPlay > cap) targetPlay = cap;
    } else if (canFinishPath && targetPlay > maxT) {
        targetPlay = maxT;
    }
    remoteLivePlayT = targetPlay;

    const auto stepPrediction = [&](float stepDt) {
        remoteLivePredVel.x += windForce * cfg::PPM * stepDt;
        remoteLivePredVel.y += cfg::GRAVITY_MPS2 * cfg::PPM * stepDt;
        remoteLivePos.x += remoteLivePredVel.x * stepDt;
        remoteLivePos.y += remoteLivePredVel.y * stepDt;
    };

    Vector2 prev = remoteLivePos;
    if (remoteLiveSamples.size() < 2) {
        stepPrediction(dt);
    } else if (remoteLivePlayT <= remoteLiveSamples.front().t) {
        remoteLivePos = { remoteLiveSamples.front().x, remoteLiveSamples.front().y };
    } else if (remoteLivePlayT >= remoteLiveSamples.back().t && !canFinishPath) {
        if (remoteLiveSamples.size() >= 2) {
            const auto& a = remoteLiveSamples[remoteLiveSamples.size() - 2];
            const auto& b = remoteLiveSamples.back();
            const float span = std::max(1e-4f, b.t - a.t);
            remoteLivePredVel.x = (b.x - a.x) / span;
            remoteLivePredVel.y = (b.y - a.y) / span;
        }
        stepPrediction(dt);
    } else if (remoteLivePlayT >= remoteLiveSamples.back().t) {
        remoteLivePos = { remoteLiveSamples.back().x, remoteLiveSamples.back().y };
    } else {
        for (size_t i = 0; i + 1 < remoteLiveSamples.size(); ++i) {
            const auto& a = remoteLiveSamples[i];
            const auto& b = remoteLiveSamples[i + 1];
            if (remoteLivePlayT >= a.t && remoteLivePlayT <= b.t) {
                float span = std::max(1e-4f, b.t - a.t);
                float u = (remoteLivePlayT - a.t) / span;
                u = u * u * (3.0f - 2.0f * u);
                remoteLivePos.x = a.x + (b.x - a.x) * u;
                remoteLivePos.y = a.y + (b.y - a.y) * u;
                remoteLivePredVel.x = (b.x - a.x) / span;
                remoteLivePredVel.y = (b.y - a.y) / span;
                break;
            }
        }
    }

    Vector2 vel = { remoteLivePos.x - prev.x, remoteLivePos.y - prev.y };
    {
        const Cannon& shooter = GetCannon(currentPlayer);
        EmitAmmoTrail(particles, remoteLivePos, vel, shooter.ammoStyle,
                      version == GameVersion::Plus && shooter.pendingDoubleDamage,
                      version == GameVersion::Plus && shooter.pendingGuided);
    }

    if (remoteLiveHasPendingResult) {
        const auto& r = pendingRemoteTurn;
        const auto& last = remoteLiveSamples.back();
        if (std::fabs(last.x - r.impactX) > 2.0f || std::fabs(last.y - r.impactY) > 2.0f) {
            ProjSample fin;
            fin.seq = remoteLiveLastSeq + 1;
            fin.t = last.t + 0.08f;
            fin.x = r.impactX;
            fin.y = r.impactY;
            remoteLiveSamples.push_back(fin);
            remoteLiveLastSeq = fin.seq;
            return; // próximo frame interpola até o impacto
        }

        const bool atEnd = remoteLivePlayT >= remoteLiveSamples.back().t - 0.001f;
        // Timeout de segurança: não segurar o adversário sem turno.
        if (atEnd || remoteLiveWatchTimer > 2.5f) {
            FinishRemoteTurn(pendingRemoteTurn);
            return;
        }
    }
}

void Game::UpdateRemoteShotReplay(float dt) {
    const RemoteTurnResult& remote = pendingRemoteTurn;
    Vector2 start = GetCannon(remote.shooterPlayer).MuzzlePosition();
    Vector2 end = { remote.impactX, remote.impactY };

    if (remoteReplayAimTimer > 0.0f) {
        remoteReplayAimTimer -= dt;
        opponentAimPlayer = remote.shooterPlayer;
        opponentAimAngle = remote.shootAngle;
        opponentAimPower = remote.shootPower;
        return;
    }
    opponentAimPlayer = 0;

    constexpr float kDuration = 0.55f;
    remoteReplayT += dt / kDuration;
    float t = std::clamp(remoteReplayT, 0.0f, 1.0f);

    Vector2 prev = remoteReplayPos;
    remoteReplayPos.x = start.x + (end.x - start.x) * t;
    remoteReplayPos.y = start.y + (end.y - start.y) * t - std::sin(t * PI) * 80.0f;

    Vector2 vel = { remoteReplayPos.x - prev.x, remoteReplayPos.y - prev.y };
    {
        const Cannon& shooter = GetCannon(remote.shooterPlayer);
        EmitAmmoTrail(particles, remoteReplayPos, vel, shooter.ammoStyle,
                      version == GameVersion::Plus && shooter.pendingDoubleDamage,
                      version == GameVersion::Plus && shooter.pendingGuided);
    }

    if (t >= 1.0f) {
        FinishRemoteTurn(remote);
    }
}

void Game::FinishRemoteTurn(const RemoteTurnResult& remote) {
    Vector2 impactPos = { remote.impactX, remote.impactY };
    remoteLiveActive = false;
    remoteLiveHasPendingResult = false;
    remoteLiveSamples.clear();
    netMatch.ClearLiveShot();

    // Power-ups coletados no tiro remoto (visual + efeitos nos badges).
    if (version == GameVersion::Plus) {
        const bool applyEffects = !powerups.RemotePickupsAppliedThisShot();
        for (const auto& pu : remote.pickedPowerups) {
            powerups.ApplyRemotePickup(pu.type, pu.x, GetCannon(remote.shooterPlayer),
                                       language, applyEffects, powerups.RemoteEffectApplied());
        }
    }
    powerups.RemoteEffectApplied() = false;

    Cannon& shooter = GetCannon(remote.shooterPlayer);
    if (version == GameVersion::Plus) {
        shooter.OnShotResolved();
        if (roster.IsPlayerAlive(remote.shooterPlayer)) {
            shooter.OnTurnEnded();
        }
    }

    if (audioReady) PlaySound(sndExplosion);
    EmitAmmoImpact(particles, impactPos, shooter.ammoStyle);
    terrain.Explode(impactPos.x, impactPos.y, remote.craterRadius);
    if (shooter.ammoStyle == AmmoStyle::Nuclear) {
        effects.TriggerShake(cfg::NUCLEAR_SHAKE_MAGNITUDE_PX, cfg::NUCLEAR_SHAKE_DURATION_SEC);
    }

    ApplyRemoteTurnDamage(remote);

    if (version == GameVersion::Plus && shooter.ammoStyle != AmmoStyle::Nuclear) {
        effects.TriggerShake(cfg::SHAKE_MAGNITUDE_TERRAIN_PX, cfg::SHAKE_DURATION_TERRAIN_SEC);
    }

    TryOnlinePowerupSpawn(remote.turnNumber);

    windForce = remote.nextWind;
    currentPlayer = ResolveOnlineTurnPlayer(remote.nextTurnPlayer);
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;
    prevProjectilePos = {0, 0};
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    opponentAimForTurn = 0;
    opponentAimHasLiveTarget = false;

    if (remote.matchOver) {
        if (isSpectating) {
            roundOutcome = RoundOutcomeFromOnlineWinner(remote.winnerPlayer, matchComposition, false);
            state = GameState::RoundOver;
            stateTimer = 1.0f;
        } else {
            const bool draw = (remote.winnerPlayer == 0);
            if (draw) {
                roundOutcome = RoundOutcome::Draw;
            } else {
                roundOutcome = RoundOutcomeFromOnlineWinner(remote.winnerPlayer, matchComposition, false);
            }
            if (!draw) {
                onlineLobby.ReportMatchResult(
                    OnlineDidIWin(remote.winnerPlayer, netMatch.MyPlayerNumber(), matchComposition));
            }
            onlineLobby.MarkIdle();
            state = GameState::RoundOver;
            stateTimer = 1.0f;
        }
    } else {
        state = GameState::TurnTransition;
        stateTimer = 0.35f;
    }
}

void Game::TryOnlinePowerupSpawn(int completedTurn, int* outType, float* outX) {
    if (mode != GameMode::Online || version != GameVersion::Plus) return;
    if (completedTurn <= 0 || completedTurn == lastOnlineSpawnTurn_) return;
    if (completedTurn % cfg::POWERUP_SPAWN_EVERY_TURNS != 0) return;
    lastOnlineSpawnTurn_ = completedTurn;
    powerups.MaybeSpawnSeeded(onlineSeed, completedTurn, roster, outType, outX, true);
}

void Game::ApplyOnlinePowerupSpawn(int type, float x) {
    if (version != GameVersion::Plus || type < 0 || type >= static_cast<int>(PowerupType::COUNT)) return;
    for (const auto& pu : powerups.Active()) {
        if (pu.active && static_cast<int>(pu.type) == type && std::fabs(pu.x - x) < 1.0f) return;
    }
    powerups.SpawnAtExact(x, static_cast<PowerupType>(type));
}

void Game::ConsumeRemotePowerupSpawns() {
    LivePowerupSpawn sp;
    while (netMatch.PollRemotePowerupSpawn(sp)) {
        if (sp.type < 0) continue;
        ApplyOnlinePowerupSpawn(sp.type, sp.x);
    }
}

void Game::ConsumeRemotePowerupPickups() {
    LivePowerupPickup pu;
    while (netMatch.PollRemotePowerupPickup(pu)) {
        if (pu.player <= 0 || pu.type < 0) continue;
        powerups.ApplyRemotePickup(pu.type, pu.x, GetCannon(pu.player),
                                   language, true, powerups.RemoteEffectApplied());
    }
}

void Game::ApplyTurnSilently(const RemoteTurnResult& turn) {
    if (version == GameVersion::Plus) {
        for (const auto& pu : turn.pickedPowerups) {
            powerups.ApplyRemotePickup(pu.type, pu.x, GetCannon(turn.shooterPlayer),
                                       language, true, powerups.RemoteEffectApplied());
        }
        powerups.RemoteEffectApplied() = false;
    }

    Cannon& shooter = GetCannon(turn.shooterPlayer);
    if (version == GameVersion::Plus) {
        shooter.OnShotResolved();
        if (roster.IsPlayerAlive(turn.shooterPlayer)) {
            shooter.OnTurnEnded();
        }
    }

    terrain.Explode(turn.impactX, turn.impactY, turn.craterRadius);
    ApplyRemoteTurnDamage(turn);

    TryOnlinePowerupSpawn(turn.turnNumber);

    windForce = turn.nextWind;
    currentPlayer = ResolveOnlineTurnPlayer(turn.nextTurnPlayer);
}

void Game::StartSpectating(const ActiveMatchCard& match) {
    using json = nlohmann::json;
    SupabaseClient client;
    json matchRows = client.Select("matches",
        "select=terrain_seed,version,wind,current_turn_player,status,winner_player,match_format,"
        "team_a_count,team_b_count,"
        "player1_id,player2_id,player3_id,player4_id,player5_id,player6_id,player7_id,player8_id,player9_id,player10_id"
        "&id=eq." + match.matchId);
    if (!matchRows.is_array() || matchRows.empty()) {
        DebugLogf(LOG_WARNING, "GAME: espectador — partida %s não encontrada", match.matchId.c_str());
        return;
    }

    const json& mrow = matchRows[0];
    const unsigned int seed = static_cast<unsigned int>(json_helpers::Int64(mrow, "terrain_seed", 0));
    const bool isPlus = (json_helpers::Str(mrow, "version", "classic") == "plus");
    const std::string status = json_helpers::Str(mrow, "status", "active");
    const int currentTurn = json_helpers::Int(mrow, "current_turn_player", 1);
    const int winner = json_helpers::Int(mrow, "winner_player", 0);
    const float matchWind = json_helpers::Float(mrow, "wind", 0.0f);
    const MatchComposition comp = ParseMatchComposition(
        json_helpers::Str(mrow, "match_format", "duel_1v1"),
        json_helpers::Int(mrow, "team_a_count", 0),
        json_helpers::Int(mrow, "team_b_count", 0));

    json turnRows = client.Select("match_turns",
        "select=*&match_id=eq." + match.matchId + "&order=turn_number.asc&limit=200");

    isSpectating = true;
    mode = GameMode::Online;
    matchComposition = comp;
    matchFormat = comp.IsTeamGame() ? MatchFormat::Team2v2 : MatchFormat::Duel1v1;
    version = isPlus ? GameVersion::Plus : GameVersion::Classic;
    onlineWinByDisconnect = false;
    onlineLobby.PauseRealtime();

    onlinePlayerNames.fill("");
    onlineEquippedCannonColors.fill(kDefaultCannonColorId);
    onlineEquippedCannonSkins.fill(kDefaultCannonSkinId);
    onlineEquippedCannonEffects.fill(kDefaultCannonEffectId);
    onlineEquippedNameEffects.fill(kDefaultNameEffectId);
    onlineEquippedAmmo.fill(kDefaultAmmoId);
    std::string pidList;
    for (int i = 0; i < comp.TotalPlayers() && i < MatchRoster::kMaxCannons; ++i) {
        const std::string key = "player" + std::to_string(i + 1) + "_id";
        const std::string pid = json_helpers::Str(mrow, key.c_str());
        if (pid.empty()) continue;
        if (!pidList.empty()) pidList += ",";
        pidList += pid;
    }
    std::unordered_map<std::string, std::string> namesById;
    struct OnlineCosmetics {
        std::string color;
        std::string skin;
        std::string effect;
        std::string nameEffect;
        std::string ammo;
    };
    std::unordered_map<std::string, OnlineCosmetics> cosmeticsById;
    if (!pidList.empty()) {
        json prows = client.Select("players",
            "select=id,display_name,equipped_cannon_color,equipped_cannon_skin,equipped_cannon_effect,equipped_name_effect,equipped_ammo&id=in.("
            + pidList + ")");
        if (prows.is_array()) {
            for (const auto& p : prows) {
                const std::string id = json_helpers::Str(p, "id");
                namesById[id] = json_helpers::Str(p, "display_name", "???");
                cosmeticsById[id] = {
                    json_helpers::Str(p, "equipped_cannon_color", kDefaultCannonColorId),
                    json_helpers::Str(p, "equipped_cannon_skin", kDefaultCannonSkinId),
                    json_helpers::Str(p, "equipped_cannon_effect", kDefaultCannonEffectId),
                    json_helpers::Str(p, "equipped_name_effect", kDefaultNameEffectId),
                    json_helpers::Str(p, "equipped_ammo", kDefaultAmmoId)
                };
            }
        }
    }
    for (int i = 0; i < comp.TotalPlayers() && i < MatchRoster::kMaxCannons; ++i) {
        const std::string key = "player" + std::to_string(i + 1) + "_id";
        const std::string pid = json_helpers::Str(mrow, key.c_str());
        if (!pid.empty()) {
            auto it = namesById.find(pid);
            onlinePlayerNames[static_cast<size_t>(i)] =
                (it != namesById.end()) ? it->second : "???";
            auto cit = cosmeticsById.find(pid);
            if (cit != cosmeticsById.end()) {
                onlineEquippedCannonColors[static_cast<size_t>(i)] = cit->second.color;
                onlineEquippedCannonSkins[static_cast<size_t>(i)] = cit->second.skin;
                onlineEquippedCannonEffects[static_cast<size_t>(i)] = cit->second.effect;
                onlineEquippedNameEffects[static_cast<size_t>(i)] = cit->second.nameEffect;
                onlineEquippedAmmo[static_cast<size_t>(i)] = cit->second.ammo;
            }
        } else {
            onlinePlayerNames[static_cast<size_t>(i)] =
                (i == 0) ? match.player1Name : (i == comp.teamA) ? match.player2Name : "???";
        }
    }

    ResetRound(seed);
    onlineSeed = seed;
    powerups.Reset();
    if (version == GameVersion::Plus) {
        powerups.SetSpawnEveryTurns(cfg::POWERUP_SPAWN_EVERY_TURNS);
    }

    int lastTurnNumber = 0;
    if (turnRows.is_array()) {
        for (const auto& row : turnRows) {
            RemoteTurnResult turn = NetMatch::ParseTurnJson(row);
            if (turn.turnNumber <= 0) continue;
            ApplyTurnSilently(turn);
            lastTurnNumber = turn.turnNumber;
        }
    }

    windForce = (lastTurnNumber > 0) ? windForce : matchWind;
    currentPlayer = ResolveOnlineTurnPlayer((lastTurnNumber > 0) ? currentPlayer : currentTurn);

    netMatch.BeginSpectating(match.matchId, currentPlayer, lastTurnNumber, comp);

    if (audioReady && musicTracks[currentMusicIndex].frameCount > 0) {
        PlayMusicStream(musicTracks[currentMusicIndex]);
    }

    if (status == "finished" || status == "abandoned") {
        EndSpectatorMatch(winner);
        return;
    }

    state = GameState::Aiming;
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;
    opponentAimForTurn = netMatch.TurnsCompleted() + 1;
    ResetOpponentAimSim(currentPlayer);
    DebugLogf(LOG_INFO, "GAME: espectador entrou match=%s turnos=%d", match.matchId.c_str(), lastTurnNumber);
}

void Game::EndSpectatorMatch(int winnerPlayer) {
    roundOutcome = (winnerPlayer == 0) ? RoundOutcome::Draw
                   : RoundOutcomeFromOnlineWinner(winnerPlayer, matchComposition, false);
    state = GameState::RoundOver;
    stateTimer = 1.0f;
}

void Game::ReturnToOnlineLobbyAfterMatch() {
    if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
    netMatch.LeaveMatch();
    isSpectating = false;
    onlineLobby.ReturnToLobbyAfterMatch();
    state = GameState::OnlineLobby;
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;
}

void Game::ReturnToTeamRoomForRematch() {
    if (onlineWinByDisconnect) return;
    if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
    netMatch.LeaveMatch();
    isSpectating = false;
    if (onlineLobby.HasRematchTeamRoom()) {
        onlineLobby.EnterTeamRoomForRematch();
        state = GameState::OnlineTeamRoom;
    } else {
        onlineLobby.ReturnToLobbyAfterMatch();
        state = GameState::OnlineLobby;
    }
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;
}

namespace {

Rectangle OnlineRematchBtnRect() {
    return { cfg::SCREEN_WIDTH / 2.0f - 210.0f, cfg::SCREEN_HEIGHT / 2.0f + 40.0f, 200.0f, 48.0f };
}

Rectangle OnlineLobbyBtnRect() {
    return { cfg::SCREEN_WIDTH / 2.0f + 10.0f, cfg::SCREEN_HEIGHT / 2.0f + 40.0f, 200.0f, 48.0f };
}

} // namespace

void Game::UpdateOnlineRoundOver() {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;

    const Vector2 mouse = ::GetVirtualMouse();
    if (!onlineWinByDisconnect && onlineLobby.HasRematchTeamRoom() &&
        CheckCollisionPointRec(mouse, OnlineRematchBtnRect())) {
        ReturnToTeamRoomForRematch();
        return;
    }

    const Rectangle lobbyRect = onlineWinByDisconnect
        ? Rectangle{ cfg::SCREEN_WIDTH / 2.0f - 100.0f, cfg::SCREEN_HEIGHT / 2.0f + 40.0f, 200.0f, 48.0f }
        : OnlineLobbyBtnRect();
    if (CheckCollisionPointRec(mouse, lobbyRect)) {
        ReturnToOnlineLobbyAfterMatch();
    }
}

void Game::DrawOnlineRoundOverOptions() const {
    const Vector2 mouse = ::GetVirtualMouse();

    auto drawBtn = [&](Rectangle rect, const char* label, Color base, Color hover) {
        const bool h = CheckCollisionPointRec(mouse, rect);
        DrawRectangleRec(rect, h ? hover : base);
        DrawRectangleLinesEx(rect, 2, Color{30, 22, 12, 255});
        const int fs = 18;
        const int tw = MeasureText(label, fs);
        DrawText(label, static_cast<int>(rect.x + rect.width / 2 - tw / 2),
                 static_cast<int>(rect.y + rect.height / 2 - fs / 2), fs, WHITE);
    };

    const char* rematchLbl = T(TK::RoundOverRematch, language);
    if (!onlineWinByDisconnect && onlineLobby.HasRematchTeamRoom()) {
        drawBtn(OnlineRematchBtnRect(), rematchLbl,
                Color{70, 140, 90, 255}, Color{95, 185, 110, 255});
    } else if (!onlineWinByDisconnect) {
        Rectangle rect = OnlineRematchBtnRect();
        DrawRectangleRec(rect, Color{55, 55, 55, 180});
        DrawRectangleLinesEx(rect, 2, Color{80, 80, 80, 255});
        const int fs = 18;
        const int tw = MeasureText(rematchLbl, fs);
        DrawText(rematchLbl, static_cast<int>(rect.x + rect.width / 2 - tw / 2),
                 static_cast<int>(rect.y + rect.height / 2 - fs / 2), fs, Color{140, 140, 140, 255});
    }
    const Rectangle lobbyRect = onlineWinByDisconnect
        ? Rectangle{ cfg::SCREEN_WIDTH / 2.0f - 100.0f, cfg::SCREEN_HEIGHT / 2.0f + 40.0f, 200.0f, 48.0f }
        : OnlineLobbyBtnRect();
    drawBtn(lobbyRect, T(TK::RoundOverBackToLobby, language),
            Color{90, 75, 55, 255}, Color{120, 100, 75, 255});
}

void Game::ExitSpectatorToLobby() {
    if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
    netMatch.LeaveMatch();
    isSpectating = false;
    onlineLobby.ReturnToLobbyAfterMatch();
    state = GameState::OnlineLobby;
}

void Game::DrawSpectatorBanner() const {
    auto joinTeamNames = [this](int start, int count) -> std::string {
        std::string joined;
        for (int i = 0; i < count; ++i) {
            const size_t idx = static_cast<size_t>(start + i);
            if (idx >= onlinePlayerNames.size()) break;
            const std::string& name = onlinePlayerNames[idx];
            if (!joined.empty()) joined += " & ";
            joined += name.empty() ? "???" : name;
        }
        return joined;
    };

    const std::string sideA = joinTeamNames(0, matchComposition.teamA);
    const std::string sideB = joinTeamNames(matchComposition.teamA, matchComposition.teamB);

    char buf[256];
    snprintf(buf, sizeof(buf), T(TK::OnlineSpectatingFmt, language),
             sideA.c_str(), sideB.c_str());
    int tw = MeasureText(buf, 16);
    DrawRectangle(cfg::SCREEN_WIDTH / 2 - tw / 2 - 12, 4, tw + 24, 26, Fade(BLACK, 0.55f));
    DrawText(buf, cfg::SCREEN_WIDTH / 2 - tw / 2, 10, 16, Color{235, 220, 180, 255});
}
