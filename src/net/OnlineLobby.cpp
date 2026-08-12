#include "OnlineLobby.h"
#include "../DebugLog.h"
#include <raylib.h>
#include <ctime>
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
    DebugLogf(LOG_INFO, "LOBBY: EnterLobby — bootstrap imediato");
}

void OnlineLobby::EnsurePlayerRegistered() {
    if (registeredPlayer || !identity) return;

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

void OnlineLobby::UpsertPresenceWithStatus(const char* status) {
    if (!identity) return;

    json me = client.Select("players", "select=wins,losses&id=eq." + identity->Id());
    int myWins = 0, myLosses = 0;
    if (me.is_array() && !me.empty()) {
        myWins = me[0].value("wins", 0);
        myLosses = me[0].value("losses", 0);
    }

    json body = {
        { "player_id", identity->Id() },
        { "display_name", identity->DisplayName() },
        { "wins", myWins },
        { "losses", myLosses },
        { "status", status },
        { "last_seen", UtcNowIso8601() }
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
    UpsertPresenceWithStatus("in_match");
}

void OnlineLobby::LeaveLobby() {
    if (!identity) return;
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

    const std::time_t cutoff = std::time(nullptr) - 25; // 25s
    std::tm t {};
#if defined(_WIN32)
    gmtime_s(&t, &cutoff);
#else
    gmtime_r(&cutoff, &t);
#endif
    std::ostringstream cutoffOss;
    cutoffOss << std::put_time(&t, "%Y-%m-%dT%H:%M:%SZ");
    const std::string cutoffIso = cutoffOss.str();

    json rows = client.Select("lobby_presence",
        "select=player_id,display_name,wins,losses,last_seen&order=last_seen.desc&limit=30");

    players.clear();
    if (!rows.is_array()) {
        DebugLogf(LOG_WARNING, "LOBBY: RefreshPlayerList não recebeu um array (requisição falhou?)");
        return;
    }

    for (auto& row : rows) {
        std::string pid = row.value("player_id", "");
        if (pid.empty() || pid == identity->Id()) continue;

        // ISO-8601 UTC ordena lexicograficamente — só mostra heartbeat recente.
        std::string seen = row.value("last_seen", "");
        if (!seen.empty() && seen < cutoffIso) continue;

        LobbyPlayerCard card;
        card.playerId = pid;
        card.displayName = row.value("display_name", "???");
        card.wins = row.value("wins", 0);
        card.losses = row.value("losses", 0);
        players.push_back(card);
    }
    DebugLogf(LOG_INFO, "LOBBY: eu=%s | %d online (filtrado)",
              identity->Id().c_str(), static_cast<int>(players.size()));
}

void OnlineLobby::RefreshIncomingChallenges() {
    if (!identity) return;

    json rows = client.Select("challenges",
        "select=id,from_player_id,from_display_name,status"
        "&to_player_id=eq." + identity->Id() +
        "&status=eq.pending&order=created_at.desc&limit=5");

    incoming.clear();
    if (!rows.is_array()) return;

    for (auto& row : rows) {
        IncomingChallenge c;
        c.challengeId = row.value("id", "");
        c.fromPlayerId = row.value("from_player_id", "");
        c.fromDisplayName = row.value("from_display_name", "???");
        incoming.push_back(c);
    }

    TryResolveAcceptedChallenge();
}

void OnlineLobby::Update(float dt) {
    static bool loggedOnce = false;
    if (!loggedOnce) {
        loggedOnce = true;
        DebugLogf(LOG_INFO, "LOBBY: OnlineLobby::Update() alcançado pela primeira vez (identity=%s)",
                  identity ? "ok" : "NULO");
    }

    if (!identity || !lobbyActive_) return;

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
}

void OnlineLobby::SendChallenge(const LobbyPlayerCard& target) {
    if (!identity) return;

    json body = {
        { "from_player_id", identity->Id() },
        { "from_display_name", identity->DisplayName() },
        { "to_player_id", target.playerId },
        { "status", "pending" }
    };
    json created = client.Insert("challenges", body);
    if (created.is_array() && !created.empty()) {
        pendingChallengeId = created[0].value("id", "");
        pendingChallengeOpponentId = target.playerId;
        pendingChallengeOpponentName = target.displayName;
    }
}

void OnlineLobby::AcceptChallenge(const IncomingChallenge& challenge, bool isPlusVersion) {
    if (!identity) return;

    // Quem ACEITA cria a linha da partida (evita corrida entre os dois
    // clientes tentando criar a mesma partida ao mesmo tempo) e decide a
    // versão (Classic/Plus) — o desafiante recebe essa escolha de volta ao
    // ler a linha da partida (ver RefreshIncomingChallenges). O desafiante
    // (from_player_id) sempre joga como "player1" (current_turn_player==1
    // começa a rodada), e quem aceitou joga como "player2".
    //
    // NOTA: na versão Plus, o spawn de power-ups usa RNG local de cada
    // cliente (não é sincronizado por rede ainda) — os dois lados podem ver
    // um power-up aparecer no mesmo turno, mas em posições/tipos diferentes.
    // O RESULTADO de cada tiro (dano, cratera) continua correto e
    // sincronizado; só a exibição do ícone do power-up no mapa pode
    // divergir visualmente entre os dois clientes por enquanto.
    unsigned int seed = static_cast<unsigned int>(rand()) ^ static_cast<unsigned int>(time(nullptr));
    float wind = 0.0f;

    json matchBody = {
        { "player1_id", challenge.fromPlayerId },
        { "player2_id", identity->Id() },
        { "terrain_seed", static_cast<long long>(seed) },
        { "version", isPlusVersion ? "plus" : "classic" },
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

