#include "Game.h"
#include "AssetPath.h"
#include "CannonUILayout.h"
#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"
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
#include <vector>

void Game::StartOnlineMatch(const MatchStart& ms) {
    mode = GameMode::Online;
    isSpectating = false;
    matchFormat = MatchFormat::Duel1v1;
    version = ms.isPlus ? GameVersion::Plus : GameVersion::Classic;
    onlineWinByDisconnect = false;
    onlineLobby.MarkInMatch(ms.matchId);
    onlineLobby.PauseRealtime();

    if (ms.myPlayerNumber == 1) {
        onlineP1Name = playerIdentity.DisplayName();
        onlineP2Name = ms.opponentName;
    } else {
        onlineP1Name = ms.opponentName;
        onlineP2Name = playerIdentity.DisplayName();
    }

    netMatch.Begin(ms.matchId, ms.myPlayerNumber, ms.opponentId, ms.opponentName);
    ResetRound(ms.terrainSeed);

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
            onlineLobby.AbandonActiveMatch(mid, netMatch.MyPlayerNumber());
        }
        netMatch.LeaveMatch();
    } else if (state == GameState::OnlineLobby) {
        onlineLobby.LeaveLobby();
    }
}

void Game::EndOnlineMatchOpponentLeft() {
    onlineWinByDisconnect = true;
    roundOutcome = (netMatch.MyPlayerNumber() == 1)
        ? RoundOutcome::P1Wins : RoundOutcome::P2Wins;
    onlineLobby.ReportMatchResult(true);
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
    const Cannon& active = GetCannon(shooterPlayer);
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
    currentPlayer = remote.shooterPlayer;

    Vector2 start = shooter.MuzzlePosition();
    remoteReplayPos = start;
    remoteReplayT = 0.0f;
    remoteReplayAimTimer = 0.25f;
    prevProjectilePos = start;
    guidedPathT = 0.0f;

    if (audioReady) PlaySound(sndFire);
    if (version == GameVersion::Plus) shooter.OnShotFired();
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
    currentPlayer = shot.shooterPlayer;
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    opponentAimHasLiveTarget = false;

    if (audioReady) PlaySound(sndFire);
    if (version == GameVersion::Plus) shooter.OnShotFired();
    state = GameState::RemoteProjectileLive;
    powerups.RemoteEffectApplied() = false;
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
            fin.t = 0.35f;
            fin.x = r.impactX;
            fin.y = r.impactY;
            remoteLiveSamples.push_back(fin);
            remoteLiveLastSeq = fin.seq;
        } else if (remoteLiveWatchTimer > 6.0f) {
            // Stream nunca veio — desiste e espera poll do turno (fallback).
            DebugLogf(LOG_WARNING, "GAME: stream live timeout sem amostras — volta a Aiming");
            remoteLiveActive = false;
            netMatch.ClearLiveShot();
            state = GameState::Aiming;
            return;
        } else {
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
    // Com resultado autoritativo (ou shot_end), NÃO segurar buffer delay —
    // senão playT nunca alcança o fim e deadlocks o adversário.
    const bool canFinishPath = streamEnded || remoteLiveHasPendingResult;
    if (!canFinishPath) {
        const float cap = std::max(0.0f, maxT - remoteLiveBufferDelay);
        if (targetPlay > cap) targetPlay = cap;
    } else if (targetPlay > maxT) {
        targetPlay = maxT;
    }
    remoteLivePlayT = targetPlay;

    Vector2 prev = remoteLivePos;
    if (remoteLivePlayT <= remoteLiveSamples.front().t) {
        remoteLivePos = { remoteLiveSamples.front().x, remoteLiveSamples.front().y };
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
                break;
            }
        }
    }

    Vector2 vel = { remoteLivePos.x - prev.x, remoteLivePos.y - prev.y };
    particles.EmitTrail(remoteLivePos, vel);

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

    constexpr float kDuration = 0.85f;
    remoteReplayT += dt / kDuration;
    float t = std::clamp(remoteReplayT, 0.0f, 1.0f);

    Vector2 prev = remoteReplayPos;
    remoteReplayPos.x = start.x + (end.x - start.x) * t;
    remoteReplayPos.y = start.y + (end.y - start.y) * t - std::sin(t * PI) * 80.0f;

    Vector2 vel = { remoteReplayPos.x - prev.x, remoteReplayPos.y - prev.y };
    particles.EmitTrail(remoteReplayPos, vel);

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

    // Power-up coletado no tiro remoto (autoritativo).
    if (version == GameVersion::Plus && remote.pickedPowerupType >= 0) {
        powerups.ApplyRemotePickup(remote.pickedPowerupType, remote.pickedPowerupX,
                                   remote.shooterPlayer, GetCannon(1), GetCannon(2),
                                   language, true, powerups.RemoteEffectApplied());
    }
    powerups.RemoteEffectApplied() = false;

    Cannon& shooter = GetCannon(remote.shooterPlayer);
    if (version == GameVersion::Plus) {
        shooter.OnShotResolved();
    }

    if (audioReady) PlaySound(sndExplosion);
    particles.EmitExplosion(impactPos, 50);
    terrain.Explode(impactPos.x, impactPos.y, remote.craterRadius);

    GetCannon(1).TakeDamage(remote.damageP1);
    GetCannon(2).TakeDamage(remote.damageP2);

    GetCannon(1).groundY = terrain.HeightAt(GetCannon(1).x);
    GetCannon(2).groundY = terrain.HeightAt(GetCannon(2).x);

    if (version == GameVersion::Plus) {
        effects.TriggerShake(cfg::SHAKE_MAGNITUDE_TERRAIN_PX, cfg::SHAKE_DURATION_TERRAIN_SEC);
    }

    windForce = remote.nextWind;
    currentPlayer = remote.nextTurnPlayer;
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;
    prevProjectilePos = {0, 0};
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    opponentAimForTurn = 0;
    opponentAimHasLiveTarget = false;

    if (remote.matchOver) {
        if (isSpectating) {
            roundOutcome = (remote.winnerPlayer == 0) ? RoundOutcome::Draw
                           : (remote.winnerPlayer == 1 ? RoundOutcome::P1Wins : RoundOutcome::P2Wins);
            state = GameState::RoundOver;
            stateTimer = 1.0f;
        } else {
            bool iWon = (remote.winnerPlayer == netMatch.MyPlayerNumber());
            bool draw = (remote.winnerPlayer == 0);
            if (draw) {
                roundOutcome = RoundOutcome::Draw;
            } else {
                roundOutcome = iWon
                    ? (netMatch.MyPlayerNumber() == 1 ? RoundOutcome::P1Wins : RoundOutcome::P2Wins)
                    : (netMatch.MyPlayerNumber() == 1 ? RoundOutcome::P2Wins : RoundOutcome::P1Wins);
            }
            if (!draw) onlineLobby.ReportMatchResult(iWon);
            onlineLobby.MarkIdle();
            state = GameState::RoundOver;
            stateTimer = 1.0f;
        }
    } else {
        OnOnlineTurnCompleted(currentPlayer);
        state = GameState::TurnTransition;
        stateTimer = 0.35f;
    }
}

void Game::OnOnlineTurnCompleted(int startingTurnPlayer) {
    onlineCompletedTurns++;
    if (version == GameVersion::Plus) {
        Cannon& startingCannon = GetCannon(startingTurnPlayer);
        startingCannon.OnTurnStarted();
        if (onlineCompletedTurns % cfg::POWERUP_SPAWN_EVERY_TURNS == 0) {
            powerups.MaybeSpawnSeeded(onlineSeed, onlineCompletedTurns);
        }
    }
}

void Game::ConsumeRemotePowerupPickups() {
    LivePowerupPickup pu;
    while (netMatch.PollRemotePowerupPickup(pu)) {
        int shooter = netMatch.SyncedCurrentTurnPlayer();
        powerups.ApplyRemotePickup(pu.type, pu.x, shooter, GetCannon(1), GetCannon(2),
                                   language, false, powerups.RemoteEffectApplied());
    }
}

void Game::ApplyTurnSilently(const RemoteTurnResult& turn) {
    if (version == GameVersion::Plus && turn.pickedPowerupType >= 0) {
        powerups.ApplyRemotePickup(turn.pickedPowerupType, turn.pickedPowerupX,
                                   turn.shooterPlayer, GetCannon(1), GetCannon(2),
                                   language, true, powerups.RemoteEffectApplied());
        powerups.RemoteEffectApplied() = false;
    }

    Cannon& shooter = GetCannon(turn.shooterPlayer);
    if (version == GameVersion::Plus) {
        shooter.OnShotResolved();
    }

    terrain.Explode(turn.impactX, turn.impactY, turn.craterRadius);
    GetCannon(1).TakeDamage(turn.damageP1);
    GetCannon(2).TakeDamage(turn.damageP2);
    GetCannon(1).groundY = terrain.HeightAt(GetCannon(1).x);
    GetCannon(2).groundY = terrain.HeightAt(GetCannon(2).x);

    windForce = turn.nextWind;
    currentPlayer = turn.nextTurnPlayer;
    OnOnlineTurnCompleted(turn.nextTurnPlayer);
}

void Game::StartSpectating(const ActiveMatchCard& match) {
    using json = nlohmann::json;
    SupabaseClient client;
    json matchRows = client.Select("matches",
        "select=terrain_seed,version,wind,current_turn_player,status,winner_player&id=eq." + match.matchId);
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

    json turnRows = client.Select("match_turns",
        "select=*&match_id=eq." + match.matchId + "&order=turn_number.asc&limit=200");

    isSpectating = true;
    mode = GameMode::Online;
    matchFormat = MatchFormat::Duel1v1;
    version = isPlus ? GameVersion::Plus : GameVersion::Classic;
    onlineWinByDisconnect = false;
    onlineLobby.PauseRealtime();

    onlineP1Name = match.player1Name;
    onlineP2Name = match.player2Name;

    ResetRound(seed);
    onlineSeed = seed;
    onlineCompletedTurns = 0;
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
    currentPlayer = (lastTurnNumber > 0) ? currentPlayer : currentTurn;

    netMatch.BeginSpectating(match.matchId, currentPlayer, lastTurnNumber);

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
                   : (winnerPlayer == 1 ? RoundOutcome::P1Wins : RoundOutcome::P2Wins);
    state = GameState::RoundOver;
    stateTimer = 1.0f;
}

void Game::ExitSpectatorToLobby() {
    if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
    netMatch.LeaveMatch();
    isSpectating = false;
    onlineLobby.EnterLobby();
    state = GameState::OnlineLobby;
}

void Game::DrawSpectatorBanner() const {
    char buf[128];
    snprintf(buf, sizeof(buf), T(TK::OnlineSpectatingFmt, language),
             onlineP1Name.c_str(), onlineP2Name.c_str());
    int tw = MeasureText(buf, 16);
    DrawRectangle(cfg::SCREEN_WIDTH / 2 - tw / 2 - 12, 4, tw + 24, 26, Fade(BLACK, 0.55f));
    DrawText(buf, cfg::SCREEN_WIDTH / 2 - tw / 2, 10, 16, Color{235, 220, 180, 255});
}
