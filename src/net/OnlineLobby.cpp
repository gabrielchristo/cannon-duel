#include "OnlineLobby.h"
#include "../DebugLog.h"
#include "JsonHelpers.h"
#include "NetValidation.h"
#include "NetWorker.h"
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <algorithm>
#include <ctime>
#include <unordered_map>
#include <unordered_set>
#include <unordered_set>
#include <cstdio>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace {
std::string UtcNowIso8601() {
    std::time_t now = std::time(nullptr);
    std::tm t {};
#if defined(_WIN32)
    gmtime_s(&t, &now);
#else
    gmtime_r(&now, &t);
#endif
    std::ostringstream oss;
    oss << std::put_time(&t, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
} // namespace

void OnlineLobby::Init(PlayerIdentity* id) {
    identity = id;
    pollTimer = 0.0f;
    ghostCleanupTimer = 0.0f;
    registeredPlayer.store(false);
    registerInFlight_.store(false);
    lobbyActive_.store(false);
    needsBootstrap_ = false;
    wasRealtimeConnected_ = false;
    hasReadyMatch = false;
    pendingChallengeId.clear();
    pendingChallengeOpponentId.clear();
    pendingChallengeOpponentName.clear();
    pendingChallengeTimer_ = 0.0f;
    DebugLogf(LOG_INFO, "LOBBY: inicializado com player_id=%s nome=%s",
             id ? id->Id().c_str() : "(nulo)", id ? id->DisplayName().c_str() : "(nulo)");
}

void OnlineLobby::EnterLobby() {
    lobbyActive_ = true;
    needsBootstrap_ = true;
    pollTimer = PollIntervalSec();
    ghostCleanupTimer = GHOST_CLEANUP_SEC;
    wasRealtimeConnected_ = false;
    players.clear();
    incoming.clear();
    incomingTeamInvites.clear();
    activeTeamRoomId_.clear();
    inTeamRoom_ = false;
    teamRoomActive_ = false;
    DebugLogf(LOG_INFO, "LOBBY: EnterLobby — bootstrap imediato");
}

void OnlineLobby::EnsurePlayerRegistered() {
    if (registeredPlayer.load() || !identity) return;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) {
        DebugLogf(LOG_WARNING, "LOBBY: player_id inválido — abortando registro");
        return;
    }
    if (registerInFlight_.exchange(true)) return;

    const std::string playerId = identity->Id();
    const std::string displayName = identity->DisplayName();
    GlobalNetWorker().Post([this, playerId, displayName](SupabaseClient& http) {
        json body = {
            { "id", playerId },
            { "display_name", displayName }
        };
        http.Upsert("players", body, "id");
        if (http.LastRequestOk()) {
            registeredPlayer.store(true);
            DebugLogf(LOG_INFO, "LOBBY: jogador registrado com sucesso em 'players'");
        } else {
            registerInFlight_.store(false);
            DebugLogf(LOG_WARNING, "LOBBY: falha ao registrar jogador em 'players' — tentando de novo no próximo ciclo");
        }
    });
}

void OnlineLobby::UpsertPresenceWithStatus(SupabaseClient& http, const char* status,
                                           const std::string& matchId) {
    if (!identity) return;

    json me = http.Select("players", "select=wins,losses&id=eq." + identity->Id());
    int wins = 0, losses = 0;
    if (me.is_array() && !me.empty()) {
        wins = json_helpers::Int(me[0], "wins", 0);
        losses = json_helpers::Int(me[0], "losses", 0);
    }
    myWins_.store(wins);
    myLosses_.store(losses);

    json body = {
        { "player_id", identity->Id() },
        { "display_name", identity->DisplayName() },
        { "wins", wins },
        { "losses", losses },
        { "status", status },
        { "last_seen", UtcNowIso8601() },
        { "match_id", matchId.empty() ? json(nullptr) : json(matchId) }
    };
    http.Upsert("lobby_presence", body, "player_id");
    DebugLogf(http.LastRequestOk() ? LOG_INFO : LOG_WARNING,
             "LOBBY: upsert de presença (%s) %s", status,
             http.LastRequestOk() ? "OK" : "FALHOU");
}

void OnlineLobby::UpsertPresence() {
    PostPresenceUpsert("idle", "");
}

void OnlineLobby::PostPresenceUpsert(const char* status, const std::string& matchId) {
    if (!identity) return;
    const std::string playerId = identity->Id();
    const std::string displayName = identity->DisplayName();
    const std::string statusStr = status ? status : "idle";
    const std::string matchCopy = matchId;
    GlobalNetWorker().PostCoalesced("presence", [=](SupabaseClient& client) {
        json me = client.Select("players", "select=wins,losses&id=eq." + playerId);
        int myWins = 0, myLosses = 0;
        if (me.is_array() && !me.empty()) {
            myWins = json_helpers::Int(me[0], "wins", 0);
            myLosses = json_helpers::Int(me[0], "losses", 0);
        }
        json body = {
            { "player_id", playerId },
            { "display_name", displayName },
            { "wins", myWins },
            { "losses", myLosses },
            { "status", statusStr },
            { "last_seen", UtcNowIso8601() },
            { "match_id", matchCopy.empty() ? json(nullptr) : json(matchCopy) }
        };
        client.Upsert("lobby_presence", body, "player_id");
        myWins_.store(myWins);
        myLosses_.store(myLosses);
    });
}

void OnlineLobby::HeartbeatInMatch(float dt) {
    matchHeartbeatTimer += dt;
    if (matchHeartbeatTimer < MATCH_HEARTBEAT_SEC) return;
    matchHeartbeatTimer = 0.0f;
    PostPresenceUpsert("in_match", currentMatchId_);
}

void OnlineLobby::MarkInMatch(const std::string& matchId) {
    matchHeartbeatTimer = 0.0f;
    currentMatchId_ = matchId;
    if (registeredPlayer.load()) PostPresenceUpsert("in_match", currentMatchId_);
}

void OnlineLobby::MarkIdle() {
    currentMatchId_.clear();
    matchHeartbeatTimer = 0.0f;
    if (registeredPlayer.load()) PostPresenceUpsert("idle", "");
}

void OnlineLobby::SnapshotRematchRoom() {
    if (!activeTeamRoomId_.empty()) {
        lastTeamRoomId_ = activeTeamRoomId_;
    }
}

bool OnlineLobby::HasRematchTeamRoom() const {
    return !lastTeamRoomId_.empty();
}

void OnlineLobby::ClearRematchRoom() {
    lastTeamRoomId_.clear();
}

void OnlineLobby::ReturnToLobbyAfterMatch() {
    MarkIdle();
    teamRealtime_.Stop();
    teamRealtimeStarted_ = false;
    inTeamRoom_ = false;
    teamRoomActive_ = false;
    activeTeamRoomId_.clear();
    lobbyActive_ = true;
    needsBootstrap_ = true;
    pollTimer = 0.0f;
    wasRealtimeConnected_ = false;
    players.clear();
    incoming.clear();
    incomingTeamInvites.clear();
    DebugLogf(LOG_INFO, "LOBBY: retorno ao lobby apos partida");
}

void OnlineLobby::EnterTeamRoomForRematch() {
    if (lastTeamRoomId_.empty()) return;

    json rows = client.Select("team_rooms", "select=status,match_id&id=eq." + lastTeamRoomId_);
    if (rows.is_array() && !rows.empty()) {
        const std::string status = json_helpers::Str(rows[0], "status");
        if (status == "started" || status == "cancelled") {
            client.Update("team_rooms", "id=eq." + lastTeamRoomId_, json{
                { "status", "recruiting" },
                { "match_id", nullptr }
            });
        }
    }

    MarkIdle();
    EnterTeamRoom(lastTeamRoomId_);
    DebugLogf(LOG_INFO, "LOBBY: rematch — reentrando sala %s", lastTeamRoomId_.c_str());
}

void OnlineLobby::AbandonActiveMatch(const std::string& matchId, int winnerPlayer) {
    if (matchId.empty() || winnerPlayer == 0) return;

    GlobalNetWorker().Post([matchId, winnerPlayer](SupabaseClient& http) {
        json rows = http.Select("matches", "select=status&id=eq." + matchId);
        if (!rows.is_array() || rows.empty()) return;
        if (json_helpers::Str(rows[0], "status") != "active") return;
        http.Update("matches", "id=eq." + matchId, {
            { "status", "abandoned" },
            { "winner_player", winnerPlayer }
        });
        DebugLogf(http.LastRequestOk() ? LOG_INFO : LOG_WARNING,
                  "LOBBY: abandonou partida %s", matchId.c_str());
    });
}

void OnlineLobby::LeaveLobby() {
    if (!identity) return;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) return;
    lobbyActive_ = false;
    needsBootstrap_ = false;
    wasRealtimeConnected_ = false;
    realtime_.Stop();
    realtimeStarted_ = false;
    const std::string playerId = identity->Id();
    GlobalNetWorker().Post([playerId](SupabaseClient& http) {
        http.Delete("lobby_presence", "player_id=eq." + playerId);
    });
    client.Close();
    matchHeartbeatTimer = 0.0f;
    players.clear();
    incoming.clear();
    incomingTeamInvites.clear();
    currentMatchId_.clear();
    activeTeamRoomId_.clear();
    inTeamRoom_ = false;
    teamRoomActive_ = false;
    teamRealtime_.Stop();
    teamRealtimeStarted_ = false;
    client.Close();
    matchHeartbeatTimer = 0.0f;
}

void OnlineLobby::PauseRealtime() {
    realtime_.Stop();
    realtimeStarted_ = false;
}

float OnlineLobby::PollIntervalSec() const {
    if (realtime_.IsConnected()) return POLL_INTERVAL_REALTIME_SEC;
    return POLL_INTERVAL_FALLBACK_SEC;
}

void OnlineLobby::EnsureRealtime() {
    if (!identity || realtimeStarted_ || !registeredPlayer) return;
    realtimeStarted_ = true;

    realtime_.SetOnPostgres("lobby_presence", [this](const json&) {
        dirtyPresence_.store(true);
    });
    realtime_.SetOnPostgres("challenges", [this](const json&) {
        dirtyChallenges_.store(true);
    });

    std::vector<RealtimeClient::PostgresSub> subs = {
        { "*", "public", "lobby_presence", "" },
        { "*", "public", "challenges", "to_player_id=eq." + identity->Id() },
        { "*", "public", "challenges", "from_player_id=eq." + identity->Id() },
    };
    realtime_.Start("lobby:" + identity->Id(), subs);
    DebugLogf(LOG_INFO, "LOBBY: Realtime iniciado");
}

void OnlineLobby::TryResolveAcceptedChallenge(SupabaseClient& http) {
    if (!identity || pendingChallengeId.empty() || pendingChallengeId == "sending" || hasReadyMatch) {
        return;
    }

    json mine = http.Select("challenges", "select=status,match_id&id=eq." + pendingChallengeId);
    if (!mine.is_array() || mine.empty()) return;

    const std::string status = mine[0].value("status", "pending");
    if (status == "accepted") {
        json rooms = http.Select("team_rooms",
            "select=id&challenge_id=eq." + pendingChallengeId + "&limit=1");
        if (rooms.is_array() && !rooms.empty()) {
            enterTeamRoomId_ = json_helpers::Str(rooms[0], "id");
            hasEnterTeamRoom_ = true;
            DebugLogf(LOG_INFO, "LOBBY: desafio aceito — sala %s", enterTeamRoomId_.c_str());
        }
        ClearPendingChallengeState();
    } else if (status == "declined") {
        DebugLogf(LOG_INFO, "LOBBY: desafio %s recusado", pendingChallengeId.c_str());
        NotifyChallengeResult(OutgoingChallengeResult::Declined);
    } else if (status == "expired") {
        DebugLogf(LOG_INFO, "LOBBY: desafio %s expirou", pendingChallengeId.c_str());
        NotifyChallengeResult(OutgoingChallengeResult::Expired);
    } else if (status != "pending") {
        ClearPendingChallengeState();
    }
}

void OnlineLobby::ClearPendingChallengeState() {
    pendingChallengeId.clear();
    pendingChallengeOpponentId.clear();
    pendingChallengeOpponentName.clear();
    pendingChallengeTimer_ = 0.0f;
}

void OnlineLobby::NotifyChallengeResult(OutgoingChallengeResult result) {
    challengeResultOpponent_ = pendingChallengeOpponentName;
    challengeResult_ = result;
    challengeResultTimer_ = CHALLENGE_RESULT_DISPLAY_SEC;
    ClearPendingChallengeState();
}

void OnlineLobby::TickChallengeResultDisplay(float dt) {
    if (challengeResultTimer_ <= 0.0f) return;
    challengeResultTimer_ = std::max(0.0f, challengeResultTimer_ - dt);
    if (challengeResultTimer_ <= 0.0f) {
        challengeResult_ = OutgoingChallengeResult::None;
        challengeResultOpponent_.clear();
    }
}

void OnlineLobby::MaybeCleanupGhostPresence() {
    if (!identity) return;

    const std::time_t cutoff = std::time(nullptr) - 25; // 25s
    std::tm t {};
#if defined(_WIN32)
    gmtime_s(&t, &cutoff);
#else
    gmtime_r(&cutoff, &t);
#endif
    std::ostringstream cutoffOss;
    cutoffOss << std::put_time(&t, "%Y-%m-%dT%H:%M:%SZ");
    GlobalNetWorker().Post([cutoff = cutoffOss.str()](SupabaseClient& http) {
        http.Delete("lobby_presence", "last_seen=lt." + cutoff);
    });
}

void OnlineLobby::SyncLobbyData(SupabaseClient& http, bool upsertPresence) {
    if (!lobbyActive_.load()) return;
    if (upsertPresence) UpsertPresenceWithStatus(http, "idle");
    RefreshPlayerList(http);
    RefreshIncomingChallenges(http);
    RefreshIncomingTeamInvites(http);
}

void OnlineLobby::ScheduleLobbySync(bool upsertPresence) {
    if (!identity || !lobbyActive_.load()) return;
    GlobalNetWorker().PostCoalesced("lobby_sync", [this, upsertPresence](SupabaseClient& http) {
        SyncLobbyData(http, upsertPresence);
    });
}

void OnlineLobby::ScheduleTeamSync() {
    if (!identity || !teamRoomActive_) return;
    GlobalNetWorker().PostCoalesced("team_sync", [this](SupabaseClient& http) {
        RefreshTeamRoom(http);
        RefreshPlayerList(http);
        RefreshIncomingTeamInvites(http);
        TryResolveTeamRoomMatchStart(http);
    });
}

std::vector<LobbyPlayerCard> OnlineLobby::Players() const {
    std::lock_guard lock(dataMu_);
    return players;
}

std::vector<IncomingChallenge> OnlineLobby::IncomingChallenges() const {
    std::lock_guard lock(dataMu_);
    return incoming;
}

std::vector<IncomingTeamInvite> OnlineLobby::IncomingTeamInvites() const {
    std::lock_guard lock(dataMu_);
    return incomingTeamInvites;
}

bool OnlineLobby::CopyActiveTeamRoom(TeamRoomView& out) const {
    std::lock_guard lock(dataMu_);
    if (!inTeamRoom_) return false;
    out = teamRoom_;
    return true;
}

void OnlineLobby::RefreshPlayerList(SupabaseClient& http) {
    if (!identity) return;

    json rows = http.Select("lobby_presence",
        "select=player_id,display_name,wins,losses,last_seen,status,match_id&order=player_id.asc&limit=30");

    std::vector<LobbyPlayerCard> built;
    if (!rows.is_array()) {
        DebugLogf(LOG_WARNING, "LOBBY: RefreshPlayerList não recebeu um array (requisição falhou?)");
        return;
    }

    const std::time_t presenceCutoff = std::time(nullptr) - 25;
    const std::time_t matchCutoff = std::time(nullptr) - LIVE_MATCH_MAX_AGE_SEC;
    std::tm pt {}, mt {};
#if defined(_WIN32)
    gmtime_s(&pt, &presenceCutoff);
    gmtime_s(&mt, &matchCutoff);
#else
    gmtime_r(&presenceCutoff, &pt);
    gmtime_r(&matchCutoff, &mt);
#endif
    std::ostringstream presenceCutoffOss, matchCutoffOss;
    presenceCutoffOss << std::put_time(&pt, "%Y-%m-%dT%H:%M:%SZ");
    matchCutoffOss << std::put_time(&mt, "%Y-%m-%dT%H:%M:%SZ");
    const std::string presenceCutoffIso = presenceCutoffOss.str();
    const std::string inMatchCutoffIso = [&]() {
        const std::time_t inMatchCutoff = std::time(nullptr) - IN_MATCH_STALE_SEC;
        std::tm imt {};
#if defined(_WIN32)
        gmtime_s(&imt, &inMatchCutoff);
#else
        gmtime_r(&inMatchCutoff, &imt);
#endif
        std::ostringstream oss;
        oss << std::put_time(&imt, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }();
    const std::string matchCutoffIso = matchCutoffOss.str();

    struct PresenceRow {
        std::string playerId;
        std::string displayName;
        int wins = 0;
        int losses = 0;
        std::string status;
        std::string matchId;
        std::string lastSeen;
        bool online = false;
        bool inMatchLive = false;
    };
    std::unordered_map<std::string, PresenceRow> presenceByPlayer;

    for (const auto& row : rows) {
        const std::string pid = json_helpers::Str(row, "player_id");
        if (pid.empty() || pid == identity->Id()) continue;

        PresenceRow pr;
        pr.playerId = pid;
        pr.displayName = json_helpers::Str(row, "display_name", "???");
        pr.wins = json_helpers::Int(row, "wins", 0);
        pr.losses = json_helpers::Int(row, "losses", 0);
        pr.status = json_helpers::Str(row, "status");
        pr.matchId = json_helpers::Str(row, "match_id");
        pr.lastSeen = json_helpers::Str(row, "last_seen");

        if (!pr.lastSeen.empty() && pr.lastSeen < presenceCutoffIso) continue;

        pr.online = true;
        pr.inMatchLive = (pr.status == "in_match" && !pr.matchId.empty()
                          && !pr.lastSeen.empty() && pr.lastSeen >= inMatchCutoffIso);
        presenceByPlayer[pid] = std::move(pr);
    }

    std::vector<std::string> candidateMatchIds;
    for (const auto& [pid, pr] : presenceByPlayer) {
        (void)pid;
        if (!pr.inMatchLive) continue;
        candidateMatchIds.push_back(pr.matchId);
    }
    std::sort(candidateMatchIds.begin(), candidateMatchIds.end());
    candidateMatchIds.erase(std::unique(candidateMatchIds.begin(), candidateMatchIds.end()),
                            candidateMatchIds.end());

    struct VerifiedMatch {
        std::string id;
        std::string p1id, p2id, p3id, p4id;
        std::string p1name, p2name;
        int currentTurn = 1;
        bool isPlus = false;
        bool isTeam2v2 = false;
    };
    std::unordered_map<std::string, VerifiedMatch> verifiedMatches;

    if (!candidateMatchIds.empty()) {
        std::string inList;
        for (size_t i = 0; i < candidateMatchIds.size(); ++i) {
            if (i > 0) inList += ",";
            inList += candidateMatchIds[i];
        }
        json matchRows = http.Select("matches",
            "select=id,player1_id,player2_id,player3_id,player4_id,match_format,version,current_turn_player,status,updated_at"
            "&id=in.(" + inList + ")&status=eq.active&updated_at=gte." + matchCutoffIso);

        std::vector<std::string> playerIds;
        if (matchRows.is_array()) {
            for (const auto& mrow : matchRows) {
                VerifiedMatch vm;
                vm.id = json_helpers::Str(mrow, "id");
                vm.p1id = json_helpers::Str(mrow, "player1_id");
                vm.p2id = json_helpers::Str(mrow, "player2_id");
                vm.p3id = json_helpers::Str(mrow, "player3_id");
                vm.p4id = json_helpers::Str(mrow, "player4_id");
                vm.isTeam2v2 = (json_helpers::Str(mrow, "match_format", "duel_1v1") == "team_2v2");
                vm.currentTurn = json_helpers::Int(mrow, "current_turn_player", 1);
                vm.isPlus = (json_helpers::Str(mrow, "version", "classic") == "plus");
                if (!vm.p1id.empty()) playerIds.push_back(vm.p1id);
                if (!vm.p2id.empty()) playerIds.push_back(vm.p2id);
                if (!vm.p3id.empty()) playerIds.push_back(vm.p3id);
                if (!vm.p4id.empty()) playerIds.push_back(vm.p4id);
                verifiedMatches[vm.id] = vm;
            }
        }

        std::sort(playerIds.begin(), playerIds.end());
        playerIds.erase(std::unique(playerIds.begin(), playerIds.end()), playerIds.end());
        std::unordered_map<std::string, std::string> names;
        if (!playerIds.empty()) {
            std::string pidList;
            for (size_t i = 0; i < playerIds.size(); ++i) {
                if (i > 0) pidList += ",";
                pidList += playerIds[i];
            }
            json prows = http.Select("players", "select=id,display_name&id=in.(" + pidList + ")");
            if (prows.is_array()) {
                for (const auto& p : prows) {
                    names[json_helpers::Str(p, "id")] = json_helpers::Str(p, "display_name", "???");
                }
            }
        }
        for (auto& [id, vm] : verifiedMatches) {
            auto lookup = [&](const std::string& pid) -> std::string {
                auto it = names.find(pid);
                return (it != names.end()) ? it->second : "???";
            };
            vm.p1name = lookup(vm.p1id);
            vm.p2name = vm.isTeam2v2 ? lookup(vm.p3id) : lookup(vm.p2id);
        }

        for (auto it = verifiedMatches.begin(); it != verifiedMatches.end(); ) {
            const VerifiedMatch& vm = it->second;
            auto playerLive = [&](const std::string& pid) -> bool {
                auto pit = presenceByPlayer.find(pid);
                if (pit == presenceByPlayer.end()) return false;
                const PresenceRow& pr = pit->second;
                return pr.inMatchLive && pr.matchId == vm.id;
            };
            if (vm.isTeam2v2) {
                if (!playerLive(vm.p1id) || !playerLive(vm.p2id)
                    || !playerLive(vm.p3id) || !playerLive(vm.p4id)) {
                    it = verifiedMatches.erase(it);
                    continue;
                }
            } else if (!playerLive(vm.p1id) || !playerLive(vm.p2id)) {
                it = verifiedMatches.erase(it);
                continue;
            }
            ++it;
        }
    }

    for (const auto& [pid, pr] : presenceByPlayer) {
        (void)pid;
        LobbyPlayerCard card;
        card.playerId = pr.playerId;
        card.displayName = pr.displayName;
        card.wins = pr.wins;
        card.losses = pr.losses;

        if (pr.inMatchLive) {
            auto it = verifiedMatches.find(pr.matchId);
            if (it != verifiedMatches.end()) {
                card.inLiveMatch = true;
                card.liveMatchId = it->second.id;
                card.liveMatchP1Name = it->second.p1name;
                card.liveMatchP2Name = it->second.p2name;
                card.liveMatchIsPlus = it->second.isPlus;
                card.liveMatchCurrentTurn = it->second.currentTurn;
            }
        } else if (pr.status == "team_room" && !pr.matchId.empty()) {
            card.inTeamRoom = true;
            card.teamRoomId = pr.matchId;
        }

        built.push_back(card);
    }

    EnrichPlayersFromTeamRooms(http, built);

    std::sort(built.begin(), built.end(),
              [](const LobbyPlayerCard& a, const LobbyPlayerCard& b) {
                  return a.playerId < b.playerId;
              });

    const int onlineCount = static_cast<int>(built.size());
    {
        std::lock_guard lock(dataMu_);
        players.swap(built);
    }

    DebugLogf(LOG_INFO, "LOBBY: eu=%s | %d online (filtrado)",
              identity->Id().c_str(), onlineCount);
}

void OnlineLobby::EnrichPlayersFromTeamRooms(SupabaseClient& http,
                                             std::vector<LobbyPlayerCard>& dest) {
    if (!identity) return;

    json rooms = http.Select("team_rooms",
        "select=id,status&status=eq.recruiting&limit=30");
    if (!rooms.is_array() || rooms.empty()) return;

    std::unordered_set<std::string> known;
    for (const auto& p : dest) known.insert(p.playerId);

    struct PendingCard {
        std::string playerId;
        std::string roomId;
    };
    std::vector<PendingCard> pending;
    std::vector<std::string> missingIds;

    for (const auto& row : rooms) {
        const std::string roomId = json_helpers::Str(row, "id");
        json members = http.Select("team_room_members",
            "select=player_id&room_id=eq." + roomId);
        if (!members.is_array()) continue;
        for (const auto& m : members) {
            const std::string pid = json_helpers::Str(m, "player_id");
            if (pid.empty() || pid == identity->Id() || known.count(pid)) continue;
            known.insert(pid);
            missingIds.push_back(pid);
            pending.push_back({ pid, roomId });
        }
    }
    if (pending.empty()) return;

    std::sort(missingIds.begin(), missingIds.end());
    missingIds.erase(std::unique(missingIds.begin(), missingIds.end()), missingIds.end());

    std::unordered_map<std::string, std::string> names;
    std::string inList;
    for (size_t i = 0; i < missingIds.size(); ++i) {
        if (i > 0) inList += ",";
        inList += missingIds[i];
    }
    json prows = http.Select("players", "select=id,display_name,wins,losses&id=in.(" + inList + ")");
    std::unordered_map<std::string, std::pair<int, int>> records;
    if (prows.is_array()) {
        for (const auto& p : prows) {
            const std::string id = json_helpers::Str(p, "id");
            names[id] = json_helpers::Str(p, "display_name", "???");
            records[id] = { json_helpers::Int(p, "wins", 0), json_helpers::Int(p, "losses", 0) };
        }
    }

    for (const PendingCard& pc : pending) {
        LobbyPlayerCard card;
        card.playerId = pc.playerId;
        card.displayName = names.count(pc.playerId) ? names[pc.playerId] : "???";
        if (records.count(pc.playerId)) {
            card.wins = records[pc.playerId].first;
            card.losses = records[pc.playerId].second;
        }
        card.inTeamRoom = true;
        card.teamRoomId = pc.roomId;
        dest.push_back(card);
    }
}

void OnlineLobby::RefreshIncomingChallenges(SupabaseClient& http) {
    if (!identity) return;

    json rows = http.Select("challenges",
        "select=id,from_player_id,from_display_name,format,version,status"
        "&to_player_id=eq." + identity->Id() +
        "&status=eq.pending&order=created_at.desc&limit=5");

    std::vector<IncomingChallenge> built;
    if (rows.is_array()) {
        for (auto& row : rows) {
            IncomingChallenge c;
            c.challengeId = row.value("id", "");
            c.fromPlayerId = row.value("from_player_id", "");
            c.fromDisplayName = row.value("from_display_name", "???");
            c.challengeVersion = (json_helpers::Str(row, "version", "classic") == "plus")
                ? GameVersion::Plus : GameVersion::Classic;
            built.push_back(c);
        }
    }
    {
        std::lock_guard lock(dataMu_);
        incoming.swap(built);
    }

    TryResolveAcceptedChallenge(http);
}

void OnlineLobby::TickPendingChallenge(float dt) {
    TickChallengeResultDisplay(dt);
    if (pendingChallengeId.empty() || pendingChallengeId == "sending") return;
    pendingChallengeTimer_ += dt;
    if (pendingChallengeTimer_ >= CHALLENGE_TIMEOUT_SEC) {
        ExpirePendingChallenge();
    }
}

void OnlineLobby::ExpirePendingChallenge() {
    if (pendingChallengeId.empty() || pendingChallengeId == "sending") return;
    const std::string id = pendingChallengeId;
    DebugLogf(LOG_INFO, "LOBBY: desafio %s expirou (timeout)", id.c_str());
    NotifyChallengeResult(OutgoingChallengeResult::Expired);
    GlobalNetWorker().Post([id](SupabaseClient& http) {
        http.Update("challenges", "id=eq." + id, json{ { "status", "expired" } });
    });
}

void OnlineLobby::Update(float dt) {
    static bool loggedOnce = false;
    if (!loggedOnce) {
        loggedOnce = true;
        DebugLogf(LOG_INFO, "LOBBY: OnlineLobby::Update() alcançado pela primeira vez (identity=%s)",
                  identity ? "ok" : "NULO");
    }

    if (!identity || !lobbyActive_) return;

    TickPendingChallenge(dt);

    EnsurePlayerRegistered();
    EnsureRealtime();
    realtime_.Drain();

    const bool rtConnected = realtime_.IsConnected();
    if (rtConnected && !wasRealtimeConnected_) {
        wasRealtimeConnected_ = true;
        DebugLogf(LOG_INFO, "LOBBY: Realtime conectado — sync imediato");
        ScheduleLobbySync(true);
    } else if (!rtConnected) {
        wasRealtimeConnected_ = false;
    }

    if (needsBootstrap_ && registeredPlayer.load()) {
        needsBootstrap_ = false;
        pollTimer = 0.0f;
        DebugLogf(LOG_INFO, "LOBBY: bootstrap — presença + lista");
        ScheduleLobbySync(true);
    }

    ghostCleanupTimer += dt;
    if (ghostCleanupTimer >= GHOST_CLEANUP_SEC) {
        ghostCleanupTimer = 0.0f;
        MaybeCleanupGhostPresence();
    }

    const bool dirty = dirtyPresence_.exchange(false) || dirtyChallenges_.exchange(false);

    pollTimer += dt;
    const bool duePoll = pollTimer >= PollIntervalSec();
    if (!duePoll && !dirty) return;
    if (duePoll) pollTimer = 0.0f;

    ScheduleLobbySync(duePoll);
}

void OnlineLobby::SendChallenge(const LobbyPlayerCard& target, GameVersion ver) {
    if (!identity) return;

    pendingChallengeId = "sending";
    pendingChallengeOpponentId = target.playerId;
    pendingChallengeOpponentName = target.displayName;
    pendingChallengeVersion_ = ver;
    pendingChallengeTimer_ = 0.0f;

    json body = {
        { "from_player_id", identity->Id() },
        { "from_display_name", identity->DisplayName() },
        { "to_player_id", target.playerId },
        { "status", "pending" },
        { "format", "composition" },
        { "version", ver == GameVersion::Plus ? "plus" : "classic" }
    };
    GlobalNetWorker().Post([this, body](SupabaseClient& http) {
        json created = http.Insert("challenges", body);
        if (created.is_array() && !created.empty()) {
            const std::string id = created[0].value("id", "");
            if (pendingChallengeId == "sending") pendingChallengeId = id;
        } else if (pendingChallengeId == "sending") {
            NotifyChallengeResult(OutgoingChallengeResult::Expired);
        }
    });
}

void OnlineLobby::AcceptChallenge(const IncomingChallenge& challenge) {
    if (!identity) return;

    json roomBody = {
        { "challenge_id", challenge.challengeId },
        { "captain_a_id", challenge.fromPlayerId },
        { "captain_b_id", identity->Id() },
        { "version", challenge.challengeVersion == GameVersion::Plus ? "plus" : "classic" },
        { "status", "recruiting" }
    };
    json created = client.Insert("team_rooms", roomBody);
    if (!created.is_array() || created.empty()) return;

    const std::string roomId = json_helpers::Str(created[0], "id");
    if (roomId.empty()) return;

    InsertRoomCaptains(roomId, challenge.fromPlayerId, identity->Id());

    client.Update("challenges", "id=eq." + challenge.challengeId,
                  json{ { "status", "accepted" } });

    incoming.erase(
        std::remove_if(incoming.begin(), incoming.end(),
                       [&](const IncomingChallenge& c) { return c.challengeId == challenge.challengeId; }),
        incoming.end());

    enterTeamRoomId_ = roomId;
    hasEnterTeamRoom_ = true;
    DebugLogf(LOG_INFO, "LOBBY: aceitei desafio — sala %s", roomId.c_str());
}

void OnlineLobby::DeclineChallenge(const IncomingChallenge& challenge) {
    json body = { { "status", "declined" } };
    client.Update("challenges", "id=eq." + challenge.challengeId, body);
    incoming.erase(
        std::remove_if(incoming.begin(), incoming.end(),
                       [&](const IncomingChallenge& c) { return c.challengeId == challenge.challengeId; }),
        incoming.end());
}

bool OnlineLobby::PollMatchStart(MatchStart& out) {
    if (hasReadyMatch) {
        out = readyMatch;
        hasReadyMatch = false;
        return true;
    }
    return false;
}

void OnlineLobby::ReportMatchResult(bool won) {
    if (!identity) return;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) return;

    if (won) myWins_.fetch_add(1);
    else myLosses_.fetch_add(1);

    const std::string playerId = identity->Id();
    GlobalNetWorker().Post([playerId, won](SupabaseClient& http) {
        json me = http.Select("players", "select=wins,losses&id=eq." + playerId);
        int wins = 0, losses = 0;
        if (me.is_array() && !me.empty()) {
            wins = me[0].value("wins", 0);
            losses = me[0].value("losses", 0);
        }
        if (won) wins++; else losses++;
        http.Update("players", "id=eq." + playerId, json{ { "wins", wins }, { "losses", losses } });
    });
}

bool OnlineLobby::UpdateDisplayName(const std::string& rawName, std::string& outSanitized) {
    if (!identity) return false;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) return false;

    outSanitized = net_validation::SanitizeDisplayName(rawName);
    if (outSanitized.empty()) return false;

    identity->SetDisplayName(outSanitized);

    const std::string playerId = identity->Id();
    GlobalNetWorker().Post([playerId, outSanitized](SupabaseClient& http) {
        http.Update("players", "id=eq." + playerId, json{ { "display_name", outSanitized } });
    });
    if (lobbyActive_.load() && registeredPlayer.load()) {
        PostPresenceUpsert("idle", "");
    }

    DebugLogf(LOG_INFO, "LOBBY: display_name atualizado para '%s'", outSanitized.c_str());
    return true;
}

