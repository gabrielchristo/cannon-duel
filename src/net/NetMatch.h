#pragma once
#include "../Platform.h"
#include "../GameTypes.h"
#include "../MatchRoster.h"
#include "../PowerupSystem.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "SupabaseClient.h"
#include "RealtimeClient.h"

struct RemoteTurnResult {
    struct PickedPowerupEntry {
        int type = -1;
        float x = 0.0f;
    };

    int turnNumber = 0;
    int shooterPlayer = 1;
    float shootAngle = 45.0f;
    float shootPower = 0.5f;
    float windAtShot = 0.0f;
    float impactX = 0, impactY = 0;
    float craterRadius = 0;
    float damages[MatchRoster::kMaxCannons] = {};
    float healthAfter[MatchRoster::kMaxCannons] = {};
    int healthAfterCount = 0;
    bool hasHealthAfter = false;
    float nextWind = 0;
    int nextTurnPlayer = 1;
    bool matchOver = false;
    int winnerPlayer = 0;
    int pickedPowerupType = -1; // legado / primeiro da lista
    float pickedPowerupX = 0.0f;
    std::vector<PickedPowerupEntry> pickedPowerups;
    int spawnPowerupType = -1; // spawn autoritativo do turno (-1 = nenhum)
    float spawnPowerupX = 0.0f;
};

struct LiveAimState {
    int player = 0;
    float angleDeg = 45.0f;
    float power01 = 0.0f;
    std::string phase = "idle";
    bool valid = false;
};

struct LiveShotStart {
    int shotId = 0;
    int shooterPlayer = 0;
    float angleDeg = 45.0f;
    float power01 = 0.5f;
    float wind = 0.0f;
    float muzzleX = 0.0f;
    float muzzleY = 0.0f;
};

struct ProjSample {
    int seq = 0;
    float t = 0.0f; // tempo de voo no atirador (s)
    float x = 0.0f;
    float y = 0.0f;
};

struct LivePowerupPickup {
    int player = 0;
    int type = -1;
    float x = 0.0f;
    bool valid = false;
};

struct LivePowerupSpawn {
    int type = -1;
    float x = 0.0f;
    int turnNumber = 0;
    bool valid = false;
};

// Comando de debug repassado via Realtime (dev panel online).
struct DevCommand {
    std::string action;
    int player = 0;
    int type = -1;
    float x = 0.0f;
    float value = 0.0f;
    bool valid = false;
};

enum class DisconnectResult {
    None,
    OpponentLeft,
    MatchAbandoned
};

// Sync: Realtime (CDC + broadcast mira/projétil) primário; poll HTTP fallback.
class NetMatch {
public:
    void Begin(const std::string& matchId, int myPlayerNumber,
               const std::string& opponentId, const std::string& opponentName,
               const MatchComposition& composition);
    void BeginSpectating(const std::string& matchId, int currentTurnPlayer, int lastTurnNumber,
                         const MatchComposition& composition);
    void Pump(float dt);

    int MyPlayerNumber() const { return myPlayerNumber; }
    MatchComposition GetComposition() const { return composition_; }
    int AbandonWinnerPlayer() const;
    bool IsSpectator() const { return myPlayerNumber == 0; }
    int SyncedCurrentTurnPlayer() const { return syncedCurrentTurnPlayer; }
    const std::string& OpponentName() const { return opponentName; }
    bool IsMyTurn() const;
    bool InMatch() const { return !matchId.empty(); }
    const std::string& MatchId() const { return matchId; }
    bool AwaitingOpponentTurn() const { return awaitingOpponentTurn_.load(); }
    bool RealtimeConnected() const { return realtime_.IsConnected(); }

    void SubmitMyTurn(float shootAngle, float shootPower, float windAtShot,
                       float impactX, float impactY, float craterRadius,
                       const float damages[MatchRoster::kMaxCannons],
                       const float healthAfter[MatchRoster::kMaxCannons],
                       float nextWind, int nextTurnPlayer,
                       bool matchOver, int winnerPlayer,
                       const std::vector<ShotPowerupPickup>& pickups = {},
                       int spawnPowerupType = -1, float spawnPowerupX = 0.0f);

    void PublishLiveAim(float dt, float angleDeg, float power01, const char* phase);
    LiveAimState GetOpponentLiveAim() const;

    // Aviso imediato de coleta (visual); o efeito autoritativo vem no match_turns.
    void PublishPowerupPicked(int type, float x);
    bool PollRemotePowerupPickup(LivePowerupPickup& out);

    void PublishPowerupSpawn(int type, float x, int turnNumber);
    bool PollRemotePowerupSpawn(LivePowerupSpawn& out);
    bool PollSpawnResync(RemoteTurnResult& out);

    // Stream do projétil (atirador → adversário via broadcast).
    void PublishShotFired(float angleDeg, float power01, float wind,
                          float muzzleX, float muzzleY);
    void PublishProjectileSample(float dt, float x, float y);
    void PublishShotEnded(float x, float y);

    bool PollLiveShotStart(LiveShotStart& out);
    // Copia amostras novas desde lastSeq (inclusive next). Retorna quantas.
    int PullProjectileSamples(int afterSeq, std::vector<ProjSample>& out);
    bool PeekShotEnded(float& outX, float& outY) const;
    void ClearLiveShot();

    int TurnsCompleted() const { return lastSeenTurnNumber; }

    bool PollOpponentTurn(RemoteTurnResult& out);
    // Vida autoritativa que chegou depois do turno já ter sido consumido (race postgres/broadcast).
    bool PollHealthResync(RemoteTurnResult& out);
    bool PollSpectatorMatchEnded(int& winnerOut);
    DisconnectResult PollDisconnect();
    int TakeAbandonWinner();

    void AbandonMatch();
    void LeaveMatch();

    void PublishDevCommand(const std::string& action, int player = 0, int type = -1,
                           float x = 0.0f, float value = 0.0f);
    bool PollDevCommand(DevCommand& out);
    void DevSyncTurnTo(int nextPlayer);
    void SyncTurnTo(int nextPlayer);

    static RemoteTurnResult ParseTurnJson(const nlohmann::json& row);
    static RemoteTurnResult ParseTurnPayload(const nlohmann::json& payload);

private:
    bool TryEnqueueRemoteTurn(const RemoteTurnResult& turn);
    void SchedulePoll();
    float PollIntervalSec() const;
    void ApplyTurnRecord(const nlohmann::json& row);
    void ApplyMatchRecord(const nlohmann::json& row);
    void ApplyBroadcast(const nlohmann::json& envelope);
    void ApplyLiveAimFromRecord(const nlohmann::json& row);
    void CheckParticipantsPresence(SupabaseClient& client, const nlohmann::json& matchRow);
    void SignalParticipantLeft(int leavingPlayerNum);

    std::string matchId;
    int myPlayerNumber = 1;
    MatchComposition composition_ = { 1, 1 };
    std::string opponentId;
    std::string opponentName;
    int syncedCurrentTurnPlayer = 1;
    int lastSeenTurnNumber = 0;

    float pollTimer = 0.0f;
    float liveAimPublishTimer_ = 0.0f;
    float projPublishTimer_ = 0.0f;
    float localShotFlightT_ = 0.0f;
    int localShotId_ = 0;
    int localProjSeq_ = 0;

    mutable std::mutex mu_;
    bool hasPendingTurn_ = false;
    RemoteTurnResult pendingTurn_{};
    bool hasPendingHealthResync_ = false;
    RemoteTurnResult pendingHealthResync_{};
    int cachedCurrentTurnPlayer_ = 1;
    DisconnectResult pendingDisconnect_ = DisconnectResult::None;
    int pendingAbandonWinner_ = 0;
    bool disconnectReported_ = false;
    LiveAimState opponentAim_{};
    std::deque<LivePowerupPickup> pendingPowerupPickups_;
    std::deque<LivePowerupSpawn> pendingPowerupSpawns_;

    bool hasPendingSpawnResync_ = false;
    RemoteTurnResult pendingSpawnResync_{};

    bool hasPendingDevCommand_ = false;
    DevCommand pendingDevCommand_{};

    bool pendingLiveShotStart_ = false;
    LiveShotStart pendingLiveShot_{};
    int liveShotId_ = 0;
    std::deque<ProjSample> liveSamples_;
    bool liveShotEnded_ = false;
    float liveShotEndX_ = 0.0f;
    float liveShotEndY_ = 0.0f;

    bool spectatorMatchEnded_ = false;
    bool spectatorEndReported_ = false;
    int spectatorWinner_ = 0;

    std::atomic<bool> active_{false};
    std::atomic<bool> pollInFlight_{false};
    std::atomic<bool> awaitingOpponentTurn_{false};

    RealtimeClient realtime_;

    static constexpr float POLL_INTERVAL_REALTIME_SEC = 5.0f;
    static constexpr float POLL_INTERVAL_FALLBACK_SEC = 1.2f;
    static constexpr float LIVE_AIM_PUBLISH_INTERVAL_SEC = 0.05f;
    static constexpr float LIVE_AIM_HTTP_INTERVAL_SEC = 0.35f;
    static constexpr float PROJ_PUBLISH_INTERVAL_SEC = 0.04f; // ~25 Hz
    static constexpr int PARTICIPANT_STALE_SEC = 8;
    static constexpr float MATCH_START_PRESENCE_GRACE_SEC = 20.0f;
    static constexpr size_t MAX_LIVE_SAMPLES = 256;

    float presenceGraceRemaining_ = 0.0f;
};
