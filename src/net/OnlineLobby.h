#pragma once
#include "../Platform.h"


#include <vector>
#include <string>
#include "PlayerIdentity.h"
#include "SupabaseClient.h"

// Um "card" exibido na tela de lobby — um jogador atualmente online.
struct LobbyPlayerCard {
    std::string playerId;
    std::string displayName;
    int wins = 0;
    int losses = 0;
};

// Um desafio de partida recebido de outro jogador, aguardando minha resposta.
struct IncomingChallenge {
    std::string challengeId;
    std::string fromPlayerId;
    std::string fromDisplayName;
};

// Emitido quando uma partida está pronta pra começar (seja porque EU aceitei
// um desafio, seja porque o desafio que EU enviei acabou de ser aceito).
struct MatchStart {
    std::string matchId;
    int myPlayerNumber = 1;   // 1 ou 2 — define quem começa (current_turn_player == 1)
    std::string opponentId;
    std::string opponentName;
    unsigned int terrainSeed = 0;
    bool isPlus = false; // versão da partida, decidida por quem ACEITA o desafio
};

class OnlineLobby {
public:
    void Init(PlayerIdentity* identity);

    // Chamado a cada frame enquanto a tela de lobby está aberta — faz
    // polling periódico (não bloqueia o frame; cada requisição HTTP roda de
    // forma síncrona mas curta, com timeout de poucos segundos).
    void Update(float dt);

    // Chamado ao SAIR da tela de lobby, pra remover minha presença (não
    // aparecer mais como "online" pros outros).
    void LeaveLobby();

    const std::vector<LobbyPlayerCard>& Players() const { return players; }
    const std::vector<IncomingChallenge>& IncomingChallenges() const { return incoming; }

    void SendChallenge(const LobbyPlayerCard& target);
    // 'isPlusVersion' é a escolha de quem ACEITA — vira a versão da partida
    // pros dois lados (a partida busca esse valor de volta do banco).
    void AcceptChallenge(const IncomingChallenge& challenge, bool isPlusVersion);
    void DeclineChallenge(const IncomingChallenge& challenge);

    // Se uma partida ficou pronta pra começar desde a última checagem,
    // preenche 'out' e retorna true (uma única vez — consome o evento).
    bool PollMatchStart(MatchStart& out);

    // true assim que EU enviei um desafio e ainda não sei se foi aceito —
    // usado pra mostrar "aguardando resposta..." na UI.
    bool HasPendingOutgoingChallenge() const { return !pendingChallengeId.empty(); }

    // Atualiza localmente + no Supabase o placar de vitórias/derrotas do
    // jogador atual. Chame ao final de uma partida online.
    void ReportMatchResult(bool won);

private:
    PlayerIdentity* identity = nullptr;
    SupabaseClient client;

    std::vector<LobbyPlayerCard> players;
    std::vector<IncomingChallenge> incoming;

    float pollTimer = 0.0f;
    static constexpr float POLL_INTERVAL_SEC = 2.5f;

    std::string pendingChallengeId; // desafio que EU enviei, aguardando resposta
    std::string pendingChallengeOpponentId;
    std::string pendingChallengeOpponentName;
    bool registeredPlayer = false;

    bool hasReadyMatch = false;
    MatchStart readyMatch;

    void EnsurePlayerRegistered();
    void UpsertPresence();
    void RefreshPlayerList();
    void RefreshIncomingChallenges();
};

