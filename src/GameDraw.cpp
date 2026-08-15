#include "Game.h"
#include "AssetPath.h"
#include "CannonUILayout.h"
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

void Game::PresentScreenWithDebug() {
    EndTextureMode();
    BeginDrawing();
#if !CANNON_DUEL_WEB_BUILD
    ClearBackground(BLACK);
#endif
    DrawVirtualScreenScaled(virtualScreen);
#if CANNON_DUEL_DEBUG_MODE
    DrawDebugLogOverlay();
#endif
    EndDrawing();
}

void Game::Draw() {
    // Fase 1: desenha tudo numa textura de resolução fixa (cfg::SCREEN_WIDTH
    // x cfg::SCREEN_HEIGHT) — todo o resto do código de desenho continua
    // usando essas coordenadas fixas, sem se importar com a resolução real
    // da janela/tela.
    BeginTextureMode(virtualScreen);
    ClearBackground(Color{ 235, 214, 190, 255 });

    if (state == GameState::MainMenu) {
        DrawMainMenu();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    if (state == GameState::About) {
        DrawAbout();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    if (state == GameState::Instructions) {
        DrawInstructions();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    if (state == GameState::FormatSelect) {
        DrawFormatSelect();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    if (state == GameState::OnlineLobby) {
        DrawOnlineLobby();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    if (state == GameState::OnlineTeamRoom) {
        DrawOnlineTeamRoom();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    if (state == GameState::Shop) {
        DrawShop();
#if CANNON_DUEL_DEBUG_MODE
        DrawDevPanelButton();
        if (devMode) DrawDevPanel();
#endif
        PresentScreenWithDebug();
        return;
    }

    // ---- background (sprite gerado: dia com sol/nuvens, ou noite com estrelas/lua) ----
    Texture2D& bgTex = nightMode ? texBackgroundNight : texBackground;
    if (spritesReady && bgTex.id != 0) {
        DrawTexture(bgTex, 0, 0, WHITE);
    } else {
        DrawCircle(cfg::SCREEN_WIDTH - 140, 110, 60, Color{255, 221, 130, 255});
        DrawRectangle(0, cfg::SCREEN_HEIGHT - 460, cfg::SCREEN_WIDTH, 40, Color{225, 200, 175, 180});
    }

    // Poeira ambiente: desenhada logo após o céu, antes do terreno/canhões —
    // fica como uma neblina atmosférica atrás da ação, sem atrapalhar a
    // leitura do jogo.
    effects.Draw();

    // Screen shake (versão Plus): tudo dentro do "mundo do jogo" (terreno,
    // canhões, projétil, partículas, power-up, indicadores de mira) é
    // desenhado com um leve deslocamento de câmera que decai com o tempo.
    // O HUD e os overlays de UI ficam FORA dessa câmera, sempre estáveis.
    Camera2D shakeCam = { 0 };
    shakeCam.target = { 0.0f, 0.0f };
    shakeCam.offset = effects.ShakeOffset();
    shakeCam.rotation = 0.0f;
    shakeCam.zoom = 1.0f;
    BeginMode2D(shakeCam);

    terrain.Draw();
    for (int i = 0; i < roster.CannonCount(); ++i) {
        const int playerNum = i + 1;
        const bool isActive = (currentPlayer == playerNum && state != GameState::RoundOver &&
                               roster.At(i).IsAlive());
        Texture2D* tex = ResolveCannonTexture(i);
        Texture2D* overlay = ResolveCannonOverlay(i);
        roster.At(i).Draw(isActive, tex, overlay);
    }

    coinPopups.Draw();

    if (version == GameVersion::Plus) {
        powerups.Draw(terrain);
        powerups.DrawTooltip(terrain, ::GetVirtualMouse(), language);
    }

    // Partículas (incluindo o rastro do projétil) desenhadas ANTES do
    // projétil em si — senão, como o rastro nasce exatamente na posição da
    // bala a cada frame, ele ficaria por cima e "pintaria" a bala com a cor
    // clara da fumaça, escondendo o preto original do sprite.
    particles.Draw();

    if (projectile.IsActive()) {
        Vector2 p = projectile.PositionPx();
        if (spritesReady && texProjectile.id != 0) {
            float r = cfg::PROJECTILE_RADIUS_PX;
            DrawTexturePro(texProjectile, {0, 0, (float)texProjectile.width, (float)texProjectile.height},
                           {p.x - r, p.y - r, r * 2, r * 2}, {0, 0}, 0.0f, WHITE);
        } else {
            DrawCircleV(p, cfg::PROJECTILE_RADIUS_PX, BLACK);
        }
    }

    if (state == GameState::RemoteShotReplay) {
        if (opponentAimPlayer != 0) {
            DrawOpponentAim(opponentAimPlayer, opponentAimAngle, opponentAimPower);
        }
        if (remoteReplayAimTimer <= 0.0f) {
            Vector2 p = remoteReplayPos;
            if (spritesReady && texProjectile.id != 0) {
                float r = cfg::PROJECTILE_RADIUS_PX;
                DrawTexturePro(texProjectile, {0, 0, (float)texProjectile.width, (float)texProjectile.height},
                               {p.x - r, p.y - r, r * 2, r * 2}, {0, 0}, 0.0f, WHITE);
            } else {
                DrawCircleV(p, cfg::PROJECTILE_RADIUS_PX, BLACK);
            }
        }
    }

    if (state == GameState::RemoteProjectileLive) {
        Vector2 p = remoteLivePos;
        if (spritesReady && texProjectile.id != 0) {
            float r = cfg::PROJECTILE_RADIUS_PX;
            DrawTexturePro(texProjectile, {0, 0, (float)texProjectile.width, (float)texProjectile.height},
                           {p.x - r, p.y - r, r * 2, r * 2}, {0, 0}, 0.0f, WHITE);
        } else {
            DrawCircleV(p, cfg::PROJECTILE_RADIUS_PX, BLACK);
        }
    }

    // mecanismo de mira: linha oscilando (fase ângulo) ou barra de força (fase potência)
    if (IsLocalHumanTurn()) {
        int activePlayer = (mode == GameMode::Online) ? netMatch.MyPlayerNumber() : currentPlayer;
        Cannon& active = GetCannon(activePlayer);
        Vector2 base = { active.x, active.groundY - cfg::CANNON_BODY_RADIUS_PX * 0.6f };

        // power-up "trajetória prevista": desenha o arco balístico estimado
        // com o ângulo/força atuais (simulação simplificada, incluindo vento)
        if (version == GameVersion::Plus && active.trajectoryPreviewTurnsLeft > 0) {
            Vector2 dir = active.AimDirection();
            float speed = cfg::MIN_POWER + active.power01 * (cfg::MAX_POWER - cfg::MIN_POWER);
            Vector2 simPos = active.MuzzlePosition();
            Vector2 simVel = { dir.x * speed * cfg::PPM, dir.y * speed * cfg::PPM };
            float simDt = 0.05f;
            float windPxAccel = windForce * cfg::PPM;
            float gravPxAccel = cfg::GRAVITY_MPS2 * cfg::PPM;
            Vector2 landingPos = simPos;
            for (int i = 0; i < 120; ++i) {
                simVel.x += windPxAccel * simDt;
                simVel.y += gravPxAccel * simDt;
                simPos.x += simVel.x * simDt;
                simPos.y += simVel.y * simDt;
                float groundY = terrain.HeightAt(simPos.x);
                if (simPos.y >= groundY || simPos.x < 0 || simPos.x > cfg::SCREEN_WIDTH) {
                    landingPos = { simPos.x, groundY };
                    break;
                }
                landingPos = simPos;
                if (i % 2 == 0) DrawCircleV(simPos, 2.5f, Fade(Color{60, 130, 220, 255}, 0.7f));
            }
            DrawCircleV(landingPos, 9.0f, Fade(Color{255, 220, 60, 255}, 0.92f));
            DrawCircleLines(static_cast<int>(landingPos.x), static_cast<int>(landingPos.y), 9, Color{40, 30, 10, 255});
        }

        if (aimPhase == AimPhase::Angle) {
            Vector2 dir = active.AimDirection();
            Vector2 tip = { base.x + dir.x * 100.0f, base.y + dir.y * 100.0f };
            DrawLineEx(base, tip, 3.0f, Fade(RED, 0.8f));
            DrawCircleV(tip, 4.0f, RED);
        } else {
            // barra de força acima do canhão: verde (fraco) -> vermelho (forte)
            float barW = 120.0f, barH = 16.0f;
            Vector2 barPos = { active.x - barW / 2, cannon_ui::PowerBarY(active, barH) };
            DrawRectangle(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                          static_cast<int>(barW), static_cast<int>(barH), Color{30, 30, 30, 220});

            int segments = 24;
            for (int i = 0; i < segments; ++i) {
                float t = (i + 0.5f) / segments;
                Color c = { static_cast<unsigned char>(GREEN.r + (RED.r - GREEN.r) * t),
                            static_cast<unsigned char>(GREEN.g + (RED.g - GREEN.g) * t),
                            static_cast<unsigned char>(GREEN.b + (RED.b - GREEN.b) * t),
                            255 };
                float segX = barPos.x + t * barW;
                DrawRectangle(static_cast<int>(segX), static_cast<int>(barPos.y),
                              static_cast<int>(barW / segments) + 1, static_cast<int>(barH), c);
            }
            DrawRectangleLines(static_cast<int>(barPos.x), static_cast<int>(barPos.y),
                                static_cast<int>(barW), static_cast<int>(barH), BLACK);

            // marcador da posição atual (oscilando)
            float markerX = barPos.x + active.power01 * barW;
            DrawRectangle(static_cast<int>(markerX) - 2, static_cast<int>(barPos.y) - 4,
                          4, static_cast<int>(barH) + 8, WHITE);
        }
    }
    else if (mode == GameMode::Online && opponentAimPlayer != 0 &&
             state != GameState::RemoteShotReplay && state != GameState::RemoteProjectileLive) {
        DrawOpponentAim(opponentAimPlayer, opponentAimAngle, opponentAimPower);
    }
    else if (mode == GameMode::Online && !isSpectating && !netMatch.IsMyTurn() && state == GameState::Aiming) {
        std::string waitMsg = std::string(netMatch.OpponentName()) + T(TK::OnlineWaitingSuffix, language);
        int ww = MeasureText(waitMsg.c_str(), 20);
        DrawText(waitMsg.c_str(), cfg::SCREEN_WIDTH / 2 - ww / 2, 90, 20, HudTextColor());
    }

    if (version == GameVersion::Plus) {
        powerups.DrawMessage();
    }

    EndMode2D();

    if (mode == GameMode::Online
#if CANNON_DUEL_DEBUG_MODE
        || mode == GameMode::PvAI || mode == GameMode::PvP
#endif
    ) {
        DrawCannonNameLabels();
    }

    DrawHUD();

    if (isSpectating && state != GameState::RoundOver) {
        DrawSpectatorBanner();
    }

    if (state == GameState::RoundOver) {
        DrawRectangle(0, 0, cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, Fade(BLACK, 0.55f));
        const char* msg = ResolveRoundMessage();
        int fs = 48;
        int tw = MeasureText(msg, fs);
        DrawText(msg, cfg::SCREEN_WIDTH / 2 - tw / 2, cfg::SCREEN_HEIGHT / 2 - 60, fs, WHITE);
        if (mode == GameMode::Online && !isSpectating) {
            DrawOnlineRoundOverOptions();
        } else {
            const char* hint = isSpectating
                ? T(TK::RoundOverSpectatorHint, language)
                : T(TK::RoundOverHint, language);
            int hw = MeasureText(hint, 20);
            DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, cfg::SCREEN_HEIGHT / 2 + 10, 20, LIGHTGRAY);
        }
    }

    DrawMenuButton();
#if CANNON_DUEL_DEBUG_MODE
    DrawDevPanelButton();
#endif

    if (IsLocalHumanTurn()) {
        DrawResetAngleButton();
    }

#if CANNON_DUEL_DEBUG_MODE
    if (devMode) {
        DrawDevPanel();
    }
#endif

    if (showMenuConfirm) {
        DrawMenuConfirmDialog();
    }

    PresentScreenWithDebug();
}
