#pragma once
#include "../Platform.h"
#include "../GameTypes.h"
#include "../MatchRoster.h"

#include <vector>
#include <string>
#include <atomic>
#include "PlayerIdentity.h"
#include "SupabaseClient.h"
#include "RealtimeClient.h"
#include <nlohmann/json.hpp>

struct LobbyPlayerCard {
    std::string playerId;
    std::string displayName;
    int wins = 0;
    int losses = 0;
    bool inLiveMatch = false;
    bool inTeamRoom = false;
    std::string teamRoomId;
    std::string liveMatchId;
    std::string liveMatchP1Name;
    std::string liveMatchP2Name;
    bool liveMatchIsPlus = false;
    int liveMatchCurrentTurn = 1;
};

struct ActiveMatchCard {
    std::string matchId;
    std::string player1Name;
    std::string player2Name;
    int currentTurnPlayer = 1;
    bool isPlus = false;
};

enum class OutgoingChallengeResult {
    None,
    Declined,
    Expired,
};

struct IncomingChallenge {
    std::string challengeId;
    std::string fromPlayerId;
    std::string fromDisplayName;
    GameVersion challengeVersion = GameVersion::Classic;
};

struct IncomingTeamInvite {
    std::string inviteId;
    std::string roomId;
    std::string fromPlayerId;
    std::string fromDisplayName;
    char team = 'a';
    GameVersion roomVersion = GameVersion::Classic;
};

struct TeamRoomMember {
    std::string playerId;
    std::string displayName;
    int slot = 0;
};

struct TeamRoomView {
    std::string roomId;
    std::vector<TeamRoomMember> teamA;
    std::vector<TeamRoomMember> teamB;
    GameVersion version = GameVersion::Classic;
    std::string status;
    bool amCaptain = false;
    char myTeam = '\0';

    int TeamACount() const { return static_cast<int>(teamA.size()); }
    int TeamBCount() const { return static_cast<int>(teamB.size()); }
    MatchComposition Composition() const {
        return { std::max(1, TeamACount()), std::max(1, TeamBCount()) };
    }
    bool CanInvite(char team) const {
        return (team == 'a') ? (TeamACount() < MatchRoster::kMaxPerTeam)
                             : (TeamBCount() < MatchRoster::kMaxPerTeam);
    }
};

struct MatchStart {
    std::string matchId;
    int myPlayerNumber = 1;
    std::string opponentId;
    std::string opponentName;
    unsigned int terrainSeed = 0;
    bool isPlus = false;
    MatchComposition composition;
    std::string playerNames[MatchRoster::kMaxCannons];
};

class OnlineLobby {
public:
    void Init(PlayerIdentity* identity);

    void EnterLobby();
    void Update(float dt);
    void LeaveLobby();

    void PauseRealtime();
    void HeartbeatInMatch(float dt);
    void MarkInMatch(const std::string& matchId);
    void MarkIdle();
    void SnapshotRematchRoom();
    bool HasRematchTeamRoom() const;
    void ClearRematchRoom();
    void ReturnToLobbyAfterMatch();
    void EnterTeamRoomForRematch();
    void AbandonActiveMatch(const std::string& matchId, int winnerPlayer);

    const std::vector<LobbyPlayerCard>& Players() const { return players; }
    const std::vector<IncomingChallenge>& IncomingChallenges() const { return incoming; }
    const std::vector<IncomingTeamInvite>& IncomingTeamInvites() const { return incomingTeamInvites; }
    const TeamRoomView* ActiveTeamRoom() const { return inTeamRoom_ ? &teamRoom_ : nullptr; }

    void SendChallenge(const LobbyPlayerCard& target, GameVersion ver);
    void AcceptChallenge(const IncomingChallenge& challenge);
    void DeclineChallenge(const IncomingChallenge& challenge);

    void EnterTeamRoom(const std::string& roomId);
    void LeaveTeamRoom();
    void UpdateTeamRoom(float dt);
    void SendTeamInvite(const LobbyPlayerCard& target);
    void AcceptTeamInvite(const IncomingTeamInvite& invite);
    void DeclineTeamInvite(const IncomingTeamInvite& invite);
    void CancelTeamRoom();
    void LeaveTeamAsPartner();
    void StartTeamMatch();

    bool PollMatchStart(MatchStart& out);
    bool PollEnterTeamRoom(std::string& roomIdOut);
    bool HasPendingOutgoingChallenge() const { return !pendingChallengeId.empty(); }
    const GameVersion& PendingChallengeVersion() const { return pendingChallengeVersion_; }
    bool HasChallengeResultNotice() const {
        return challengeResult_ != OutgoingChallengeResult::None && challengeResultTimer_ > 0.0f;
    }
    OutgoingChallengeResult ChallengeResult() const { return challengeResult_; }
    const std::string& ChallengeResultOpponentName() const { return challengeResultOpponent_; }

    void ReportMatchResult(bool won);
    bool UpdateDisplayName(const std::string& rawName, std::string& outSanitized);

private:
    PlayerIdentity* identity = nullptr;
    SupabaseClient client;
    RealtimeClient realtime_;
    RealtimeClient teamRealtime_;

    std::vector<LobbyPlayerCard> players;
    std::vector<IncomingChallenge> incoming;
    std::vector<IncomingTeamInvite> incomingTeamInvites;

    std::string currentMatchId_;
    std::string activeTeamRoomId_;
    TeamRoomView teamRoom_;
    bool inTeamRoom_ = false;

    float pollTimer = 0.0f;
    float ghostCleanupTimer = 0.0f;
    float teamPollTimer = 0.0f;
    float teamPresenceTimer_ = 0.0f;
    static constexpr float POLL_INTERVAL_REALTIME_SEC = 3.0f;
    static constexpr float POLL_INTERVAL_FALLBACK_SEC = 1.0f;
    static constexpr float GHOST_CLEANUP_SEC = 25.0f;
    static constexpr float TEAM_POLL_SEC = 1.5f;
    static constexpr float TEAM_PRESENCE_HEARTBEAT_SEC = 2.5f;

    bool lobbyActive_ = false;
    bool teamRoomActive_ = false;
    bool needsBootstrap_ = false;
    bool wasRealtimeConnected_ = false;

    float matchHeartbeatTimer = 0.0f;
    float pendingChallengeTimer_ = 0.0f;
    static constexpr float MATCH_HEARTBEAT_SEC = 2.5f;
    static constexpr float CHALLENGE_TIMEOUT_SEC = 10.0f;
    static constexpr float CHALLENGE_RESULT_DISPLAY_SEC = 4.0f;

    std::string pendingChallengeId;
    std::string pendingChallengeOpponentId;
    std::string pendingChallengeOpponentName;
    GameVersion pendingChallengeVersion_ = GameVersion::Classic;
    OutgoingChallengeResult challengeResult_ = OutgoingChallengeResult::None;
    std::string challengeResultOpponent_;
    float challengeResultTimer_ = 0.0f;

    bool registeredPlayer = false;
    bool realtimeStarted_ = false;
    bool teamRealtimeStarted_ = false;

    std::atomic<bool> dirtyPresence_{false};
    std::atomic<bool> dirtyChallenges_{false};
    std::atomic<bool> dirtyTeamRoom_{false};

    static constexpr int LIVE_MATCH_MAX_AGE_SEC = 45;
    static constexpr int IN_MATCH_STALE_SEC = 8;

    bool hasReadyMatch = false;
    MatchStart readyMatch;
    bool hasEnterTeamRoom_ = false;
    std::string enterTeamRoomId_;

    std::string lastTeamRoomId_;

    void EnsurePlayerRegistered();
    void EnsureRealtime();
    void EnsureTeamRealtime();
    void UpsertPresenceWithStatus(const char* status, const std::string& matchId = "");
    void UpsertPresence();
    void RefreshPlayerList();
    void EnrichPlayersFromTeamRooms();
    void RefreshIncomingChallenges();
    void RefreshIncomingTeamInvites();
    void RefreshTeamRoom();
    bool LoadTeamRoom(const std::string& roomId, TeamRoomView& out);
    void TickPendingChallenge(float dt);
    void ExpirePendingChallenge();
    void TryResolveAcceptedChallenge();
    void ClearPendingChallengeState();
    void NotifyChallengeResult(OutgoingChallengeResult result);
    void TickChallengeResultDisplay(float dt);
    void TryResolveTeamRoomMatchStart();
    void MaybeCleanupGhostPresence();
    void SyncLobbyData(bool upsertPresence);
    float PollIntervalSec() const;
    int MyPlayerNumberInRoom(const TeamRoomView& room) const;
    void BuildMatchStartFromRow(const nlohmann::json& mrow, const TeamRoomView& room);
    void InsertRoomCaptains(const std::string& roomId, const std::string& captainA,
                            const std::string& captainB);
};
