#include "Game.h"
#include "AssetPath.h"
#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"
#include "VirtualScreen.h"
#include "net/NetWorker.h"
#include "net/SupabaseClient.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

Game::Game() {
#if CANNON_DUEL_DEBUG_MODE
    DebugLog::InstallOverlayCapture();
#endif
    InitWindow(cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, "Cannon Duel");
    SetTargetFPS(cfg::TARGET_FPS);

    // No Android, InitWindow ignora a largura/altura pedidas e usa sempre a
    // resolução nativa da tela do aparelho em tela cheia — por isso
    // renderizamos numa textura de resolução fixa e escalamos na hora de
    // mostrar (ver DrawVirtualScreenScaled / GetVirtualMouse).
    virtualScreen = LoadRenderTexture(cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT);
    SetTextureFilter(virtualScreen.texture, TEXTURE_FILTER_BILINEAR);

    InitAudioDevice();
    audioReady = IsAudioDeviceReady();
    if (audioReady) {
        sndFire      = LoadSound(AssetPath("sounds/fire.ogg").c_str());
        sndExplosion = LoadSound(AssetPath("sounds/explosion.ogg").c_str());

        musicTracks[0] = LoadMusicStream(AssetPath("sounds/music.ogg").c_str());
        musicTracks[1] = LoadMusicStream(AssetPath("sounds/music2.ogg").c_str());
        for (Music& m : musicTracks) {
            m.looping = true;
            if (m.frameCount > 0) SetMusicVolume(m, 0.5f);
        }
    }

    texCannonLeft   = LoadTexture(AssetPath("sprites/cannon_left.png").c_str());
    texCannonRight  = LoadTexture(AssetPath("sprites/cannon_right.png").c_str());
    texBackground   = LoadTexture(AssetPath("sprites/background.png").c_str());
    texBackgroundNight = LoadTexture(AssetPath("sprites/background_night.png").c_str());
    texProjectile   = LoadTexture(AssetPath("sprites/projectile.png").c_str());

    spritesReady = (texCannonLeft.id != 0 && texCannonRight.id != 0);

    srand(static_cast<unsigned int>(time(nullptr)));

    effects.Init();

    playerIdentity.LoadOrCreate();
    SupabaseClient::ProbeCaBundle();
    onlineLobby.Init(&playerIdentity);
}

Game::~Game() {
    if (state == GameState::OnlineLobby) {
        onlineLobby.LeaveLobby();
    }
    if (netMatch.InMatch()) {
        netMatch.LeaveMatch();
    } else {
        GlobalNetWorker().Stop();
    }

    if (audioReady) {
        UnloadSound(sndFire);
        UnloadSound(sndExplosion);
        UnloadMusicStream(musicTracks[0]);
        UnloadMusicStream(musicTracks[1]);
        CloseAudioDevice();
    }
    UnloadTexture(texCannonLeft);
    UnloadTexture(texCannonRight);
    UnloadTexture(texBackground);
    UnloadTexture(texBackgroundNight);
    UnloadTexture(texProjectile);
    UnloadRenderTexture(virtualScreen);
    CloseWindow();
}

void Game::Run() {
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
}
// ---------------------------------------------------------------------------
// Setup de partida
// ---------------------------------------------------------------------------
float Game::ComputeSafeMaxWindAccel() const {
    // Alcance máximo (sem vento) na potência máxima, ângulo ótimo de 45°:
    //   R = v² / g
    // Com vento constante 'a' contra a trajetória a 45°, o alcance efetivo é:
    //   x(T) = (v²/g) * (1 - a/g)
    // (derivado de x(T) = v*cos45*T - 0.5*a*T², T = v*sqrt(2)/g)
    //
    // Queremos garantir x(T) >= D * safetyFactor, ou seja, mesmo no pior
    // vento possível (soprando contra, o tempo todo, no ângulo mais
    // desfavorável) ainda sobra alcance para acertar com folga — cobrindo
    // imprecisões de ângulo/tempo de clique do jogador.
    float distPx = roster.MaxEnemyDistancePx(0);
    if (distPx < 1.0f) distPx = std::fabs(GetCannon(2).x - GetCannon(1).x);
    float D = cfg::PxToM(distPx);
    float v = cfg::MAX_POWER;
    float g = cfg::GRAVITY_MPS2;
    constexpr float safetyFactor = 1.25f; // exige 25% de alcance extra de folga

    float requiredRange = D * safetyFactor;
    float maxRangeNoWind = (v * v) / g;

    if (maxRangeNoWind <= requiredRange) {
        // Config atual (potência/distância) já não teria folga nem sem
        // vento; não há aceleração de vento segura — melhor não arriscar.
        return 0.0f;
    }

    float a = g - requiredRange * (g * g) / (v * v);
    return std::max(0.0f, a);
}

void Game::ResetRound(unsigned int seed) {
    terrain.GenerateRandom(seed);
    terrain.RebuildPhysicsBody(physics.Id());

    roster.Setup(matchFormat, terrain);

    currentPlayer = 1;
    if (mode == GameMode::Online) {
        onlineSeed = seed;
        onlineCompletedTurns = 0;
        nightMode = ((seed >> 17) & 1u) == 0;
        windForce = SeededWind(0);
        currentMusicIndex = static_cast<int>((seed >> 25) % 2);
    } else {
        windForce = RandF(-1.0f, 1.0f) * std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());
        nightMode = (rand() % 2) == 0;
    }
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;

    powerups.Reset();
    powerups.SetSpawnEveryTurns(cfg::POWERUP_SPAWN_EVERY_TURNS * roster.PerTeam());
    remoteReplayT = 0.0f;
    remoteReplayAimTimer = 0.0f;
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    opponentAimForTurn = 0;
    opponentAimHasLiveTarget = false;
    opponentAimTargetAngle = 45.0f;
    opponentAimTargetPower = 0.0f;
    onlineWinByDisconnect = false;

    state = GameState::Aiming;
}

void Game::StartMatch(GameMode m, MatchFormat format) {
    mode = m;
    matchFormat = format;
    if (mode == GameMode::PvAI) ai.SetDifficulty(0.55f);
    ResetRound(static_cast<unsigned int>(time(nullptr)) ^ rand());

    // Música toca em loop só durante a partida (não no menu) — faixa
    // sorteada aleatoriamente a cada partida.
    currentMusicIndex = rand() % 2;
    if (audioReady && musicTracks[currentMusicIndex].frameCount > 0) {
        PlayMusicStream(musicTracks[currentMusicIndex]);
    }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
const char* Game::ResolveRoundMessage() const {
    if (onlineWinByDisconnect) return T(TK::RoundOpponentDisconnected, language);
    switch (roundOutcome) {
        case RoundOutcome::Draw: return T(TK::RoundDraw, language);
        case RoundOutcome::DrawBuried: return T(TK::RoundDrawBuried, language);
        case RoundOutcome::P1Wins: return T(TK::RoundP1Wins, language);
        case RoundOutcome::P2Wins: return T(TK::RoundP2Wins, language);
        case RoundOutcome::P1WinsBuried: return T(TK::RoundP1WinsBuried, language);
        case RoundOutcome::P2WinsBuried: return T(TK::RoundP2WinsBuried, language);
        case RoundOutcome::TeamAWins: return T(TK::RoundTeamAWins, language);
        case RoundOutcome::TeamBWins: return T(TK::RoundTeamBWins, language);
        case RoundOutcome::TeamAWinsBuried: return T(TK::RoundTeamAWinsBuried, language);
        case RoundOutcome::TeamBWinsBuried: return T(TK::RoundTeamBWinsBuried, language);
        default: return "";
    }
}

void Game::Update(float dt) {
#if CANNON_DUEL_DEBUG_MODE
    if (UpdateDebugLogOverlay()) {
        return;
    }
#endif

    // Precisa ser chamado todo frame pro streaming da música avançar (e
    // fazer o loop) — independe de qualquer outro estado/painel.
    if (audioReady && musicTracks[currentMusicIndex].frameCount > 0) {
        UpdateMusicStream(musicTracks[currentMusicIndex]);
    }

    // Multiplayer online: rede assíncrona (sem bloquear o frame).
    if (mode == GameMode::Online && netMatch.InMatch() && state != GameState::RoundOver) {
        netMatch.Pump(dt);
        currentPlayer = netMatch.SyncedCurrentTurnPlayer();

        DisconnectResult disc = netMatch.PollDisconnect();
        if (disc == DisconnectResult::OpponentLeft) {
            EndOnlineMatchOpponentLeft();
            return;
        }

#if CANNON_DUEL_DEBUG_MODE
        DevCommand devCmd;
        while (netMatch.PollDevCommand(devCmd)) {
            ApplyDevCommand(devCmd);
        }
#endif

        // Stream em tempo real tem prioridade sobre o replay fake.
        if (state != GameState::RemoteShotReplay && state != GameState::RemoteProjectileLive) {
            LiveShotStart liveShot;
            if (netMatch.PollLiveShotStart(liveShot)) {
                BeginRemoteProjectileLive(liveShot);
                return;
            }
        }

        // Coleta de power-up anunciada pelo adversário (some do mapa na hora).
        if (version == GameVersion::Plus) {
            ConsumeRemotePowerupPickups();
        }

        if (state != GameState::RemoteShotReplay && state != GameState::RemoteProjectileLive) {
            RemoteTurnResult remote;
            if (netMatch.PollOpponentTurn(remote)) {
                // Sem stream: fallback no arco interpolado antigo.
                BeginRemoteShotReplay(remote);
                return;
            }
        } else if (state == GameState::RemoteProjectileLive) {
            // Também consumido dentro de UpdateRemoteProjectileLive; aqui é backup.
            RemoteTurnResult remote;
            if (!remoteLiveHasPendingResult && netMatch.PollOpponentTurn(remote)) {
                pendingRemoteTurn = remote;
                remoteLiveHasPendingResult = true;
            }
        }

        if (netMatch.IsMyTurn()) {
            opponentAimPlayer = 0;
            opponentAimSimActive = false;
            opponentAimHasLiveTarget = false;
        } else if (state == GameState::Aiming || state == GameState::TurnTransition) {
            int waitingTurn = netMatch.TurnsCompleted() + 1;
            if (waitingTurn != opponentAimForTurn) {
                ResetOpponentAimSim(netMatch.SyncedCurrentTurnPlayer());
                opponentAimForTurn = waitingTurn;
            }

            // Preferir mira real via Realtime; interpolar suavemente até o alvo.
            LiveAimState live = netMatch.GetOpponentLiveAim();
            if (live.valid && live.player != 0) {
                opponentAimPlayer = live.player;
                opponentAimTargetAngle = live.angleDeg;
                opponentAimTargetPower = std::clamp(live.power01, 0.0f, 1.0f);
                opponentAimHasLiveTarget = true;
                opponentAimSimActive = false;
            } else if (!opponentAimHasLiveTarget) {
                UpdateOpponentAimSim(dt);
            }

            if (opponentAimHasLiveTarget && opponentAimPlayer != 0) {
                // Follow exponencial — elimina “degraus” dos broadcasts ~12 Hz.
                const float follow = 1.0f - std::exp(-dt * 22.0f);
                opponentAimAngle += (opponentAimTargetAngle - opponentAimAngle) * follow;
                opponentAimPower += (opponentAimTargetPower - opponentAimPower) * follow;
            }
        }
    }

#if CANNON_DUEL_DEBUG_MODE
#if !CANNON_DUEL_ANDROID_BUILD
    if (IsKeyPressed(KEY_F9)) {
        devMode = !devMode;
    }
#endif
    if (HandleDevPanelButtonClick()) {
        return;
    }
    if (devMode && UpdateDevPanel()) {
        return;
    }
#endif

    // Diálogo de confirmação tem prioridade máxima: enquanto aberto, nenhum
    // outro input do jogo é processado.
    if (showMenuConfirm) {
        UpdateMenuConfirmDialog();
        return;
    }

    // Botão "voltar ao menu" abre o diálogo de confirmação em vez de sair
    // direto — evita perder uma partida em andamento por clique acidental.
    if (state != GameState::MainMenu && state != GameState::About && state != GameState::Instructions &&
        state != GameState::OnlineLobby && HandleMenuButtonClick()) {
        showMenuConfirm = true;
        return;
    }

    // Botão in-game "resetar linha de ângulo" — só faz sentido durante a
    // mira de um jogador humano (não durante o turno da IA). Equivalente
    // touch-friendly do atalho de teclado B (PC), mas disponível nas duas
    // plataformas.
    bool humanAimingTurn = IsLocalHumanTurn();
    if (humanAimingTurn && HandleResetAngleButtonClick()) {
        if (aimPhase == AimPhase::Angle) {
            aimOscTimer = 0.0f;
        } else {
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
        }
        return;
    }

    // Atualiza as partículas em TODOS os estados (exceto o menu, onde não há
    // partida em andamento). Antes isso só rodava durante ProjectileFlying/
    // RoundOver, então uma explosão ficava "congelada" na tela durante a
    // transição de turno e a mira do próximo jogador, só sumindo quando o
    // próximo tiro fosse disparado.
    if (state != GameState::MainMenu) {
        particles.Update(dt);
    }

    // Poeira ambiente: atualiza durante o jogo (não no menu, onde não é
    // mais desenhada).
    if (state != GameState::MainMenu) {
        effects.Update(dt, windForce);
    }

        effects.UpdateShake(dt);
        powerups.UpdateMessageTimer(dt);

    switch (state) {
        case GameState::MainMenu:
            UpdateMainMenu();
            break;
        case GameState::FormatSelect:
            UpdateFormatSelect();
            break;
        case GameState::About:
            UpdateAbout();
            break;
        case GameState::Instructions:
            UpdateInstructions();
            break;
        case GameState::OnlineLobby:
            UpdateOnlineLobby();
            break;
        case GameState::Aiming:
            UpdateAiming();
            break;
        case GameState::ProjectileFlying:
            UpdateProjectileFlight(dt);
            break;
        case GameState::RemoteShotReplay:
            UpdateRemoteShotReplay(dt);
            break;
        case GameState::RemoteProjectileLive:
            UpdateRemoteProjectileLive(dt);
            break;
        case GameState::TurnTransition:
            stateTimer -= dt;
            if (stateTimer <= 0.0f) state = GameState::Aiming;
            break;
        case GameState::RoundOver:
            stateTimer -= dt;
            if (stateTimer <= 0.0f && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
                if (mode == GameMode::Online) {
                    netMatch.LeaveMatch();
                    onlineLobby.LeaveLobby();
                }
                state = GameState::MainMenu;
            }
            break;
    }
}
