#pragma once
#include "../Platform.h"
#include "../GameTypes.h"
#include "../MatchRoster.h"

#include <vector>
#include <string>
#include <atomic>
#include <mutex>
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
    std::string equippedCannonColors[MatchRoster::kMaxCannons];
    std::string equippedCannonSkins[MatchRoster::kMaxCannons];
    std::string equippedCannonEffects[MatchRoster::kMaxCannons];
    std::string equippedNameEffects[MatchRoster::kMaxCannons];
    std::string equippedAmmo[MatchRoster::kMaxCannons];
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

    std::vector<LobbyPlayerCard> Players() const;
    std::vector<IncomingChallenge> IncomingChallenges() const;
    std::vector<IncomingTeamInvite> IncomingTeamInvites() const;
    bool CopyActiveTeamRoom(TeamRoomView& out) const;

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

    int MyWins() const { return myWins_.load(); }
    int MyLosses() const { return myLosses_.load(); }

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

    std::atomic<bool> lobbyActive_{false};
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

    std::atomic<int> myWins_{0};
    std::atomic<int> myLosses_{0};
    std::atomic<bool> registeredPlayer{false};
    std::atomic<bool> registerInFlight_{false};

    mutable std::mutex dataMu_;
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
    void UpsertPresenceWithStatus(SupabaseClient& http, const char* status,
                                  const std::string& matchId = "");
    void PostPresenceUpsert(const char* status, const std::string& matchId);
    void UpsertPresence();
    void ScheduleLobbySync(bool upsertPresence);
    void ScheduleTeamSync();
    void RefreshPlayerList(SupabaseClient& http);
    void EnrichPlayersFromTeamRooms(SupabaseClient& http, std::vector<LobbyPlayerCard>& dest);
    void RefreshIncomingChallenges(SupabaseClient& http);
    void RefreshIncomingTeamInvites(SupabaseClient& http);
    void RefreshTeamRoom(SupabaseClient& http);
    bool LoadTeamRoom(SupabaseClient& http, const std::string& roomId, TeamRoomView& out);
    void TickPendingChallenge(float dt);
    void ExpirePendingChallenge();
    void TryResolveAcceptedChallenge(SupabaseClient& http);
    void ClearPendingChallengeState();
    void NotifyChallengeResult(OutgoingChallengeResult result);
    void TickChallengeResultDisplay(float dt);
    void TryResolveTeamRoomMatchStart(SupabaseClient& http);
    void MaybeCleanupGhostPresence();
    void SyncLobbyData(SupabaseClient& http, bool upsertPresence);
    float PollIntervalSec() const;
    int MyPlayerNumberInRoom(const TeamRoomView& room) const;
    void BuildMatchStartFromRow(const nlohmann::json& mrow, const TeamRoomView& room);
    void FillMatchStartCosmetics(const nlohmann::json& mrow, int totalPlayers);
    void InsertRoomCaptains(const std::string& roomId, const std::string& captainA,
                            const std::string& captainB);
};
