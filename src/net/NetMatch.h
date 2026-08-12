#pragma once
#include "../Platform.h"


#include <string>
#include "SupabaseClient.h"

// Resultado de um turno recebido do adversário — já vem calculado (o
// cliente que atirou roda a física localmente, aqui só aplicamos o
// resultado). Evita depender de simulação física bit-a-bit idêntica entre
// plataformas/compiladores diferentes, que o Box2D não garante.
struct RemoteTurnResult {
    int turnNumber = 0;
    int shooterPlayer = 1;
    float impactX = 0, impactY = 0;
    float craterRadius = 0;
    float damageP1 = 0, damageP2 = 0;
    float nextWind = 0;
    int nextTurnPlayer = 1;
    bool matchOver = false;
    int winnerPlayer = 0; // 0 = ninguém ainda
};

// Sincroniza uma partida 1x1 já em andamento entre dois clientes, usando as
// tabelas "matches" e "match_turns" do Supabase. Client-authoritative: cada
// jogador só manda o RESULTADO do próprio tiro; nunca recebe/aplica input
// bruto do adversário sem já vir resolvido.
class NetMatch {
public:
    void Begin(const std::string& matchId, int myPlayerNumber,
               const std::string& opponentId, const std::string& opponentName);

    int MyPlayerNumber() const { return myPlayerNumber; }
    const std::string& OpponentName() const { return opponentName; }
    bool IsMyTurn() const { return isMyTurn; }

    // Chamado depois que EU resolvo um tiro localmente (ResolveImpact) —
    // publica o resultado pro adversário buscar.
    void SubmitMyTurn(float impactX, float impactY, float craterRadius,
                       float damageP1, float damageP2, float nextWind,
                       bool matchOver, int winnerPlayer);

    // Chamado a cada frame enquanto NÃO é minha vez — faz polling do
    // próximo turno do adversário. Retorna true (uma única vez) quando um
    // novo resultado chega, preenchendo 'out'.
    bool PollOpponentTurn(RemoteTurnResult& out, float dt);

    void LeaveMatch();

private:
    SupabaseClient client;
    std::string matchId;
    int myPlayerNumber = 1;
    std::string opponentId;
    std::string opponentName;
    bool isMyTurn = true;
    int lastSeenTurnNumber = 0;

    float pollTimer = 0.0f;
    static constexpr float POLL_INTERVAL_SEC = 1.5f;
};

