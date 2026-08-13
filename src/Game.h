#pragma once

#include <raylib.h>
#include <array>
#include <string>
#include <vector>

#include "GameTypes.h"
#include "MatchRoster.h"
#include "PhysicsWorld.h"
#include "Terrain.h"
#include "Cannon.h"
#include "Projectile.h"
#include "ParticleSystem.h"
#include "AI.h"
#include "PowerupSystem.h"
#include "ScreenEffects.h"
#include "Localization.h"
#include "Platform.h"
#include "ScrollList.h"
#include "VirtualScreen.h"

#include "net/PlayerIdentity.h"
#include "net/OnlineLobby.h"
#include "net/NetMatch.h"

class Game {
public:
    Game();
    ~Game();

    void Run();

private:
    void Update(float dt);
    void Draw();

    // --- telas de menu / lobby ---
    void UpdateMainMenu();
    void DrawMainMenu();
    void UpdateAbout();
    void DrawAbout();
    void UpdateInstructions();
    void DrawInstructions();
    void UpdateOnlineLobby();
    void DrawOnlineLobby();
    void UpdateOnlineTeamRoom();
    void DrawOnlineTeamRoom();

    // --- multiplayer online ---
    PlayerIdentity playerIdentity;
    OnlineLobby onlineLobby;
    NetMatch netMatch;
    void StartOnlineMatch(const MatchStart& ms);
    void StartSpectating(const ActiveMatchCard& match);
    void ApplyTurnSilently(const RemoteTurnResult& turn);
    void EndSpectatorMatch(int winnerPlayer);
    void ExitSpectatorToLobby();
    void ShutdownOnlinePresence();
    void EndOnlineMatchOpponentLeft();
    void BeginRemoteShotReplay(const RemoteTurnResult& remote);
    void UpdateRemoteShotReplay(float dt);
    void BeginRemoteProjectileLive(const LiveShotStart& shot);
    void UpdateRemoteProjectileLive(float dt);
    void FinishRemoteTurn(const RemoteTurnResult& remote);
    void ApplyOnlinePowerupSpawn(int type, float x);
    void TryOnlinePowerupSpawn(int completedTurn, int* outType = nullptr, float* outX = nullptr);
    void DrawOpponentAim(int shooterPlayer, float angleDeg, float power01) const;
    void ResetOpponentAimSim(int shooterPlayer);
    void UpdateOpponentAimSim(float dt);
    float SeededWind(int turnIndex) const;
    void ConsumeRemotePowerupPickups();
    void ConsumeRemotePowerupSpawns();
    void ApplyRemoteTurnDamage(const RemoteTurnResult& turn);
    void SyncCannonHealthFromTurn(const RemoteTurnResult& turn);
    void ApplyOnlineNamesFromMatchStart(const MatchStart& ms);
    int ResolveOnlineTurnPlayer(int proposedPlayer) const;
    void MaybeAdvancePastDeadOnlineTurn(float dt);

    // --- gameplay local ---
    void UpdateFormatSelect();
    void DrawFormatSelect();
    void StartMatch(GameMode mode, MatchFormat format = MatchFormat::Duel1v1);
    void ResetRound(unsigned int seed);
    void UpdateAiming();
    void UpdateProjectileFlight(float dt);
    void BeginGuidedFlight(Vector2 muzzle, const Cannon& target);
    Vector2 SampleGuidedPath(float t) const;
    void ResolveImpact(Vector2 impactPos, bool hitCannon, Cannon* hitTarget);
    void EndTurn();
    void CheckRoundEnd();
    float ComputeSafeMaxWindAccel() const;
    const char* ResolveRoundMessage() const;
    bool IsLocalHumanTurn() const;

    // --- UI in-game ---
    void DrawHUD();
    void DrawWindIndicator() const;
    void DrawMenuButton() const;
    bool HandleMenuButtonClick();
#if CANNON_DUEL_DEBUG_MODE
    void DrawDevPanelButton() const;
    bool HandleDevPanelButtonClick();
#endif
    void DrawMenuConfirmDialog() const;
    void UpdateMenuConfirmDialog();
    void DrawResetAngleButton() const;
    bool HandleResetAngleButtonClick();
    void DrawOnlineCannonLabels() const;
    void ReturnToOnlineLobbyAfterMatch();
    void ReturnToTeamRoomForRematch();
    void UpdateOnlineRoundOver();
    void DrawOnlineRoundOverOptions() const;
    void DrawSpectatorBanner() const;
    void DrawVersionSwitch(Vector2 mouse);
    void UpdateVersionSwitch(Vector2 mouse);
    void DrawLanguageFlags(Vector2 mouse);
    void UpdateLanguageFlags(Vector2 mouse);

    Color HudTextColor() const { return nightMode ? Color{235, 235, 235, 255} : Color{40, 30, 20, 255}; }
    Color HudTextColorDim() const { return nightMode ? Color{200, 200, 210, 255} : Color{60, 45, 30, 255}; }

    void PresentScreenWithDebug();

#if CANNON_DUEL_DEBUG_MODE
    void DrawDebugLogOverlay() const;
    bool UpdateDebugLogOverlay();
    bool debugLogVisible = false;
    int debugLogScrollIndex = 0;
    bool debugLogFollowTail = true;
    bool debugLogDragging = false;
    float debugLogDragStartY = 0.0f;
    int debugLogDragStartScroll = 0;
#endif

#if CANNON_DUEL_DEBUG_MODE
    bool devMode = false;
    float devPanelScrollY = 0.0f;
    bool devPanelScrollDragging = false;
    float devPanelScrollDragStartY = 0.0f;
    float devPanelScrollDragStart = 0.0f;
    bool UpdateDevPanel();
    void DrawDevPanel() const;
    void DevForceSpawnPowerup();
    void DevGrantPowerup(int playerNumber, PowerupType type);
    void DevHealAll();
    void DevSetWindZero();
    void DevSkipTurn();
    void ApplyDevCommand(const DevCommand& cmd);
    void DevBroadcastCommand(const std::string& action, int player = 0, int type = -1,
                             float x = 0.0f, float value = 0.0f);
#endif

    Cannon& GetCannon(int playerNum) { return roster.AtPlayerNum(ClampPlayerNum(playerNum)); }
    const Cannon& GetCannon(int playerNum) const { return roster.AtPlayerNum(ClampPlayerNum(playerNum)); }
    int ActiveSlot() const { return currentPlayer - 1; }
    int ClampPlayerNum(int playerNum) const;

    // --- estado geral ---
    GameState state = GameState::MainMenu;
    GameMode  mode  = GameMode::PvAI;
    MatchFormat matchFormat = MatchFormat::Duel1v1;
    GameMode pendingMatchMode = GameMode::PvAI;
    GameVersion version = GameVersion::Classic;
    Lang language = Lang::PT_BR;

    PhysicsWorld physics;
    Terrain terrain;
    MatchRoster roster;
    Projectile projectile;
    ParticleSystem particles;
    AI ai;
    PowerupSystem powerups;
    ScreenEffects effects;

    int currentPlayer = 1;
    float windForce = 0.0f;

    Sound sndFire{};
    Sound sndExplosion{};
    Music musicTracks[2]{};
    int currentMusicIndex = 0;
    bool audioReady = false;

    Texture2D texCannonLeft{};
    Texture2D texCannonRight{};
    Texture2D texCannon1{};
    Texture2D texCannon2{};
    Texture2D texCannon3{};
    Texture2D texCannon4{};
    Texture2D texBackground{};
    Texture2D texBackgroundNight{};
    Texture2D texProjectile{};
    bool spritesReady = false;
    bool nightMode = false;

    AimPhase aimPhase = AimPhase::Angle;
    float aimOscTimer = 0.0f;
    float stateTimer = 0.0f;
    RoundOutcome roundOutcome = RoundOutcome::None;
    bool showMenuConfirm = false;

    RenderTexture2D virtualScreen{};
    Vector2 prevProjectilePos{};
    float guidedPathT = 0.0f;
    Vector2 guidedPathStart{};
    Vector2 guidedPathApex{};
    Vector2 guidedPathTarget{};

    int guidedTargetPlayerNum = 2;

    bool onlineNameEditing = false;
    std::string onlineNameEditBuffer;
    float onlineNameEditCursorBlink = 0.0f;

    unsigned int onlineSeed = 0;
    int lastOnlineSpawnTurn_ = 0;
    Vector2 remoteReplayPos{};
    float remoteReplayT = 0.0f;
    float remoteReplayAimTimer = 0.0f;
    RemoteTurnResult pendingRemoteTurn{};
    bool remoteLiveActive = false;
    int remoteLiveShotId = 0;
    int remoteLiveLastSeq = -1;
    float remoteLivePlayT = 0.0f;
    float remoteLiveBufferDelay = 0.08f;
    bool remoteLivePlayStarted = false;
    bool remoteLiveHasPendingResult = false;
    float remoteLiveWatchTimer = 0.0f;
    std::vector<ProjSample> remoteLiveSamples;
    Vector2 remoteLivePos{};
    int opponentAimPlayer = 0;
    float opponentAimAngle = 45.0f;
    float opponentAimPower = 0.5f;
    float opponentAimTargetAngle = 45.0f;
    float opponentAimTargetPower = 0.5f;
    bool opponentAimHasLiveTarget = false;
    float opponentAimOscTimer = 0.0f;
    bool opponentAimSimActive = false;
    int opponentAimForTurn = 0;
    bool onlineWinByDisconnect = false;
    bool isSpectating = false;
    float onlineDeadTurnSkipCooldown_ = 0.0f;

    MatchComposition matchComposition = { 1, 1 };
    std::array<std::string, MatchRoster::kMaxCannons> onlinePlayerNames{};

    ScrollListState onlineLobbyScroll_;
    ScrollListState onlineTeamInviteScroll_;

    bool IsTeamGame() const {
        if (mode == GameMode::Online) return matchComposition.IsTeamGame();
        return IsTeamMode(matchFormat);
    }
};
