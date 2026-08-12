#include "OnlineLobby.h"


using json = nlohmann::json;

void OnlineLobby::Init(PlayerIdentity* id) {
    identity = id;
    pollTimer = 0.0f;
    registeredPlayer = false;
    hasReadyMatch = false;
    pendingChallengeId.clear();
    pendingChallengeOpponentId.clear();
    pendingChallengeOpponentName.clear();
}

void OnlineLobby::EnsurePlayerRegistered() {
    if (registeredPlayer || !identity) return;

    // Upsert na tabela players — cria na primeira vez, ou só confirma que
    // já existe nas próximas (o id é sempre o mesmo, gerado localmente).
    json body = {
        { "id", identity->Id() },
        { "display_name", identity->DisplayName() }
    };
    client.Upsert("players", body);
    registeredPlayer = true; // não insiste a cada frame, só uma vez por sessão de lobby
}

void OnlineLobby::UpsertPresence() {
    if (!identity) return;

    // Busca meu próprio placar atual (wins/losses) pra incluir na presença
    // — assim os outros jogadores veem meu histórico sem uma segunda
    // consulta separada por card.
    json me = client.Select("players", "select=wins,losses&id=eq." + identity->Id());
    int myWins = 0, myLosses = 0;
    if (me.is_array() && !me.empty()) {
        myWins = me[0].value("wins", 0);
        myLosses = me[0].value("losses", 0);
    }

    // "last_seen" não é enviado explicitamente — o valor padrão da coluna
    // (default now()) cuida disso a cada upsert, já que o PostgREST não
    // avalia "now()" como SQL quando mandado como valor de campo comum.
    json body = {
        { "player_id", identity->Id() },
        { "display_name", identity->DisplayName() },
        { "wins", myWins },
        { "losses", myLosses },
        { "status", "idle" }
    };
    client.Upsert("lobby_presence", body);
}

void OnlineLobby::RefreshPlayerList() {
    if (!identity) return;

    // PostgREST não avalia funções como now() dentro de um filtro de query
    // string (isso exigiria uma view ou função RPC no banco). Pra manter
    // esta primeira etapa simples, sem SQL customizado além do schema
    // básico, buscamos os registros mais recentes e não filtramos por
    // "online há quanto tempo" no banco — só ordenamos por last_seen.
    json rows = client.Select("lobby_presence",
        "select=player_id,display_name,wins,losses,last_seen&order=last_seen.desc&limit=30");

    players.clear();
    if (!rows.is_array()) return;

    for (auto& row : rows) {
        std::string pid = row.value("player_id", "");
        if (pid == identity->Id()) continue; // não me listo pra mim mesmo
        LobbyPlayerCard card;
        card.playerId = pid;
        card.displayName = row.value("display_name", "???");
        card.wins = row.value("wins", 0);
        card.losses = row.value("losses", 0);
        players.push_back(card);
    }
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

    // Se eu tinha um desafio pendente enviado e ele não está mais "pending"
    // (foi aceito ou recusado), verifica o resultado.
    if (!pendingChallengeId.empty()) {
        json mine = client.Select("challenges", "select=status,match_id&id=eq." + pendingChallengeId);
        if (mine.is_array() && !mine.empty()) {
            std::string status = mine[0].value("status", "pending");
            if (status == "accepted") {
                std::string matchId = mine[0].value("match_id", "");
                if (!matchId.empty()) {
                    json matchRows = client.Select("matches", "select=terrain_seed,version&id=eq." + matchId);
                    if (matchRows.is_array() && !matchRows.empty()) {
                        readyMatch.matchId = matchId;
                        readyMatch.myPlayerNumber = 1; // quem desafia sempre começa como player1
                        readyMatch.opponentId = pendingChallengeOpponentId;
                        readyMatch.opponentName = pendingChallengeOpponentName;
                        long long seed = matchRows[0].value("terrain_seed", 0LL);
                        readyMatch.terrainSeed = static_cast<unsigned int>(seed);
                        readyMatch.isPlus = (matchRows[0].value("version", "classic") == "plus");
                        hasReadyMatch = true;
                    }
                }
                pendingChallengeId.clear();
            } else if (status != "pending") {
                pendingChallengeId.clear();
            }
        }
    }
}

void OnlineLobby::Update(float dt) {
    if (!identity) return;

    EnsurePlayerRegistered();

    pollTimer += dt;
    if (pollTimer < POLL_INTERVAL_SEC) return;
    pollTimer = 0.0f;

    UpsertPresence();
    RefreshPlayerList();
    RefreshIncomingChallenges();
}

void OnlineLobby::LeaveLobby() {
    if (!identity) return;
    client.Delete("lobby_presence", "player_id=eq." + identity->Id());
    registeredPlayer = false;
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

