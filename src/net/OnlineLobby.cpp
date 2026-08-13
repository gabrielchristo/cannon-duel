#include "OnlineLobby.h"
#include "../DebugLog.h"
#include "JsonHelpers.h"
#include "NetValidation.h"
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
    registeredPlayer = false;
    lobbyActive_ = false;
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
    if (registeredPlayer || !identity) return;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) {
        DebugLogf(LOG_WARNING, "LOBBY: player_id inválido — abortando registro");
        return;
    }

    // Upsert na tabela players — cria na primeira vez, ou só confirma que
    // já existe nas próximas (o id é sempre o mesmo, gerado localmente).
    json body = {
        { "id", identity->Id() },
        { "display_name", identity->DisplayName() }
    };
    client.Upsert("players", body, "id");

    // Só marca como registrado se a requisição de fato deu certo — senão,
    // a linha em "players" nunca chega a existir, e toda tentativa futura
    // de registrar presença falha em silêncio (lobby_presence.player_id
    // tem uma foreign key pra players(id)). Sem essa checagem, os dois
    // lados ficavam "conectados" mas nenhum via o outro no lobby.
    if (client.LastRequestOk()) {
        registeredPlayer = true;
        DebugLogf(LOG_INFO, "LOBBY: jogador registrado com sucesso em 'players'");
    } else {
        DebugLogf(LOG_WARNING, "LOBBY: falha ao registrar jogador em 'players' — tentando de novo no próximo ciclo");
    }
}

void OnlineLobby::UpsertPresenceWithStatus(const char* status, const std::string& matchId) {
    if (!identity) return;

    json me = client.Select("players", "select=wins,losses&id=eq." + identity->Id());
    int myWins = 0, myLosses = 0;
    if (me.is_array() && !me.empty()) {
        myWins = json_helpers::Int(me[0], "wins", 0);
        myLosses = json_helpers::Int(me[0], "losses", 0);
    }

    json body = {
        { "player_id", identity->Id() },
        { "display_name", identity->DisplayName() },
        { "wins", myWins },
        { "losses", myLosses },
        { "status", status },
        { "last_seen", UtcNowIso8601() },
        { "match_id", matchId.empty() ? json(nullptr) : json(matchId) }
    };
    client.Upsert("lobby_presence", body, "player_id");
    DebugLogf(client.LastRequestOk() ? LOG_INFO : LOG_WARNING,
             "LOBBY: upsert de presença (%s) %s", status,
             client.LastRequestOk() ? "OK" : "FALHOU");
}

void OnlineLobby::UpsertPresence() {
    UpsertPresenceWithStatus("idle");
}

void OnlineLobby::HeartbeatInMatch(float dt) {
    matchHeartbeatTimer += dt;
    if (matchHeartbeatTimer < MATCH_HEARTBEAT_SEC) return;
    matchHeartbeatTimer = 0.0f;
    UpsertPresenceWithStatus("in_match", currentMatchId_);
}

void OnlineLobby::MarkInMatch(const std::string& matchId) {
    matchHeartbeatTimer = 0.0f;
    currentMatchId_ = matchId;
    if (registeredPlayer) UpsertPresenceWithStatus("in_match", currentMatchId_);
}

void OnlineLobby::MarkIdle() {
    currentMatchId_.clear();
    matchHeartbeatTimer = 0.0f;
    if (registeredPlayer) UpsertPresenceWithStatus("idle", "");
}

void OnlineLobby::AbandonActiveMatch(const std::string& matchId, int winnerPlayer) {
    if (matchId.empty() || winnerPlayer == 0) return;

    json rows = client.Select("matches", "select=status&id=eq." + matchId);
    if (!rows.is_array() || rows.empty()) return;

    if (json_helpers::Str(rows[0], "status") != "active") return;

    client.Update("matches", "id=eq." + matchId, {
        { "status", "abandoned" },
        { "winner_player", winnerPlayer }
    });
    DebugLogf(client.LastRequestOk() ? LOG_INFO : LOG_WARNING,
              "LOBBY: abandonou partida %s (sync)", matchId.c_str());
}

void OnlineLobby::LeaveLobby() {
    if (!identity) return;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) return;
    lobbyActive_ = false;
    needsBootstrap_ = false;
    wasRealtimeConnected_ = false;
    realtime_.Stop();
    realtimeStarted_ = false;
    client.Delete("lobby_presence", "player_id=eq." + identity->Id());
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

void OnlineLobby::TryResolveAcceptedChallenge() {
    if (!identity || pendingChallengeId.empty() || hasReadyMatch) return;

    json mine = client.Select("challenges", "select=status,match_id&id=eq." + pendingChallengeId);
    if (!mine.is_array() || mine.empty()) return;

    std::string status = mine[0].value("status", "pending");
    if (status == "accepted") {
        if (pendingChallengeFormat_ == OnlineChallengeFormat::Team2v2) {
            json rooms = client.Select("team_rooms",
                "select=id&challenge_id=eq." + pendingChallengeId + "&limit=1");
            if (rooms.is_array() && !rooms.empty()) {
                enterTeamRoomId_ = json_helpers::Str(rooms[0], "id");
                hasEnterTeamRoom_ = true;
                DebugLogf(LOG_INFO, "LOBBY: desafio 2x2 aceito — sala %s", enterTeamRoomId_.c_str());
            }
            pendingChallengeId.clear();
            return;
        }

        std::string matchId = mine[0].value("match_id", "");
        if (!matchId.empty()) {
            json matchRows = client.Select("matches",
                "select=terrain_seed,version,player1_id,player2_id&id=eq." + matchId);
            if (matchRows.is_array() && !matchRows.empty()) {
                readyMatch.matchId = matchId;
                const std::string p1 = matchRows[0].value("player1_id", "");
                const std::string p2 = matchRows[0].value("player2_id", "");
                if (identity->Id() == p1) {
                    readyMatch.myPlayerNumber = 1;
                    readyMatch.opponentId = p2.empty() ? pendingChallengeOpponentId : p2;
                } else if (identity->Id() == p2) {
                    readyMatch.myPlayerNumber = 2;
                    readyMatch.opponentId = p1.empty() ? pendingChallengeOpponentId : p1;
                } else {
                    readyMatch.myPlayerNumber = 1;
                    readyMatch.opponentId = pendingChallengeOpponentId;
                }
                readyMatch.opponentName = pendingChallengeOpponentName;
                long long seed = matchRows[0].value("terrain_seed", 0LL);
                readyMatch.terrainSeed = static_cast<unsigned int>(seed);
                readyMatch.isPlus = (matchRows[0].value("version", "classic") == "plus");
                readyMatch.format = MatchFormat::Duel1v1;
                readyMatch.playerNames[0] = identity->DisplayName();
                readyMatch.playerNames[1] = pendingChallengeOpponentName;
                hasReadyMatch = true;
                DebugLogf(LOG_INFO, "LOBBY: partida aceita match=%s eu=P%d",
                          matchId.c_str(), readyMatch.myPlayerNumber);
            }
        }
        pendingChallengeId.clear();
    } else if (status != "pending") {
        pendingChallengeId.clear();
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
    client.Delete("lobby_presence", "last_seen=lt." + cutoffOss.str());
}

void OnlineLobby::SyncLobbyData(bool upsertPresence) {
    if (upsertPresence) UpsertPresence();
    RefreshPlayerList();
    RefreshIncomingChallenges();
}

void OnlineLobby::RefreshPlayerList() {
    if (!identity) return;

    json rows = client.Select("lobby_presence",
        "select=player_id,display_name,wins,losses,last_seen,status,match_id&order=player_id.asc&limit=30");

    players.clear();
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
        json matchRows = client.Select("matches",
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
            json prows = client.Select("players", "select=id,display_name&id=in.(" + pidList + ")");
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

        players.push_back(card);
    }

    EnrichPlayersFromTeamRooms();

    std::sort(players.begin(), players.end(),
              [](const LobbyPlayerCard& a, const LobbyPlayerCard& b) {
                  return a.playerId < b.playerId;
              });

    DebugLogf(LOG_INFO, "LOBBY: eu=%s | %d online (filtrado)",
              identity->Id().c_str(), static_cast<int>(players.size()));
}

void OnlineLobby::EnrichPlayersFromTeamRooms() {
    if (!identity) return;

    json rooms = client.Select("team_rooms",
        "select=id,captain_a_id,captain_b_id,partner_a_id,partner_b_id,status"
        "&status=eq.recruiting&limit=30");
    if (!rooms.is_array() || rooms.empty()) return;

    std::unordered_set<std::string> known;
    for (const auto& p : players) known.insert(p.playerId);

    std::vector<std::string> missingIds;
    struct PendingCard {
        std::string playerId;
        std::string roomId;
    };
    std::vector<PendingCard> pending;

    for (const auto& row : rooms) {
        const std::string roomId = json_helpers::Str(row, "id");
        const char* keys[] = { "captain_a_id", "captain_b_id", "partner_a_id", "partner_b_id" };
        for (const char* key : keys) {
            const std::string pid = json_helpers::Str(row, key);
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
    json prows = client.Select("players", "select=id,display_name,wins,losses&id=in.(" + inList + ")");
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
        players.push_back(card);
    }
}

void OnlineLobby::RefreshIncomingChallenges() {
    if (!identity) return;

    json rows = client.Select("challenges",
        "select=id,from_player_id,from_display_name,format,version,status"
        "&to_player_id=eq." + identity->Id() +
        "&status=eq.pending&order=created_at.desc&limit=5");

    incoming.clear();
    if (!rows.is_array()) return;

    for (auto& row : rows) {
        IncomingChallenge c;
        c.challengeId = row.value("id", "");
        c.fromPlayerId = row.value("from_player_id", "");
        c.fromDisplayName = row.value("from_display_name", "???");
        c.format = ParseOnlineChallengeFormat(json_helpers::Str(row, "format", "duel_1v1"));
        c.challengeVersion = (json_helpers::Str(row, "version", "classic") == "plus")
            ? GameVersion::Plus : GameVersion::Classic;
        incoming.push_back(c);
    }

    TryResolveAcceptedChallenge();
}

void OnlineLobby::TickPendingChallenge(float dt) {
    if (pendingChallengeId.empty()) return;
    pendingChallengeTimer_ += dt;
    if (pendingChallengeTimer_ >= CHALLENGE_TIMEOUT_SEC) {
        ExpirePendingChallenge();
    }
}

void OnlineLobby::ExpirePendingChallenge() {
    if (pendingChallengeId.empty()) return;
    const std::string id = pendingChallengeId;
    DebugLogf(LOG_INFO, "LOBBY: desafio %s expirou (timeout)", id.c_str());
    pendingChallengeId.clear();
    pendingChallengeOpponentId.clear();
    pendingChallengeOpponentName.clear();
    pendingChallengeTimer_ = 0.0f;
    pendingChallengeTimer_ = 0.0f;
    client.Update("challenges", "id=eq." + id, json{ { "status", "expired" } });
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
        SyncLobbyData(true);
    } else if (!rtConnected) {
        wasRealtimeConnected_ = false;
    }

    if (needsBootstrap_ && registeredPlayer) {
        needsBootstrap_ = false;
        pollTimer = 0.0f;
        DebugLogf(LOG_INFO, "LOBBY: bootstrap — presença + lista");
        SyncLobbyData(true);
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

    SyncLobbyData(duePoll);
    RefreshIncomingTeamInvites();
}

void OnlineLobby::SendChallenge(const LobbyPlayerCard& target, OnlineChallengeFormat format,
                              GameVersion ver) {
    if (!identity) return;

    json body = {
        { "from_player_id", identity->Id() },
        { "from_display_name", identity->DisplayName() },
        { "to_player_id", target.playerId },
        { "status", "pending" },
        { "format", OnlineChallengeFormatDb(format) },
        { "version", ver == GameVersion::Plus ? "plus" : "classic" }
    };
    json created = client.Insert("challenges", body);
    if (created.is_array() && !created.empty()) {
        pendingChallengeId = created[0].value("id", "");
        pendingChallengeOpponentId = target.playerId;
        pendingChallengeOpponentName = target.displayName;
        pendingChallengeFormat_ = format;
        pendingChallengeVersion_ = ver;
        pendingChallengeTimer_ = 0.0f;
    }
}

void OnlineLobby::AcceptChallenge(const IncomingChallenge& challenge) {
    if (!identity) return;

    if (challenge.format == OnlineChallengeFormat::Team2v2) {
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

        client.Update("challenges", "id=eq." + challenge.challengeId,
                      json{ { "status", "accepted" } });

        enterTeamRoomId_ = roomId;
        hasEnterTeamRoom_ = true;
        DebugLogf(LOG_INFO, "LOBBY: aceitei desafio 2x2 — sala %s", roomId.c_str());
        return;
    }

    const bool isPlusVersion = (challenge.challengeVersion == GameVersion::Plus);
    unsigned int seed = static_cast<unsigned int>(rand()) ^ static_cast<unsigned int>(time(nullptr));
    float wind = 0.0f;

    json matchBody = {
        { "player1_id", challenge.fromPlayerId },
        { "player2_id", identity->Id() },
        { "terrain_seed", static_cast<long long>(seed) },
        { "version", isPlusVersion ? "plus" : "classic" },
        { "match_format", "duel_1v1" },
        { "current_turn_player", 1 },
        { "wind", wind },
        { "status", "active" }
    };
    json created = client.Insert("matches", matchBody);
    if (!created.is_array() || created.empty()) return;

    std::string matchId = created[0].value("id", "");
    if (matchId.empty()) return;

    json updateBody = { { "status", "accepted" }, { "match_id", matchId } };
    client.Update("challenges", "id=eq." + challenge.challengeId, updateBody);

    readyMatch.matchId = matchId;
    readyMatch.myPlayerNumber = 2;
    readyMatch.opponentId = challenge.fromPlayerId;
    readyMatch.opponentName = challenge.fromDisplayName;
    readyMatch.terrainSeed = seed;
    readyMatch.isPlus = isPlusVersion;
    readyMatch.format = MatchFormat::Duel1v1;
    readyMatch.playerNames[0] = challenge.fromDisplayName;
    readyMatch.playerNames[1] = identity->DisplayName();
    hasReadyMatch = true;
    DebugLogf(LOG_INFO, "LOBBY: aceitei desafio match=%s eu=P2", matchId.c_str());
}

void OnlineLobby::DeclineChallenge(const IncomingChallenge& challenge) {
    json body = { { "status", "declined" } };
    client.Update("challenges", "id=eq." + challenge.challengeId, body);
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

    json me = client.Select("players", "select=wins,losses&id=eq." + identity->Id());
    int wins = 0, losses = 0;
    if (me.is_array() && !me.empty()) {
        wins = me[0].value("wins", 0);
        losses = me[0].value("losses", 0);
    }
    if (won) wins++; else losses++;

    json body = { { "wins", wins }, { "losses", losses } };
    client.Update("players", "id=eq." + identity->Id(), body);
}

bool OnlineLobby::UpdateDisplayName(const std::string& rawName, std::string& outSanitized) {
    if (!identity) return false;
    if (!net_validation::IsValidPlayerUuid(identity->Id())) return false;

    outSanitized = net_validation::SanitizeDisplayName(rawName);
    if (outSanitized.empty()) return false;

    identity->SetDisplayName(outSanitized);

    json body = { { "display_name", outSanitized } };
    client.Update("players", "id=eq." + identity->Id(), body);
    if (!client.LastRequestOk()) {
        DebugLogf(LOG_WARNING, "LOBBY: falha ao atualizar display_name em players");
        return false;
    }

    if (lobbyActive_ && registeredPlayer) {
        UpsertPresenceWithStatus("idle");
    }

    DebugLogf(LOG_INFO, "LOBBY: display_name atualizado para '%s'", outSanitized.c_str());
    return true;
}

