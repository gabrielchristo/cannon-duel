#pragma once
#include "../Platform.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include "SupabaseClient.h"
#include "RealtimeClient.h"

struct RemoteTurnResult {
    int turnNumber = 0;
    int shooterPlayer = 1;
    float shootAngle = 45.0f;
    float shootPower = 0.5f;
    float windAtShot = 0.0f;
    float impactX = 0, impactY = 0;
    float craterRadius = 0;
    float damageP1 = 0, damageP2 = 0;
    float nextWind = 0;
    int nextTurnPlayer = 1;
    bool matchOver = false;
    int winnerPlayer = 0;
    int pickedPowerupType = -1; // -1 = nenhum; senão cast de PowerupType
    float pickedPowerupX = 0.0f;
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
    int type = -1;
    float x = 0.0f;
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
               const std::string& opponentId, const std::string& opponentName);
    void Pump(float dt);

    int MyPlayerNumber() const { return myPlayerNumber; }
    int SyncedCurrentTurnPlayer() const { return syncedCurrentTurnPlayer; }
    const std::string& OpponentName() const { return opponentName; }
    bool IsMyTurn() const;
    bool InMatch() const { return !matchId.empty(); }
    bool AwaitingOpponentTurn() const { return awaitingOpponentTurn_.load(); }
    bool RealtimeConnected() const { return realtime_.IsConnected(); }

    void SubmitMyTurn(float shootAngle, float shootPower, float windAtShot,
                       float impactX, float impactY, float craterRadius,
                       float damageP1, float damageP2, float nextWind,
                       bool matchOver, int winnerPlayer,
                       int pickedPowerupType = -1, float pickedPowerupX = 0.0f);

    void PublishLiveAim(float dt, float angleDeg, float power01, const char* phase);
    LiveAimState GetOpponentLiveAim() const;

    // Aviso imediato de coleta (visual); o efeito autoritativo vem no match_turns.
    void PublishPowerupPicked(int type, float x);
    bool PollRemotePowerupPickup(LivePowerupPickup& out);

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
    DisconnectResult PollDisconnect();

    void AbandonMatch();
    void LeaveMatch();

private:
    void SchedulePoll();
    float PollIntervalSec() const;
    void ApplyTurnRecord(const nlohmann::json& row);
    void ApplyMatchRecord(const nlohmann::json& row);
    void ApplyBroadcast(const nlohmann::json& envelope);
    void ApplyLiveAimFromRecord(const nlohmann::json& row);
    static RemoteTurnResult TurnFromJson(const nlohmann::json& row);

    std::string matchId;
    int myPlayerNumber = 1;
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
    int cachedCurrentTurnPlayer_ = 1;
    DisconnectResult pendingDisconnect_ = DisconnectResult::None;
    bool disconnectReported_ = false;
    LiveAimState opponentAim_{};
    bool hasPendingPowerupPickup_ = false;
    LivePowerupPickup pendingPowerupPickup_{};

    bool pendingLiveShotStart_ = false;
    LiveShotStart pendingLiveShot_{};
    int liveShotId_ = 0;
    std::deque<ProjSample> liveSamples_;
    bool liveShotEnded_ = false;
    float liveShotEndX_ = 0.0f;
    float liveShotEndY_ = 0.0f;

    std::atomic<bool> active_{false};
    std::atomic<bool> pollInFlight_{false};
    std::atomic<bool> awaitingOpponentTurn_{false};

    RealtimeClient realtime_;

    static constexpr float POLL_INTERVAL_REALTIME_SEC = 5.0f;
    static constexpr float POLL_INTERVAL_FALLBACK_SEC = 1.2f;
    static constexpr float LIVE_AIM_PUBLISH_INTERVAL_SEC = 0.05f;
    static constexpr float LIVE_AIM_HTTP_INTERVAL_SEC = 0.35f;
    static constexpr float PROJ_PUBLISH_INTERVAL_SEC = 0.04f; // ~25 Hz
    static constexpr size_t MAX_LIVE_SAMPLES = 256;
};
