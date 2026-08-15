#include "Game.h"
#include "AssetPath.h"
#include "Config.h"
#include "CosmeticShaders.h"
#include "DebugLog.h"
#include "GameRand.h"
#include "ShopCatalog.h"
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

#if CANNON_DUEL_WEB_BUILD
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

Game::Game() {
#if CANNON_DUEL_DEBUG_MODE
    DebugLog::InstallOverlayCapture();
#endif
    InitWindow(cfg::WINDOW_WIDTH, cfg::WINDOW_HEIGHT, "Cannon Duel");
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
        musicTracks[2] = LoadMusicStream(AssetPath("sounds/music_valley.ogg").c_str());
        for (Music& m : musicTracks) {
            m.looping = true;
            if (m.frameCount > 0) SetMusicVolume(m, 0.5f);
        }
    }

    texCannonLeft   = LoadTexture(AssetPath("sprites/cannon_left.png").c_str());
    texCannonRight  = LoadTexture(AssetPath("sprites/cannon_right.png").c_str());
    for (size_t i = 0; i < texCannonColors.size(); ++i) {
        texCannonColors[i] = LoadTexture(AssetPath(kCannonColorFiles[i]).c_str());
    }
    for (size_t i = 0; i < texCannonOverlays.size(); ++i) {
        texCannonOverlays[i] = LoadTexture(AssetPath(kCannonSkinFiles[i]).c_str());
    }
    texBackground       = LoadTexture(AssetPath("sprites/background.png").c_str());
    texBackgroundNight  = LoadTexture(AssetPath("sprites/background_night.png").c_str());
    texBackgroundValley = LoadTexture(AssetPath("sprites/background_valley.png").c_str());
    texProjectile   = LoadTexture(AssetPath("sprites/projectile.png").c_str());
    for (size_t i = 0; i < texAmmo.size(); ++i) {
        texAmmo[i] = LoadTexture(AssetPath(kAmmoSpriteFiles[i]).c_str());
    }

    spritesReady = (texCannonLeft.id != 0 && texCannonRight.id != 0);

    srand(static_cast<unsigned int>(time(nullptr)));

    effects.Init();

    gCosmeticShaders.Init();

    playerIdentity.LoadOrCreate();
    SupabaseClient::ProbeCaBundle();
    onlineLobby.Init(&playerIdentity);
    wallet.Init(&playerIdentity);
}

Game::~Game() {
    ShutdownOnlinePresence();

    gCosmeticShaders.Shutdown();

    if (!netMatch.InMatch()) {
        GlobalNetWorker().Stop();
    }

    if (audioReady) {
        UnloadSound(sndFire);
        UnloadSound(sndExplosion);
        for (Music& m : musicTracks) UnloadMusicStream(m);
        CloseAudioDevice();
    }
    UnloadTexture(texCannonLeft);
    UnloadTexture(texCannonRight);
    for (Texture2D& tex : texCannonColors) UnloadTexture(tex);
    for (Texture2D& tex : texCannonOverlays) UnloadTexture(tex);
    UnloadTexture(texBackground);
    UnloadTexture(texBackgroundNight);
    UnloadTexture(texBackgroundValley);
    UnloadTexture(texProjectile);
    for (Texture2D& tex : texAmmo) UnloadTexture(tex);
    UnloadRenderTexture(virtualScreen);
    CloseWindow();
}

#if CANNON_DUEL_WEB_BUILD
void Game::WebMainLoopStep(void* userData) {
    Game* game = static_cast<Game*>(userData);
    SyncWebCanvasSize();
    float dt = GetFrameTime();
    game->Update(dt);
    game->Draw();
}
#endif

void Game::Run() {
#if CANNON_DUEL_WEB_BUILD
    // WASM roda dentro do loop de eventos do browser — não dá pra ter um
    // while(...) bloqueante aqui. emscripten_set_main_loop_arg chama
    // WebMainLoopStep uma vez por requestAnimationFrame (fps=0) e devolve
    // o controle pro navegador entre frames.
    emscripten_set_main_loop_arg(WebMainLoopStep, this, 0, 1);
#else
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
#endif
}
// ---------------------------------------------------------------------------
// Setup de partida
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

int Game::ClampPlayerNum(int playerNum) const {
    const int maxPlayer = std::max(1, roster.CannonCount());
    if (playerNum < 1 || playerNum > maxPlayer) {
        DebugLogf(LOG_WARNING, "GAME: playerNum %d fora do intervalo 1..%d", playerNum, maxPlayer);
        return std::clamp(playerNum, 1, maxPlayer);
    }
    return playerNum;
}

void Game::ApplyScenario(Scenario next, bool rebuildTerrain) {
    scenario = next;
    if (rebuildTerrain && roster.CannonCount() > 0 && state != GameState::MainMenu) {
        terrain.Generate(roundSeed_, scenario);
        terrain.RebuildPhysicsBody(physics.Id());
        for (int i = 0; i < roster.CannonCount(); ++i) {
            Cannon& c = roster.At(i);
            c.groundY = terrain.HeightAt(c.x);
        }
    } else {
        terrain.SetScenario(scenario);
    }
    if (audioReady) {
        StopMusicStream(musicTracks[currentMusicIndex]);
        PickMatchMusic();
        if (musicTracks[currentMusicIndex].frameCount > 0) {
            PlayMusicStream(musicTracks[currentMusicIndex]);
        }
    } else {
        PickMatchMusic();
    }
}

void Game::ResetRound(unsigned int seed) {
    roundSeed_ = seed;
    if (mode == GameMode::Online) {
        scenario = ScenarioFromSeed(seed);
    } else {
        scenario = static_cast<Scenario>(rand() % kScenarioCount);
    }
    terrain.Generate(seed, scenario);
    terrain.RebuildPhysicsBody(physics.Id());

    if (mode == GameMode::Online) {
        roster.SetupComposition(matchComposition, terrain);
        for (int i = 0; i < roster.CannonCount(); ++i) {
            Cannon& c = roster.At(i);
            if (c.health <= 0.0f) {
                DebugLogf(LOG_WARNING, "ROSTER: slot %d com vida zerada apos setup — corrigindo", i);
                c.health = cfg::CANNON_MAX_HEALTH;
            }
        }
    } else {
        roster.Setup(matchFormat, terrain);
    }

    currentPlayer = 1;
    if (mode == GameMode::Online) {
        onlineSeed = seed;
        windForce = SeededWind(0);
    } else {
        windForce = RandF(-1.0f, 1.0f) * std::min(cfg::WIND_MAX_ACCEL, ComputeSafeMaxWindAccel());
    }
    PickMatchMusic();
    aimPhase = AimPhase::Angle;
    aimOscTimer = 0.0f;

    powerups.Reset();
    powerups.SetSpawnEveryTurns(cfg::POWERUP_SPAWN_EVERY_TURNS);
    lastOnlineSpawnTurn_ = 0;
    remoteReplayT = 0.0f;
    remoteReplayAimTimer = 0.0f;
    opponentAimPlayer = 0;
    opponentAimSimActive = false;
    opponentAimForTurn = 0;
    opponentAimHasLiveTarget = false;
    opponentAimTargetAngle = 45.0f;
    opponentAimTargetPower = 0.0f;
    onlineWinByDisconnect = false;
    roundCoinsAwarded_ = false;

    state = GameState::Aiming;

    if (mode == GameMode::Online) {
        ApplyOnlineCannonCosmetics();
    } else {
        ApplyEquippedCosmetics();
    }
}

void Game::ApplyCannonCosmetics(Cannon& cannon, const std::string& colorId,
                                const std::string& skinId, const std::string& effectId,
                                const std::string& ammoId) {
    cannon.colorIndex = 0;
    cannon.skinOverlayIndex = 0;
    cannon.cannonEffect = CannonEffectStyle::None;
    cannon.effectAccent = WHITE;
    cannon.ammoStyle = AmmoStyle::Default;

    const ShopItem* colorItem = FindShopItem(colorId);
    if (colorItem && colorItem->category == ShopCategory::CannonColor && colorId != kDefaultCannonColorId) {
        cannon.colorIndex = colorItem->colorIndex;
    }

    const ShopItem* skinItem = FindShopItem(skinId);
    if (skinItem && skinItem->category == ShopCategory::CannonSkin && skinId != kDefaultCannonSkinId) {
        cannon.skinOverlayIndex = skinItem->skinOverlayIndex;
    }

    const ShopItem* effectItem = FindShopItem(effectId);
    if (effectItem && effectItem->category == ShopCategory::CannonEffect && effectId != kDefaultCannonEffectId) {
        cannon.cannonEffect = effectItem->cannonEffect;
        cannon.effectAccent = effectItem->accent;
    }

    const ShopItem* ammoItem = FindShopItem(ammoId);
    if (ammoItem && ammoItem->category == ShopCategory::Ammo) {
        cannon.ammoStyle = ammoItem->ammoStyle;
    }
}

const Texture2D* Game::ResolveAmmoTexture(AmmoStyle style) const {
    const int idx = AmmoSpriteIndex(style);
    if (idx >= 0 && idx < static_cast<int>(texAmmo.size()) && texAmmo[static_cast<size_t>(idx)].id != 0) {
        return &texAmmo[static_cast<size_t>(idx)];
    }
    return (texProjectile.id != 0) ? &texProjectile : nullptr;
}

Texture2D* Game::ResolveCannonTexture(int rosterSlot) {
    Cannon& c = roster.At(rosterSlot);
    if (c.colorIndex > 0 && c.colorIndex <= static_cast<int>(texCannonColors.size())) {
        Texture2D& colorTex = texCannonColors[static_cast<size_t>(c.colorIndex - 1)];
        if (colorTex.id != 0) return &colorTex;
    }
    if (!spritesReady) return nullptr;
    return (c.side == CannonSide::Left) ? &texCannonLeft : &texCannonRight;
}

Texture2D* Game::ResolveCannonOverlay(int rosterSlot) {
    Cannon& c = roster.At(rosterSlot);
    if (c.skinOverlayIndex <= 0 || c.skinOverlayIndex > static_cast<int>(texCannonOverlays.size())) {
        return nullptr;
    }
    Texture2D& overlay = texCannonOverlays[static_cast<size_t>(c.skinOverlayIndex - 1)];
    return (overlay.id != 0) ? &overlay : nullptr;
}

void Game::ApplyEquippedCosmetics() {
    if (mode == GameMode::Online) return;

    const int count = roster.CannonCount();
    for (int i = 0; i < count; ++i) {
        if (roster.IsAISlot(i, mode)) {
            ApplyCannonCosmetics(roster.At(i), kDefaultCannonColorId,
                                 kDefaultCannonSkinId, kDefaultCannonEffectId, kDefaultAmmoId);
        } else {
            ApplyCannonCosmetics(roster.At(i), wallet.EquippedCannonColor(),
                                 wallet.EquippedCannonSkin(), wallet.EquippedCannonEffect(),
                                 wallet.EquippedAmmo());
        }
    }
}

void Game::ApplyOnlineCannonCosmetics() {
    const int count = std::min(roster.CannonCount(), MatchRoster::kMaxCannons);
    for (int i = 0; i < count; ++i) {
        ApplyCannonCosmetics(roster.At(i),
                             onlineEquippedCannonColors[static_cast<size_t>(i)],
                             onlineEquippedCannonSkins[static_cast<size_t>(i)],
                             onlineEquippedCannonEffects[static_cast<size_t>(i)],
                             onlineEquippedAmmo[static_cast<size_t>(i)]);
    }
}

bool Game::IsLocalHumanShooter() const {
    if (mode == GameMode::Online) {
        return currentPlayer == netMatch.MyPlayerNumber();
    }
    if (mode == GameMode::PvAI) {
        return !roster.IsAISlot(ActiveSlot(), mode);
    }
    return true;
}

void Game::AwardCoinsWithPopupAt(Vector2 pos, int amount) {
    if (amount <= 0) return;
    wallet.AwardCoins(amount);
    coinPopups.Spawn(pos, amount);
}

void Game::AwardCoinsWithPopup(int playerNum, int amount) {
    if (amount <= 0 || playerNum < 1 || playerNum > roster.CannonCount()) return;
    const Cannon& c = GetCannon(playerNum);
    AwardCoinsWithPopupAt({ c.x, c.groundY - cfg::CANNON_BODY_RADIUS_PX - 70.0f }, amount);
}

int Game::RoundEndCoinAmount() const {
    if (isSpectating) return 0;
    if (roundOutcome == RoundOutcome::None
        || roundOutcome == RoundOutcome::Draw
        || roundOutcome == RoundOutcome::DrawBuried) {
        return 0;
    }

    bool won = false;
    if (mode == GameMode::Online) {
        int winnerPlayer = 0;
        switch (roundOutcome) {
            case RoundOutcome::P1Wins:
            case RoundOutcome::P1WinsBuried:
            case RoundOutcome::TeamAWins:
            case RoundOutcome::TeamAWinsBuried:
                winnerPlayer = 1;
                break;
            case RoundOutcome::P2Wins:
            case RoundOutcome::P2WinsBuried:
            case RoundOutcome::TeamBWins:
            case RoundOutcome::TeamBWinsBuried:
                winnerPlayer = 2;
                break;
            default:
                break;
        }
        won = OnlineDidIWin(winnerPlayer, netMatch.MyPlayerNumber(), matchComposition);
    } else if (mode == GameMode::PvAI) {
        switch (roundOutcome) {
            case RoundOutcome::P1Wins:
            case RoundOutcome::P1WinsBuried:
            case RoundOutcome::TeamAWins:
            case RoundOutcome::TeamAWinsBuried:
                won = true;
                break;
            default:
                won = false;
                break;
        }
    } else {
        won = true;
    }
    return won ? cfg::COINS_ROUND_WIN : cfg::COINS_ROUND_LOSS;
}

void Game::OnRoundEndedAwardCoins() {
    if (roundCoinsAwarded_ || roundOutcome == RoundOutcome::None || isSpectating) return;
    roundCoinsAwarded_ = true;

    const int amount = RoundEndCoinAmount();
    if (amount <= 0) return;

    const int popupPlayer = (mode == GameMode::Online) ? netMatch.MyPlayerNumber() : 1;
    AwardCoinsWithPopup(popupPlayer, amount);
}

void Game::StartMatch(GameMode m, MatchFormat format) {
    mode = m;
    matchFormat = format;
    if (mode == GameMode::PvAI) ai.SetDifficulty(0.55f);
    ResetRound(static_cast<unsigned int>(time(nullptr)) ^ rand());

    if (audioReady && musicTracks[currentMusicIndex].frameCount > 0) {
        PlayMusicStream(musicTracks[currentMusicIndex]);
    }
}

void Game::PickMatchMusic() {
    switch (scenario) {
        case Scenario::Night: currentMusicIndex = 1; break;
        case Scenario::ValleyOfTheEnd: currentMusicIndex = 2; break;
        default: currentMusicIndex = 0; break;
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

    // No-op fora do build web (lá a fila de rede roda numa thread de
    // fundo de verdade); no web, drena um job de HTTP por frame já que
    // não há thread bloqueante disponível (ver NetWorker::Tick).
    GlobalNetWorker().Tick();

    // Precisa ser chamado todo frame pro streaming da música avançar (e
    // fazer o loop) — independe de qualquer outro estado/painel.
    if (audioReady && musicTracks[currentMusicIndex].frameCount > 0) {
        UpdateMusicStream(musicTracks[currentMusicIndex]);
    }

    // Multiplayer online: rede assíncrona (sem bloquear o frame).
    if (mode == GameMode::Online && netMatch.InMatch() && state != GameState::RoundOver) {
        if (!isSpectating) {
            onlineLobby.HeartbeatInMatch(dt);
        }
        netMatch.Pump(dt);
        currentPlayer = ResolveOnlineTurnPlayer(netMatch.SyncedCurrentTurnPlayer());
        MaybeAdvancePastDeadOnlineTurn(dt);

        DisconnectResult disc = netMatch.PollDisconnect();
        if (!isSpectating && disc != DisconnectResult::None) {
            EndOnlineMatchOpponentLeft();
            return;
        }

        if (isSpectating) {
            int winner = 0;
            if (netMatch.PollSpectatorMatchEnded(winner)) {
                EndSpectatorMatch(winner);
                return;
            }
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

        RemoteTurnResult healthResync;
        while (netMatch.PollHealthResync(healthResync)) {
            SyncCannonHealthFromTurn(healthResync);
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
        state != GameState::OnlineLobby && state != GameState::Shop && HandleMenuButtonClick()) {
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
        coinPopups.Update(dt);
    }

    // Poeira ambiente: atualiza durante o jogo (não no menu, onde não é
    // mais desenhada).
    if (state != GameState::MainMenu) {
        effects.Update(dt, windForce);
    }

        effects.UpdateShake(dt);
        powerups.UpdateMessageTimer(dt);
        powerups.UpdatePinnedTooltip(dt);

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
        case GameState::OnlineTeamRoom:
            UpdateOnlineTeamRoom();
            break;
        case GameState::Shop:
            UpdateShop();
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
            OnRoundEndedAwardCoins();
            if (mode == GameMode::Online && !isSpectating) {
                UpdateOnlineRoundOver();
            } else {
                stateTimer -= dt;
                if (stateTimer <= 0.0f && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (isSpectating) {
                        ExitSpectatorToLobby();
                    } else {
                        if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
                        if (mode == GameMode::Online) {
                            netMatch.LeaveMatch();
                            onlineLobby.ReturnToLobbyAfterMatch();
                            state = GameState::OnlineLobby;
                        } else {
                            state = GameState::MainMenu;
                        }
                    }
                }
            }
            break;
    }
}
