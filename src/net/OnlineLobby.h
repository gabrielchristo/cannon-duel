#pragma once
#include "../Platform.h"


#include <vector>
#include <string>
#include <atomic>
#include "PlayerIdentity.h"
#include "SupabaseClient.h"
#include "RealtimeClient.h"

// Um "card" exibido na tela de lobby — um jogador atualmente online.
struct LobbyPlayerCard {
    std::string playerId;
    std::string displayName;
    int wins = 0;
    int losses = 0;
    bool inLiveMatch = false;
    std::string liveMatchId;
    std::string liveMatchP1Name;
    std::string liveMatchP2Name;
    bool liveMatchIsPlus = false;
    int liveMatchCurrentTurn = 1;
};

// Partida ativa listada na aba "Partidas".
struct ActiveMatchCard {
    std::string matchId;
    std::string player1Name;
    std::string player2Name;
    int currentTurnPlayer = 1;
    bool isPlus = false;
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

    // Chamado ao ABRIR a tela de lobby — publica presença e busca lista na hora.
    void EnterLobby();

    // Chamado a cada frame enquanto a tela de lobby está aberta.
    // Realtime (CDC) é o caminho rápido; poll HTTP é fallback lento.
    void Update(float dt);

    // Chamado ao SAIR da tela de lobby, pra remover minha presença (não
    // aparecer mais como "online" pros outros).
    void LeaveLobby();

    // Para o WS do lobby ao entrar na partida (HTTP heartbeat continua).
    void PauseRealtime();

    // Heartbeat durante partida online — mantém last_seen e status in_match.
    void HeartbeatInMatch(float dt);
    void MarkInMatch(const std::string& matchId);
    void MarkIdle();

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

    // Sincroniza display_name sanitizado em players (+ presença se no lobby).
    bool UpdateDisplayName(const std::string& rawName, std::string& outSanitized);

private:
    PlayerIdentity* identity = nullptr;
    SupabaseClient client;
    RealtimeClient realtime_;

    std::vector<LobbyPlayerCard> players;
    std::vector<IncomingChallenge> incoming;

    std::string currentMatchId_;

    float pollTimer = 0.0f;
    float ghostCleanupTimer = 0.0f;
    static constexpr float POLL_INTERVAL_REALTIME_SEC = 3.0f;
    static constexpr float POLL_INTERVAL_FALLBACK_SEC = 1.0f;
    static constexpr float GHOST_CLEANUP_SEC = 25.0f;

    bool lobbyActive_ = false;
    bool needsBootstrap_ = false;
    bool wasRealtimeConnected_ = false;

    float matchHeartbeatTimer = 0.0f;
    static constexpr float MATCH_HEARTBEAT_SEC = 2.5f;

    std::string pendingChallengeId;
    std::string pendingChallengeOpponentId;
    std::string pendingChallengeOpponentName;
    bool registeredPlayer = false;
    bool realtimeStarted_ = false;

    std::atomic<bool> dirtyPresence_{false};
    std::atomic<bool> dirtyChallenges_{false};

    static constexpr int LIVE_MATCH_MAX_AGE_SEC = 120;

    bool hasReadyMatch = false;
    MatchStart readyMatch;

    void EnsurePlayerRegistered();
    void EnsureRealtime();
    void UpsertPresenceWithStatus(const char* status, const std::string& matchId = "");
    void UpsertPresence();
    void RefreshPlayerList();
    void RefreshIncomingChallenges();
    void TryResolveAcceptedChallenge();
    void MaybeCleanupGhostPresence();
    void SyncLobbyData(bool upsertPresence);
    float PollIntervalSec() const;
};
