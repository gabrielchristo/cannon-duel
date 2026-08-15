#include "NetMatch.h"
#include "NetWorker.h"
#include "JsonHelpers.h"
#include "../DebugLog.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>

using json = nlohmann::json;

namespace {
int JsonInt(const json& j, const char* key, int fallback) {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    const auto& v = j[key];
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number_float()) return static_cast<int>(v.get<double>());
    if (v.is_string()) {
        try { return std::stoi(v.get<std::string>()); } catch (...) { return fallback; }
    }
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    return fallback;
}

float JsonFloat(const json& j, const char* key, float fallback) {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    const auto& v = j[key];
    if (v.is_number()) return static_cast<float>(v.get<double>());
    if (v.is_string()) {
        try { return std::stof(v.get<std::string>()); } catch (...) { return fallback; }
    }
    return fallback;
}

bool JsonBool(const json& j, const char* key, bool fallback) {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    const auto& v = j[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number()) return v.get<double>() != 0.0;
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        return s == "true" || s == "t" || s == "1";
    }
    return fallback;
}

void AppendParsedPickups(RemoteTurnResult& turn, const json& source) {
    turn.pickedPowerups.clear();
    if (source.contains("picked_powerups") && source["picked_powerups"].is_array()) {
        for (const auto& item : source["picked_powerups"]) {
            RemoteTurnResult::PickedPowerupEntry entry;
            entry.type = JsonInt(item, "type", -1);
            entry.x = JsonFloat(item, "x", 0.0f);
            if (entry.type >= 0) turn.pickedPowerups.push_back(entry);
        }
    }
    if (turn.pickedPowerups.empty() && turn.pickedPowerupType >= 0) {
        turn.pickedPowerups.push_back({ turn.pickedPowerupType, turn.pickedPowerupX });
    }
}
} // namespace

bool NetMatch::IsMyTurn() const {
    if (myPlayerNumber == 0) return false;
    if (awaitingOpponentTurn_.load()) return false;
    std::lock_guard lock(mu_);
    if (hasPendingTurn_) return false;
    return syncedCurrentTurnPlayer == myPlayerNumber;
}

float NetMatch::PollIntervalSec() const {
    if (realtime_.IsConnected()) return POLL_INTERVAL_REALTIME_SEC;
    if (awaitingOpponentTurn_.load()) return POLL_INTERVAL_AWAITING_SEC;
    return POLL_INTERVAL_FALLBACK_SEC;
}

RemoteTurnResult NetMatch::ParseTurnJson(const json& row) {
    RemoteTurnResult turn;
    turn.turnNumber = JsonInt(row, "turn_number", 0);
    turn.shooterPlayer = JsonInt(row, "shooter_player", 1);
    turn.shootAngle = JsonFloat(row, "shoot_angle", 45.0f);
    turn.shootPower = JsonFloat(row, "shoot_power", 0.5f);
    turn.windAtShot = JsonFloat(row, "wind_at_shot", 0.0f);
    turn.impactX = JsonFloat(row, "impact_x", 0.0f);
    turn.impactY = JsonFloat(row, "impact_y", 0.0f);
    turn.craterRadius = JsonFloat(row, "crater_radius", 0.0f);
    for (int i = 0; i < MatchRoster::kMaxCannons; ++i) {
        const std::string dmgKey = "damage_p" + std::to_string(i + 1);
        turn.damages[i] = JsonFloat(row, dmgKey.c_str(), 0.0f);
        const std::string hpKey = "health_after_p" + std::to_string(i + 1);
        if (row.contains(hpKey)) {
            turn.healthAfter[i] = JsonFloat(row, hpKey.c_str(), cfg::CANNON_MAX_HEALTH);
            turn.healthAfterCount = i + 1;
            turn.hasHealthAfter = true;
        }
    }
    turn.nextWind = JsonFloat(row, "next_wind", 0.0f);
    turn.nextTurnPlayer = JsonInt(row, "next_turn_player", 1);
    turn.matchOver = JsonBool(row, "match_over", false);
    turn.winnerPlayer = JsonInt(row, "winner_player", 0);
    turn.pickedPowerupType = JsonInt(row, "picked_powerup_type", -1);
    turn.pickedPowerupX = JsonFloat(row, "picked_powerup_x", 0.0f);
    turn.spawnPowerupType = JsonInt(row, "spawn_powerup_type", -1);
    turn.spawnPowerupX = JsonFloat(row, "spawn_powerup_x", 0.0f);
    AppendParsedPickups(turn, row);
    return turn;
}

RemoteTurnResult NetMatch::ParseTurnPayload(const json& payload) {
    RemoteTurnResult turn;
    turn.turnNumber = JsonInt(payload, "turn_number", 0);
    turn.shooterPlayer = JsonInt(payload, "shooter", 1);
    turn.shootAngle = JsonFloat(payload, "shoot_angle", 45.0f);
    turn.shootPower = JsonFloat(payload, "shoot_power", 0.5f);
    turn.windAtShot = JsonFloat(payload, "wind_at_shot", 0.0f);
    turn.impactX = JsonFloat(payload, "impact_x", 0.0f);
    turn.impactY = JsonFloat(payload, "impact_y", 0.0f);
    turn.craterRadius = JsonFloat(payload, "crater_radius", 0.0f);
    turn.nextWind = JsonFloat(payload, "next_wind", 0.0f);
    turn.nextTurnPlayer = JsonInt(payload, "next_turn_player", 1);
    turn.matchOver = JsonBool(payload, "match_over", false);
    turn.winnerPlayer = JsonInt(payload, "winner_player", 0);
    turn.pickedPowerupType = JsonInt(payload, "picked_powerup_type", -1);
    turn.pickedPowerupX = JsonFloat(payload, "picked_powerup_x", 0.0f);
    turn.spawnPowerupType = JsonInt(payload, "spawn_powerup_type", -1);
    turn.spawnPowerupX = JsonFloat(payload, "spawn_powerup_x", 0.0f);

    if (payload.contains("damages") && payload["damages"].is_array()) {
        const json& arr = payload["damages"];
        for (int i = 0; i < MatchRoster::kMaxCannons && i < static_cast<int>(arr.size()); ++i) {
            turn.damages[i] = arr[i].is_number() ? arr[i].get<float>() : 0.0f;
        }
    }
    if (payload.contains("health_after") && payload["health_after"].is_array()) {
        const json& arr = payload["health_after"];
        turn.healthAfterCount = static_cast<int>(arr.size());
        turn.hasHealthAfter = turn.healthAfterCount > 0;
        for (int i = 0; i < MatchRoster::kMaxCannons && i < turn.healthAfterCount; ++i) {
            turn.healthAfter[i] = arr[i].is_number() ? arr[i].get<float>() : cfg::CANNON_MAX_HEALTH;
        }
    }
    AppendParsedPickups(turn, payload);
    return turn;
}

bool NetMatch::TryEnqueueRemoteTurn(const RemoteTurnResult& turn) {
    if (turn.turnNumber <= 0) return false;

    if (myPlayerNumber != 0 && turn.shooterPlayer == myPlayerNumber) {
        std::lock_guard lock(mu_);
        if (turn.turnNumber > lastSeenTurnNumber) lastSeenTurnNumber = turn.turnNumber;
        return false;
    }

    std::lock_guard lock(mu_);
    if (turn.turnNumber <= lastSeenTurnNumber) {
        if (turn.spawnPowerupType >= 0 && turn.turnNumber == lastSeenTurnNumber) {
            pendingSpawnResync_ = turn;
            hasPendingSpawnResync_ = true;
            DebugLogf(LOG_INFO, "NET: turno #%d — re-sync de spawn power-up", turn.turnNumber);
        }
        if (turn.hasHealthAfter && turn.turnNumber == lastSeenTurnNumber) {
            pendingHealthResync_ = turn;
            hasPendingHealthResync_ = true;
            DebugLogf(LOG_INFO, "NET: turno #%d — re-sync de vida autoritativa", turn.turnNumber);
        }
        return false;
    }
    if (turn.turnNumber != lastSeenTurnNumber + 1) {
        DebugLogf(LOG_WARNING, "NET: turno #%d fora de ordem (esperado #%d)",
                  turn.turnNumber, lastSeenTurnNumber + 1);
        return false;
    }
    if (hasPendingTurn_ && pendingTurn_.turnNumber == turn.turnNumber) {
        if (turn.hasHealthAfter && !pendingTurn_.hasHealthAfter) {
            pendingTurn_ = turn;
            DebugLogf(LOG_INFO, "NET: turno #%d atualizado com vida autoritativa", turn.turnNumber);
        } else if (turn.spawnPowerupType >= 0 && pendingTurn_.spawnPowerupType < 0) {
            pendingTurn_.spawnPowerupType = turn.spawnPowerupType;
            pendingTurn_.spawnPowerupX = turn.spawnPowerupX;
            DebugLogf(LOG_INFO, "NET: turno #%d atualizado com spawn power-up", turn.turnNumber);
        }
        return false;
    }
    if (hasPendingTurn_ && pendingTurn_.turnNumber >= turn.turnNumber) return false;

    pendingTurn_ = turn;
    hasPendingTurn_ = true;
    DebugLogf(LOG_INFO, "NET: turno remoto #%d enfileirado (atirador P%d hp=%d)",
              turn.turnNumber, turn.shooterPlayer, turn.hasHealthAfter ? 1 : 0);
    return true;
}

void NetMatch::ApplyTurnRecord(const json& row) {
    RemoteTurnResult turn = ParseTurnJson(row);
    TryEnqueueRemoteTurn(turn);
}

void NetMatch::ApplyLiveAimFromRecord(const json& row) {
    const int shooter = JsonInt(row, "live_shooter", 0);
    if (shooter == 0 || shooter == myPlayerNumber) return;

    LiveAimState aim;
    aim.player = shooter;
    aim.angleDeg = JsonFloat(row, "live_angle", 45.0f);
    aim.power01 = JsonFloat(row, "live_power", 0.0f);
    aim.phase = row.value("live_aim_phase", "angle");
    aim.valid = true;

    std::lock_guard lock(mu_);
    opponentAim_ = aim;
}

void NetMatch::ApplyMatchRecord(const json& row) {
    const int currentTurnPlayer = JsonInt(row, "current_turn_player", 1);
    const std::string status = row.value("status", "active");
    if (myPlayerNumber == 0) {
        if (status == "finished" || status == "abandoned") {
            std::lock_guard lock(mu_);
            if (!spectatorMatchEnded_) {
                spectatorMatchEnded_ = true;
                spectatorWinner_ = JsonInt(row, "winner_player", 0);
            }
        }
        syncedCurrentTurnPlayer = currentTurnPlayer;
        ApplyLiveAimFromRecord(row);
        return;
    }
    {
        std::lock_guard lock(mu_);
        cachedCurrentTurnPlayer_ = currentTurnPlayer;
        if (status == "abandoned" && !disconnectReported_) {
            const int winner = JsonInt(row, "winner_player", 0);
            pendingAbandonWinner_ = winner;
            pendingDisconnect_ = OnlineDidIWin(winner, myPlayerNumber, composition_)
                ? DisconnectResult::OpponentLeft
                : DisconnectResult::MatchAbandoned;
        }
    }
    syncedCurrentTurnPlayer = currentTurnPlayer;
    ApplyLiveAimFromRecord(row);
}

void NetMatch::SignalParticipantLeft(int leavingPlayerNum) {
    if (myPlayerNumber == 0 || leavingPlayerNum <= 0) return;
    const int winner = WinnerWhenPlayerLeaves(leavingPlayerNum, composition_);
    if (winner == 0) return;

    std::lock_guard lock(mu_);
    if (disconnectReported_ || pendingDisconnect_ != DisconnectResult::None) return;
    pendingAbandonWinner_ = winner;
    pendingDisconnect_ = OnlineDidIWin(winner, myPlayerNumber, composition_)
        ? DisconnectResult::OpponentLeft
        : DisconnectResult::MatchAbandoned;
    DebugLogf(LOG_INFO, "NET: jogador P%d saiu — vencedor=%d eu=P%d",
              leavingPlayerNum, winner, myPlayerNumber);
}

void NetMatch::CheckParticipantsPresence(SupabaseClient& client, const json& matchRow) {
    if (myPlayerNumber == 0 || matchId.empty()) return;
    if (json_helpers::Str(matchRow, "status", "active") != "active") return;

    const MatchComposition comp = ParseMatchComposition(
        json_helpers::Str(matchRow, "match_format", "duel_1v1"),
        json_helpers::Int(matchRow, "team_a_count", 0),
        json_helpers::Int(matchRow, "team_b_count", 0));
    const int participantCount = std::max(comp.TotalPlayers(), composition_.TotalPlayers());
    std::vector<std::pair<int, std::string>> participants;
    participants.reserve(static_cast<size_t>(participantCount));
    for (int p = 1; p <= participantCount; ++p) {
        const std::string key = "player" + std::to_string(p) + "_id";
        const std::string pid = json_helpers::Str(matchRow, key.c_str());
        if (!pid.empty()) participants.emplace_back(p, pid);
    }
    if (participants.empty()) return;

    std::string inList;
    for (size_t i = 0; i < participants.size(); ++i) {
        if (i > 0) inList += ",";
        inList += participants[i].second;
    }

    const std::time_t cutoff = std::time(nullptr) - PARTICIPANT_STALE_SEC;
    std::tm t {};
#if defined(_WIN32)
    gmtime_s(&t, &cutoff);
#else
    gmtime_r(&cutoff, &t);
#endif
    std::ostringstream cutoffOss;
    cutoffOss << std::put_time(&t, "%Y-%m-%dT%H:%M:%SZ");
    const std::string cutoffIso = cutoffOss.str();

    json prows = client.Select("lobby_presence",
        "select=player_id,status,match_id,last_seen&player_id=in.(" + inList + ")");
    if (!prows.is_array()) return;

    std::unordered_map<std::string, json> byId;
    for (const auto& row : prows) {
        byId[json_helpers::Str(row, "player_id")] = row;
    }

    for (const auto& [playerNum, pid] : participants) {
        if (playerNum == myPlayerNumber) continue;

        auto it = byId.find(pid);
        bool live = false;
        if (it != byId.end()) {
            const json& row = it->second;
            const std::string status = json_helpers::Str(row, "status");
            const std::string mid = json_helpers::Str(row, "match_id");
            const std::string lastSeen = json_helpers::Str(row, "last_seen");
            live = (status == "in_match" && mid == matchId
                    && !lastSeen.empty() && lastSeen >= cutoffIso);
        }

        if (!live) {
            DebugLogf(LOG_INFO, "NET: P%d (%s) ausente da partida %s",
                      playerNum, pid.c_str(), matchId.c_str());
            client.Update("matches", "id=eq." + matchId + "&status=eq.active", {
                { "status", "abandoned" },
                { "winner_player", WinnerWhenPlayerLeaves(playerNum, comp) }
            });
            SignalParticipantLeft(playerNum);
            return;
        }
    }
}

void NetMatch::ApplyBroadcast(const json& envelope) {
    const std::string event = envelope.value("event", "");
    const json& p = envelope.contains("payload") ? envelope["payload"] : envelope;
    if (!p.is_object()) return;

    if (event == "aim") {
        const int player = JsonInt(p, "player", 0);
        if (player == 0 || player == myPlayerNumber) return;
        LiveAimState aim;
        aim.player = player;
        aim.angleDeg = JsonFloat(p, "angle", 45.0f);
        aim.power01 = JsonFloat(p, "power", 0.0f);
        aim.phase = p.value("phase", "angle");
        aim.valid = true;
        std::lock_guard lock(mu_);
        opponentAim_ = aim;
        return;
    }

    if (event == "shot_fired") {
        const int player = JsonInt(p, "player", 0);
        if (player == 0 || player == myPlayerNumber) return;
        LiveShotStart start;
        start.shotId = JsonInt(p, "shot_id", 0);
        start.shooterPlayer = player;
        start.angleDeg = JsonFloat(p, "angle", 45.0f);
        start.power01 = JsonFloat(p, "power", 0.5f);
        start.wind = JsonFloat(p, "wind", 0.0f);
        start.muzzleX = JsonFloat(p, "mx", 0.0f);
        start.muzzleY = JsonFloat(p, "my", 0.0f);
        std::lock_guard lock(mu_);
        liveShotId_ = start.shotId;
        liveSamples_.clear();
        liveShotEnded_ = false;
        pendingLiveShot_ = start;
        pendingLiveShotStart_ = true;
        // amostra inicial = muzzle
        liveSamples_.push_back(ProjSample{ 0, 0.0f, start.muzzleX, start.muzzleY });
        DebugLogf(LOG_INFO, "NET: shot_fired remoto P%d id=%d", player, start.shotId);
        return;
    }

    if (event == "proj") {
        const int player = JsonInt(p, "player", 0);
        if (player == 0 || player == myPlayerNumber) return;
        const int shotId = JsonInt(p, "shot_id", 0);
        ProjSample s;
        s.seq = JsonInt(p, "seq", 0);
        s.t = JsonFloat(p, "t", 0.0f);
        s.x = JsonFloat(p, "x", 0.0f);
        s.y = JsonFloat(p, "y", 0.0f);
        std::lock_guard lock(mu_);
        if (shotId != 0 && liveShotId_ != 0 && shotId != liveShotId_) return;
        if (shotId != 0) liveShotId_ = shotId;
        if (!liveSamples_.empty() && s.seq <= liveSamples_.back().seq) return;
        liveSamples_.push_back(s);
        while (liveSamples_.size() > MAX_LIVE_SAMPLES) liveSamples_.pop_front();
        return;
    }

    if (event == "shot_end") {
        const int player = JsonInt(p, "player", 0);
        if (player == 0 || player == myPlayerNumber) return;
        const int shotId = JsonInt(p, "shot_id", 0);
        std::lock_guard lock(mu_);
        if (shotId != 0 && liveShotId_ != 0 && shotId != liveShotId_) return;
        liveShotEnded_ = true;
        liveShotEndX_ = JsonFloat(p, "x", 0.0f);
        liveShotEndY_ = JsonFloat(p, "y", 0.0f);
        ProjSample s;
        s.seq = liveSamples_.empty() ? 1 : liveSamples_.back().seq + 1;
        s.t = liveSamples_.empty() ? 0.0f : liveSamples_.back().t + 0.02f;
        s.x = liveShotEndX_;
        s.y = liveShotEndY_;
        if (JsonFloat(p, "t", -1.0f) >= 0.0f) s.t = JsonFloat(p, "t", s.t);
        liveSamples_.push_back(s);
        return;
    }

    if (event == "turn_result") {
        const int shooter = JsonInt(p, "shooter", 0);
        if (shooter == 0 || shooter == myPlayerNumber) return;
        RemoteTurnResult turn = ParseTurnPayload(p);
        TryEnqueueRemoteTurn(turn);
        return;
    }

    if (event == "powerup_spawn") {
        const int shooter = JsonInt(p, "shooter", 0);
        if (shooter == 0 || shooter == myPlayerNumber) return;
        LivePowerupSpawn sp;
        sp.type = JsonInt(p, "type", -1);
        sp.x = JsonFloat(p, "x", 0.0f);
        sp.turnNumber = JsonInt(p, "turn", 0);
        sp.valid = sp.type >= 0;
        if (!sp.valid) return;
        std::lock_guard lock(mu_);
        pendingPowerupSpawns_.push_back(sp);
        return;
    }

    if (event == "powerup_picked") {
        const int player = JsonInt(p, "player", 0);
        if (player == 0 || player == myPlayerNumber) return;
        LivePowerupPickup pu;
        pu.player = player;
        pu.type = JsonInt(p, "type", -1);
        pu.x = JsonFloat(p, "x", 0.0f);
        pu.valid = pu.type >= 0;
        if (!pu.valid) return;
        std::lock_guard lock(mu_);
        pendingPowerupPickups_.push_back(pu);
        return;
    }

    if (event == "dev_cmd") {
        DevCommand cmd;
        cmd.action = p.value("action", "");
        cmd.player = JsonInt(p, "player", 0);
        cmd.type = JsonInt(p, "type", -1);
        cmd.x = JsonFloat(p, "x", 0.0f);
        cmd.value = JsonFloat(p, "value", 0.0f);
        cmd.valid = !cmd.action.empty();
        if (!cmd.valid) return;
        std::lock_guard lock(mu_);
        pendingDevCommand_ = cmd;
        hasPendingDevCommand_ = true;
        DebugLogf(LOG_INFO, "NET: dev_cmd remoto action=%s", cmd.action.c_str());
        return;
    }
}

void NetMatch::PublishShotFired(float angleDeg, float power01, float wind,
                                float muzzleX, float muzzleY) {
    if (!active_.load() || !realtime_.IsConnected()) return;
    localShotId_++;
    localProjSeq_ = 0;
    localShotFlightT_ = 0.0f;
    projPublishTimer_ = 0.0f;
    realtime_.SendBroadcast("shot_fired", {
        { "player", myPlayerNumber },
        { "shot_id", localShotId_ },
        { "angle", angleDeg },
        { "power", power01 },
        { "wind", wind },
        { "mx", muzzleX },
        { "my", muzzleY }
    });
    // Primeira amostra imediata
    realtime_.SendBroadcast("proj", {
        { "player", myPlayerNumber },
        { "shot_id", localShotId_ },
        { "seq", 0 },
        { "t", 0.0f },
        { "x", muzzleX },
        { "y", muzzleY }
    });
}

void NetMatch::PublishProjectileSample(float dt, float x, float y) {
    if (!active_.load() || !realtime_.IsConnected()) return;
    if (localShotId_ <= 0) return;
    localShotFlightT_ += dt;
    projPublishTimer_ += dt;
    if (projPublishTimer_ < PROJ_PUBLISH_INTERVAL_SEC) return;
    projPublishTimer_ = 0.0f;
    localProjSeq_++;
    // Sem coalesce — cada amostra importa para o path.
    realtime_.SendBroadcast("proj", {
        { "player", myPlayerNumber },
        { "shot_id", localShotId_ },
        { "seq", localProjSeq_ },
        { "t", localShotFlightT_ },
        { "x", x },
        { "y", y }
    });
}

void NetMatch::PublishShotEnded(float x, float y) {
    if (!active_.load() || !realtime_.IsConnected()) return;
    if (localShotId_ <= 0) return;
    localProjSeq_++;
    realtime_.SendBroadcast("shot_end", {
        { "player", myPlayerNumber },
        { "shot_id", localShotId_ },
        { "seq", localProjSeq_ },
        { "t", localShotFlightT_ },
        { "x", x },
        { "y", y }
    });
}

bool NetMatch::PollLiveShotStart(LiveShotStart& out) {
    std::lock_guard lock(mu_);
    if (!pendingLiveShotStart_) return false;
    out = pendingLiveShot_;
    pendingLiveShotStart_ = false;
    return true;
}

int NetMatch::PullProjectileSamples(int afterSeq, std::vector<ProjSample>& out) {
    out.clear();
    std::lock_guard lock(mu_);
    for (const auto& s : liveSamples_) {
        if (s.seq > afterSeq) out.push_back(s);
    }
    return static_cast<int>(out.size());
}

bool NetMatch::PeekShotEnded(float& outX, float& outY) const {
    std::lock_guard lock(mu_);
    if (!liveShotEnded_) return false;
    outX = liveShotEndX_;
    outY = liveShotEndY_;
    return true;
}

void NetMatch::ClearLiveShot() {
    std::lock_guard lock(mu_);
    pendingLiveShotStart_ = false;
    liveSamples_.clear();
    liveShotEnded_ = false;
    liveShotId_ = 0;
}

void NetMatch::Begin(const std::string& id, int myPlayerNum,
                      const std::string& oppId, const std::string& oppName,
                      const MatchComposition& composition) {
    matchId = id;
    myPlayerNumber = myPlayerNum;
    composition_ = composition;
    opponentId = oppId;
    opponentName = oppName;
    syncedCurrentTurnPlayer = 1;
    cachedCurrentTurnPlayer_ = 1;
    lastSeenTurnNumber = 0;
    pollTimer = 0.0f;
    presenceGraceRemaining_ = (myPlayerNum != 0) ? MATCH_START_PRESENCE_GRACE_SEC : 0.0f;
    liveAimPublishTimer_ = 0.0f;
    projPublishTimer_ = 0.0f;
    localShotFlightT_ = 0.0f;
    localShotId_ = 0;
    localProjSeq_ = 0;
    pollInFlight_.store(false);
    awaitingOpponentTurn_.store(false);
    active_.store(true);

    {
        std::lock_guard lock(mu_);
        hasPendingTurn_ = false;
        hasPendingHealthResync_ = false;
        pendingHealthResync_ = {};
        pendingDisconnect_ = DisconnectResult::None;
        pendingAbandonWinner_ = 0;
        disconnectReported_ = false;
        opponentAim_ = {};
        pendingLiveShotStart_ = false;
        liveSamples_.clear();
        liveShotEnded_ = false;
        liveShotId_ = 0;
        pendingPowerupPickups_.clear();
        pendingPowerupSpawns_.clear();
        hasPendingSpawnResync_ = false;
        pendingSpawnResync_ = {};
        spectatorMatchEnded_ = false;
        spectatorEndReported_ = false;
        spectatorWinner_ = 0;
    }

    DebugLogf(LOG_INFO, "NET: Begin match=%s eu=P%d adversario=%s",
              matchId.c_str(), myPlayerNumber, opponentName.c_str());

    realtime_.SetOnPostgres("match_turns", [this](const json& env) {
        const std::string type = env.value("type", "");
        if (!type.empty() && type.find("INSERT") == std::string::npos) return;
        if (env.contains("record")) ApplyTurnRecord(env["record"]);
    });
    realtime_.SetOnPostgres("matches", [this](const json& env) {
        if (env.contains("record")) ApplyMatchRecord(env["record"]);
    });
    realtime_.SetOnBroadcast([this](const json& env) {
        ApplyBroadcast(env);
    });

    std::vector<RealtimeClient::PostgresSub> subs = {
        { "INSERT", "public", "match_turns", "match_id=eq." + matchId },
        { "UPDATE", "public", "matches", "id=eq." + matchId },
    };
    realtime_.Start("match:" + matchId, subs);

    SchedulePoll();
}

void NetMatch::BeginSpectating(const std::string& id, int currentTurnPlayer, int lastTurnNumber,
                               const MatchComposition& composition) {
    Begin(id, 0, "", "", composition);
    syncedCurrentTurnPlayer = currentTurnPlayer;
    cachedCurrentTurnPlayer_ = currentTurnPlayer;
    lastSeenTurnNumber = lastTurnNumber;
    {
        std::lock_guard lock(mu_);
        spectatorMatchEnded_ = false;
        spectatorEndReported_ = false;
        spectatorWinner_ = 0;
    }
    DebugLogf(LOG_INFO, "NET: espectador match=%s turnos=%d vez=P%d",
              id.c_str(), lastTurnNumber, currentTurnPlayer);
}

void NetMatch::FlushOutgoing() {
    if (!active_.load() || matchId.empty()) return;
    realtime_.Drain();
}

void NetMatch::Pump(float dt) {
    if (!active_.load() || matchId.empty()) return;

    realtime_.Drain();

    if (realtime_.ConsumeJoined()) {
        DebugLogf(LOG_INFO, "NET: Realtime joined — resync HTTP unico");
        pollTimer = 0.0f;
        SchedulePoll();
    }

    {
        std::lock_guard lock(mu_);
        syncedCurrentTurnPlayer = cachedCurrentTurnPlayer_;
    }

    pollTimer += dt;
    if (presenceGraceRemaining_ > 0.0f) {
        presenceGraceRemaining_ = std::max(0.0f, presenceGraceRemaining_ - dt);
    }
    if (pollTimer >= PollIntervalSec()) {
        pollTimer = 0.0f;
        SchedulePoll();
    }
}

void NetMatch::PublishLiveAim(float dt, float angleDeg, float power01, const char* phase) {
    if (!active_.load() || matchId.empty()) return;
    if (!IsMyTurn()) return;

    const char* ph = phase ? phase : "angle";
    liveAimPublishTimer_ += dt;

    json payload = {
        { "player", myPlayerNumber },
        { "angle", angleDeg },
        { "power", power01 },
        { "phase", ph }
    };

    if (realtime_.IsConnected()) {
        if (liveAimPublishTimer_ < LIVE_AIM_PUBLISH_INTERVAL_SEC) return;
        liveAimPublishTimer_ = 0.0f;
        realtime_.SendBroadcast("aim", payload, "aim");
        return;
    }

    if (liveAimPublishTimer_ < LIVE_AIM_HTTP_INTERVAL_SEC) return;
    liveAimPublishTimer_ = 0.0f;

    const std::string matchIdCopy = matchId;
    const int me = myPlayerNumber;
    GlobalNetWorker().PostCoalesced("live_aim", [=](SupabaseClient& client) {
        client.Update("matches", "id=eq." + matchIdCopy, {
            { "live_shooter", me },
            { "live_angle", angleDeg },
            { "live_power", power01 },
            { "live_aim_phase", ph }
        });
    });
}

LiveAimState NetMatch::GetOpponentLiveAim() const {
    std::lock_guard lock(mu_);
    return opponentAim_;
}

void NetMatch::SchedulePoll() {
    if (!active_.load() || matchId.empty()) return;
    if (pollInFlight_.exchange(true)) return;

    const std::string matchIdCopy = matchId;
    const int nextTurn = lastSeenTurnNumber + 1;
    // Presença é HTTP extra. Com Realtime o CDC de matches já cobre
    // abandono; sem Realtime, não misturar no poll urgente do turno.
    const bool checkPresence = (presenceGraceRemaining_ <= 0.0f)
        && !(awaitingOpponentTurn_.load() && !realtime_.IsConnected());

    GlobalNetWorker().Post([this, matchIdCopy, nextTurn, checkPresence](SupabaseClient& client) {
        json matchRows = client.Select("matches",
            "select=status,winner_player,current_turn_player,match_format,team_a_count,team_b_count,"
            "player1_id,player2_id,player3_id,player4_id,player5_id,player6_id,player7_id,player8_id,player9_id,player10_id,"
            "live_shooter,live_angle,live_power,live_aim_phase&id=eq." + matchIdCopy);

        if (matchRows.is_array() && !matchRows.empty()) {
            const json& mrow = matchRows[0];
            if (json_helpers::Str(mrow, "status", "active") == "active" && checkPresence) {
                CheckParticipantsPresence(client, mrow);
            }
            ApplyMatchRecord(mrow);
        }

        json rows = client.Select("match_turns",
            "select=*&match_id=eq." + matchIdCopy +
            "&turn_number=eq." + std::to_string(nextTurn) +
            "&limit=1");
        if (rows.is_array() && !rows.empty()) {
            ApplyTurnRecord(rows[0]);
        }

        pollInFlight_.store(false);
    });
}

void NetMatch::PublishPowerupPicked(int type, float x) {
    if (!active_.load() || !realtime_.IsConnected()) return;
    if (type < 0) return;
    realtime_.SendBroadcast("powerup_picked", {
        { "player", myPlayerNumber },
        { "type", type },
        { "x", x }
    });
}

bool NetMatch::PollRemotePowerupSpawn(LivePowerupSpawn& out) {
    std::lock_guard lock(mu_);
    if (pendingPowerupSpawns_.empty()) return false;
    out = pendingPowerupSpawns_.front();
    pendingPowerupSpawns_.pop_front();
    return out.valid;
}

void NetMatch::PublishPowerupSpawn(int type, float x, int turnNumber) {
    if (!active_.load() || !realtime_.IsConnected()) return;
    if (type < 0) return;
    realtime_.SendBroadcast("powerup_spawn", {
        { "shooter", myPlayerNumber },
        { "type", type },
        { "x", x },
        { "turn", turnNumber }
    });
}

bool NetMatch::PollRemotePowerupPickup(LivePowerupPickup& out) {
    std::lock_guard lock(mu_);
    if (pendingPowerupPickups_.empty()) return false;
    out = pendingPowerupPickups_.front();
    pendingPowerupPickups_.pop_front();
    return out.valid;
}

void NetMatch::PublishDevCommand(const std::string& action, int player, int type,
                                 float x, float value) {
    if (!active_.load() || !realtime_.IsConnected()) return;
    if (action.empty()) return;
    realtime_.SendBroadcast("dev_cmd", {
        { "action", action },
        { "player", player },
        { "type", type },
        { "x", x },
        { "value", value }
    });
    DebugLogf(LOG_INFO, "NET: dev_cmd enviado action=%s", action.c_str());
}

bool NetMatch::PollDevCommand(DevCommand& out) {
    std::lock_guard lock(mu_);
    if (!hasPendingDevCommand_) return false;
    out = pendingDevCommand_;
    hasPendingDevCommand_ = false;
    pendingDevCommand_ = {};
    return out.valid;
}

void NetMatch::DevSyncTurnTo(int nextPlayer) {
    SyncTurnTo(nextPlayer);
}

void NetMatch::SyncTurnTo(int nextPlayer) {
    if (!active_.load() || matchId.empty()) return;
    const int maxPlayer = composition_.TotalPlayers();
    if (nextPlayer < 1 || nextPlayer > maxPlayer) return;

    syncedCurrentTurnPlayer = nextPlayer;
    awaitingOpponentTurn_.store(false);
    {
        std::lock_guard lock(mu_);
        cachedCurrentTurnPlayer_ = nextPlayer;
        opponentAim_ = {};
    }

    const std::string matchIdCopy = matchId;
    GlobalNetWorker().Post([matchIdCopy, nextPlayer](SupabaseClient& client) {
        json body = { { "current_turn_player", nextPlayer } };
        client.Update("matches", "id=eq." + matchIdCopy, body);
    });
}

void NetMatch::SubmitMyTurn(float shootAngle, float shootPower, float windAtShot,
                             float impactX, float impactY, float craterRadius,
                             const float damages[MatchRoster::kMaxCannons],
                             const float healthAfter[MatchRoster::kMaxCannons],
                             float nextWind, int nextTurnPlayer,
                             bool matchOver, int winnerPlayer,
                             const std::vector<ShotPowerupPickup>& pickups,
                             int spawnPowerupType, float spawnPowerupX) {
    lastSeenTurnNumber++;

    syncedCurrentTurnPlayer = nextTurnPlayer;
    {
        std::lock_guard lock(mu_);
        cachedCurrentTurnPlayer_ = nextTurnPlayer;
        opponentAim_ = {};
    }

    awaitingOpponentTurn_.store(!matchOver);

    const std::string matchIdCopy = matchId;
    const int turnNum = lastSeenTurnNumber;
    const int shooter = myPlayerNumber;
    const int playerCount = std::clamp(composition_.TotalPlayers(), 1, MatchRoster::kMaxCannons);
    const int pickedPowerupType = pickups.empty() ? -1 : pickups.back().type;
    const float pickedPowerupX = pickups.empty() ? 0.0f : pickups.back().x;

    DebugLogf(LOG_INFO, "NET: SubmitMyTurn #%d atirador=P%d proximo=P%d pickups=%zu spawn=%d@%.0f",
              turnNum, shooter, nextTurnPlayer, pickups.size(), spawnPowerupType, spawnPowerupX);

    json pickupsArr = json::array();
    for (const auto& pu : pickups) {
        pickupsArr.push_back({ { "type", pu.type }, { "x", pu.x } });
    }

    if (realtime_.IsConnected()) {
        json damagesArr = json::array();
        json healthArr = json::array();
        for (int i = 0; i < playerCount; ++i) {
            damagesArr.push_back(damages[i]);
            healthArr.push_back(healthAfter[i]);
        }
        realtime_.SendBroadcast("turn_result", {
            { "turn_number", turnNum },
            { "shooter", shooter },
            { "player_count", playerCount },
            { "shoot_angle", shootAngle },
            { "shoot_power", shootPower },
            { "wind_at_shot", windAtShot },
            { "impact_x", impactX },
            { "impact_y", impactY },
            { "crater_radius", craterRadius },
            { "damages", damagesArr },
            { "health_after", healthArr },
            { "next_wind", nextWind },
            { "next_turn_player", nextTurnPlayer },
            { "match_over", matchOver },
            { "winner_player", winnerPlayer },
            { "picked_powerup_type", pickedPowerupType },
            { "picked_powerup_x", pickedPowerupX },
            { "picked_powerups", pickupsArr },
            { "spawn_powerup_type", spawnPowerupType },
            { "spawn_powerup_x", spawnPowerupX }
        });
    }

    GlobalNetWorker().Post([=](SupabaseClient& client) {
        json body = {
            { "match_id", matchIdCopy },
            { "turn_number", turnNum },
            { "shooter_player", shooter },
            { "shoot_angle", shootAngle },
            { "shoot_power", shootPower },
            { "wind_at_shot", windAtShot },
            { "impact_x", impactX },
            { "impact_y", impactY },
            { "crater_radius", craterRadius },
            { "next_wind", nextWind },
            { "next_turn_player", nextTurnPlayer },
            { "match_over", matchOver },
            { "winner_player", winnerPlayer },
            { "picked_powerup_type", pickedPowerupType },
            { "picked_powerup_x", pickedPowerupX },
            { "spawn_powerup_type", spawnPowerupType },
            { "spawn_powerup_x", spawnPowerupX }
        };
        for (int i = 0; i < playerCount; ++i) {
            body["damage_p" + std::to_string(i + 1)] = damages[i];
        }
        json inserted = client.Insert("match_turns", body);
        if (!inserted.is_array() || inserted.empty()) {
            DebugLogf(LOG_WARNING, "NET: Insert match_turns #%d FALHOU", turnNum);
        }

        json matchUpdate = {
            { "current_turn_player", nextTurnPlayer },
            { "wind", nextWind },
            { "status", matchOver ? "finished" : "active" },
            { "live_shooter", 0 },
            { "live_aim_phase", "idle" }
        };
        if (matchOver) matchUpdate["winner_player"] = winnerPlayer;
        client.Update("matches", "id=eq." + matchIdCopy, matchUpdate);
    });

    if (!realtime_.IsConnected()) {
        pollTimer = POLL_INTERVAL_FALLBACK_SEC;
        SchedulePoll();
    }
}

bool NetMatch::PollOpponentTurn(RemoteTurnResult& out) {
    std::lock_guard lock(mu_);
    if (!hasPendingTurn_) return false;
    out = pendingTurn_;
    hasPendingTurn_ = false;
    lastSeenTurnNumber = out.turnNumber;
    cachedCurrentTurnPlayer_ = out.nextTurnPlayer;
    syncedCurrentTurnPlayer = out.nextTurnPlayer;
    awaitingOpponentTurn_.store(false);
    opponentAim_ = {};
    pendingLiveShotStart_ = false;
    liveSamples_.clear();
    liveShotEnded_ = false;
    return true;
}

bool NetMatch::PollSpawnResync(RemoteTurnResult& out) {
    std::lock_guard lock(mu_);
    if (!hasPendingSpawnResync_) return false;
    out = pendingSpawnResync_;
    hasPendingSpawnResync_ = false;
    return out.spawnPowerupType >= 0;
}

bool NetMatch::PollHealthResync(RemoteTurnResult& out) {
    std::lock_guard lock(mu_);
    if (!hasPendingHealthResync_) return false;
    out = pendingHealthResync_;
    hasPendingHealthResync_ = false;
    return true;
}

bool NetMatch::PollSpectatorMatchEnded(int& winnerOut) {
    if (myPlayerNumber != 0) return false;
    std::lock_guard lock(mu_);
    if (!spectatorMatchEnded_ || spectatorEndReported_) return false;
    spectatorEndReported_ = true;
    winnerOut = spectatorWinner_;
    return true;
}

DisconnectResult NetMatch::PollDisconnect() {
    std::lock_guard lock(mu_);
    if (disconnectReported_ || pendingDisconnect_ == DisconnectResult::None) {
        return DisconnectResult::None;
    }
    disconnectReported_ = true;
    DisconnectResult r = pendingDisconnect_;
    pendingDisconnect_ = DisconnectResult::None;
    return r;
}

int NetMatch::TakeAbandonWinner() {
    std::lock_guard lock(mu_);
    const int w = pendingAbandonWinner_;
    pendingAbandonWinner_ = 0;
    return w;
}

int NetMatch::AbandonWinnerPlayer() const {
    if (myPlayerNumber == 0) return 0;
    return WinnerWhenPlayerLeaves(myPlayerNumber, composition_);
}

void NetMatch::AbandonMatch() {
    if (matchId.empty()) return;

    const std::string matchIdCopy = matchId;
    const int winner = AbandonWinnerPlayer();
    GlobalNetWorker().Post([matchIdCopy, winner](SupabaseClient& client) {
        json rows = client.Select("matches", "select=status&id=eq." + matchIdCopy);
        if (rows.is_array() && !rows.empty()) {
            std::string status = rows[0].value("status", "active");
            if (status == "active") {
                client.Update("matches", "id=eq." + matchIdCopy, {
                    { "status", "abandoned" },
                    { "winner_player", winner }
                });
            }
        }
    });
    LeaveMatch();
}

void NetMatch::LeaveMatch() {
    active_.store(false);
    realtime_.Stop();
    matchId.clear();
    opponentId.clear();
    opponentName.clear();
    lastSeenTurnNumber = 0;
    pollInFlight_.store(false);
    awaitingOpponentTurn_.store(false);

    GlobalNetWorker().ClearCoalesced();
    GlobalNetWorker().CloseConnections();

    std::lock_guard lock(mu_);
    hasPendingTurn_ = false;
    pendingDisconnect_ = DisconnectResult::None;
    pendingAbandonWinner_ = 0;
    disconnectReported_ = false;
    opponentAim_ = {};
    pendingLiveShotStart_ = false;
    liveSamples_.clear();
    liveShotEnded_ = false;
    liveShotId_ = 0;
    pendingPowerupPickups_.clear();
    pendingPowerupSpawns_.clear();
    hasPendingSpawnResync_ = false;
    pendingSpawnResync_ = {};
    spectatorMatchEnded_ = false;
    spectatorEndReported_ = false;
    spectatorWinner_ = 0;
}
