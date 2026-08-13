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
    turn.damageP1 = JsonFloat(row, "damage_p1", 0.0f);
    turn.damageP2 = JsonFloat(row, "damage_p2", 0.0f);
    turn.damageP3 = JsonFloat(row, "damage_p3", 0.0f);
    turn.damageP4 = JsonFloat(row, "damage_p4", 0.0f);
    turn.nextWind = JsonFloat(row, "next_wind", 0.0f);
    turn.nextTurnPlayer = JsonInt(row, "next_turn_player", 1);
    turn.matchOver = JsonBool(row, "match_over", false);
    turn.winnerPlayer = JsonInt(row, "winner_player", 0);
    turn.pickedPowerupType = JsonInt(row, "picked_powerup_type", -1);
    turn.pickedPowerupX = JsonFloat(row, "picked_powerup_x", 0.0f);
    return turn;
}

void NetMatch::ApplyTurnRecord(const json& row) {
    const int shooterPlayer = JsonInt(row, "shooter_player", 1);
    const int turnNumber = JsonInt(row, "turn_number", 0);
    if (turnNumber <= 0) return;

    if (myPlayerNumber != 0 && shooterPlayer == myPlayerNumber) {
        std::lock_guard lock(mu_);
        if (turnNumber > lastSeenTurnNumber) lastSeenTurnNumber = turnNumber;
        return;
    }

    RemoteTurnResult turn = ParseTurnJson(row);
    std::lock_guard lock(mu_);
    if (turnNumber <= lastSeenTurnNumber) return;
    if (turnNumber != lastSeenTurnNumber + 1) {
        DebugLogf(LOG_WARNING, "NET: turno #%d fora de ordem (esperado #%d)",
                  turnNumber, lastSeenTurnNumber + 1);
        return;
    }
    if (hasPendingTurn_ && pendingTurn_.turnNumber >= turnNumber) return;
    pendingTurn_ = turn;
    hasPendingTurn_ = true;
    DebugLogf(LOG_INFO, "NET: turno remoto #%d enfileirado (atirador P%d)",
              turn.turnNumber, turn.shooterPlayer);
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
            pendingDisconnect_ = OnlineDidIWin(winner, myPlayerNumber, matchFormat_)
                ? DisconnectResult::OpponentLeft
                : DisconnectResult::MatchAbandoned;
        }
    }
    syncedCurrentTurnPlayer = currentTurnPlayer;
    ApplyLiveAimFromRecord(row);
}

void NetMatch::SignalParticipantLeft(int leavingPlayerNum) {
    if (myPlayerNumber == 0 || leavingPlayerNum <= 0) return;
    const int winner = WinnerWhenPlayerLeaves(leavingPlayerNum, matchFormat_);
    if (winner == 0) return;

    std::lock_guard lock(mu_);
    if (disconnectReported_ || pendingDisconnect_ != DisconnectResult::None) return;
    pendingAbandonWinner_ = winner;
    pendingDisconnect_ = OnlineDidIWin(winner, myPlayerNumber, matchFormat_)
        ? DisconnectResult::OpponentLeft
        : DisconnectResult::MatchAbandoned;
    DebugLogf(LOG_INFO, "NET: jogador P%d saiu — vencedor=%d eu=P%d",
              leavingPlayerNum, winner, myPlayerNumber);
}

void NetMatch::CheckParticipantsPresence(SupabaseClient& client, const json& matchRow) {
    if (myPlayerNumber == 0 || matchId.empty()) return;
    if (json_helpers::Str(matchRow, "status", "active") != "active") return;

    const int participantCount = IsTeamMode(matchFormat_) ? 4 : 2;
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
                { "winner_player", WinnerWhenPlayerLeaves(playerNum, matchFormat_) }
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

    if (event == "powerup_picked") {
        const int player = JsonInt(p, "player", 0);
        if (player == 0 || player == myPlayerNumber) return;
        LivePowerupPickup pu;
        pu.type = JsonInt(p, "type", -1);
        pu.x = JsonFloat(p, "x", 0.0f);
        pu.valid = pu.type >= 0;
        if (!pu.valid) return;
        std::lock_guard lock(mu_);
        pendingPowerupPickup_ = pu;
        hasPendingPowerupPickup_ = true;
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
                      MatchFormat format) {
    matchId = id;
    myPlayerNumber = myPlayerNum;
    matchFormat_ = format;
    opponentId = oppId;
    opponentName = oppName;
    syncedCurrentTurnPlayer = 1;
    cachedCurrentTurnPlayer_ = 1;
    lastSeenTurnNumber = 0;
    pollTimer = 0.0f;
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
        pendingDisconnect_ = DisconnectResult::None;
        pendingAbandonWinner_ = 0;
        disconnectReported_ = false;
        opponentAim_ = {};
        pendingLiveShotStart_ = false;
        liveSamples_.clear();
        liveShotEnded_ = false;
        liveShotId_ = 0;
        hasPendingPowerupPickup_ = false;
        pendingPowerupPickup_ = {};
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

void NetMatch::BeginSpectating(const std::string& id, int currentTurnPlayer, int lastTurnNumber) {
    Begin(id, 0, "", "", MatchFormat::Duel1v1);
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

void NetMatch::Pump(float dt) {
    if (!active_.load() || matchId.empty()) return;

    realtime_.Drain();

    {
        std::lock_guard lock(mu_);
        syncedCurrentTurnPlayer = cachedCurrentTurnPlayer_;
    }

    pollTimer += dt;
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

    GlobalNetWorker().Post([this, matchIdCopy, nextTurn](SupabaseClient& client) {
        json matchRows = client.Select("matches",
            "select=status,winner_player,current_turn_player,match_format,"
            "player1_id,player2_id,player3_id,player4_id,"
            "live_shooter,live_angle,live_power,live_aim_phase&id=eq." + matchIdCopy);

        if (matchRows.is_array() && !matchRows.empty()) {
            const json& mrow = matchRows[0];
            if (json_helpers::Str(mrow, "status", "active") == "active") {
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

bool NetMatch::PollRemotePowerupPickup(LivePowerupPickup& out) {
    std::lock_guard lock(mu_);
    if (!hasPendingPowerupPickup_) return false;
    out = pendingPowerupPickup_;
    hasPendingPowerupPickup_ = false;
    pendingPowerupPickup_ = {};
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
    if (!active_.load() || matchId.empty()) return;
    const int maxPlayer = IsTeamMode(matchFormat_) ? 4 : 2;
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
                             float damageP1, float damageP2, float damageP3, float damageP4,
                             float nextWind, int nextTurnPlayer,
                             bool matchOver, int winnerPlayer,
                             int pickedPowerupType, float pickedPowerupX) {
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

    DebugLogf(LOG_INFO, "NET: SubmitMyTurn #%d atirador=P%d proximo=P%d pickup=%d",
              turnNum, shooter, nextTurnPlayer, pickedPowerupType);

    if (realtime_.IsConnected()) {
        realtime_.SendBroadcast("turn_submitted", {
            { "turn_number", turnNum },
            { "shooter", shooter }
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
            { "damage_p1", damageP1 },
            { "damage_p2", damageP2 },
            { "damage_p3", damageP3 },
            { "damage_p4", damageP4 },
            { "next_wind", nextWind },
            { "next_turn_player", nextTurnPlayer },
            { "match_over", matchOver },
            { "winner_player", winnerPlayer },
            { "picked_powerup_type", pickedPowerupType },
            { "picked_powerup_x", pickedPowerupX }
        };
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
    if (!IsTeamMode(matchFormat_)) {
        return (myPlayerNumber == 1) ? 2 : 1;
    }
    return (myPlayerNumber <= 2) ? 2 : 1;
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
    hasPendingPowerupPickup_ = false;
    pendingPowerupPickup_ = {};
    spectatorMatchEnded_ = false;
    spectatorEndReported_ = false;
    spectatorWinner_ = 0;
}
