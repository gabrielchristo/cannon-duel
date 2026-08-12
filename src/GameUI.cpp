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

namespace {

Rectangle MenuButtonRect() {
    return { cfg::SCREEN_WIDTH - 150.0f, 16.0f, 134.0f, 40.0f };
}

#if CANNON_DUEL_DEBUG_MODE
Rectangle DevPanelButtonRect() {
    Rectangle menu = MenuButtonRect();
    return { menu.x - menu.width - 10.0f, menu.y, menu.width, menu.height };
}
#endif

} // namespace

void Game::UpdateVersionSwitch(Vector2 mouse) {
    float cx = cfg::SCREEN_WIDTH / 2.0f;
    Rectangle full = { cx - 150, 175, 300, 50 };
    Rectangle leftHalf  = { full.x, full.y, full.width / 2, full.height };
    Rectangle rightHalf = { full.x + full.width / 2, full.y, full.width / 2, full.height };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, leftHalf)) version = GameVersion::Classic;
        else if (CheckCollisionPointRec(mouse, rightHalf)) version = GameVersion::Plus;
    }
}

void Game::DrawVersionSwitch(Vector2 mouse) {
    float cx = cfg::SCREEN_WIDTH / 2.0f;
    Rectangle full = { cx - 150, 175, 300, 50 };
    Rectangle leftHalf  = { full.x, full.y, full.width / 2, full.height };
    Rectangle rightHalf = { full.x + full.width / 2, full.y, full.width / 2, full.height };

    DrawRectangleRec(full, Color{60, 45, 30, 255});

    bool classicSel = (version == GameVersion::Classic);
    DrawRectangleRec(leftHalf, classicSel ? Color{230, 180, 90, 255} : Color{90, 75, 55, 255});
    DrawRectangleRec(rightHalf, !classicSel ? Color{230, 130, 60, 255} : Color{90, 75, 55, 255});
    DrawRectangleLinesEx(full, 2, Color{30, 22, 12, 255});
    DrawLineEx({cx, full.y}, {cx, full.y + full.height}, 2, Color{30, 22, 12, 255});

    int fs = 20;
    const char* lbl1 = T(TK::ClassicLabel, language);
    const char* lbl2 = T(TK::PlusLabel, language);
    int w1 = MeasureText(lbl1, fs);
    int w2 = MeasureText(lbl2, fs);
    DrawText(lbl1, static_cast<int>(leftHalf.x + leftHalf.width / 2 - w1 / 2),
             static_cast<int>(leftHalf.y + leftHalf.height / 2 - fs / 2), fs,
             classicSel ? Color{40, 25, 10, 255} : Color{230, 220, 210, 255});
    DrawText(lbl2, static_cast<int>(rightHalf.x + rightHalf.width / 2 - w2 / 2),
             static_cast<int>(rightHalf.y + rightHalf.height / 2 - fs / 2), fs,
             !classicSel ? Color{40, 15, 0, 255} : Color{230, 220, 210, 255});

    (void)mouse;
}

// ---------------------------------------------------------------------------
// Bandeiras de idioma (canto superior direito do menu principal)
// Desenhadas de forma procedural (retângulos/formas simples) — sem depender
// de nenhum arquivo de imagem, sempre nítidas em qualquer resolução.
// ---------------------------------------------------------------------------
namespace {
Rectangle BrazilFlagRect() { return { cfg::SCREEN_WIDTH - 180.0f, 20.0f, 64.0f, 44.0f }; }
Rectangle UsaFlagRect()    { return { cfg::SCREEN_WIDTH - 100.0f, 20.0f, 64.0f, 44.0f }; }

void DrawBrazilFlag(Rectangle r) {
    DrawRectangleRec(r, Color{0, 155, 58, 255}); // verde
    Vector2 c = { r.x + r.width / 2, r.y + r.height / 2 };
    Vector2 diamond[4] = {
        { c.x, r.y + 5 }, { r.x + r.width - 6, c.y }, { c.x, r.y + r.height - 5 }, { r.x + 6, c.y }
    };
    DrawTriangle(diamond[0], diamond[3], diamond[1], Color{255, 223, 0, 255});
    DrawTriangle(diamond[1], diamond[3], diamond[2], Color{255, 223, 0, 255});
    DrawCircleV(c, 9.0f, Color{0, 39, 118, 255}); // "globo" central simplificado
}

void DrawUsaFlag(Rectangle r) {
    DrawRectangleRec(r, Color{178, 34, 52, 255}); // fundo vermelho (listras ímpares)
    // Usa aritmética em ponto flutuante (não inteiro) para as listras,
    // garantindo que a soma exata das 7 faixas não ultrapasse a altura da
    // bandeira — arredondamento com "+1" antes fazia a última faixa branca
    // vazar para fora do retângulo/realce de seleção.
    float stripeH = r.height / 7.0f;
    for (int i = 0; i < 7; i += 2) {
        Rectangle stripe = { r.x, r.y + i * stripeH, r.width, stripeH };
        DrawRectangleRec(stripe, WHITE);
    }
    Rectangle canton = { r.x, r.y, r.width * 0.42f, r.height * 0.55f };
    DrawRectangleRec(canton, Color{60, 59, 110, 255});
    // pontinhos brancos representando as estrelas, simplificado
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float sx = canton.x + canton.width * (0.2f + col * 0.3f);
            float sy = canton.y + canton.height * (0.22f + row * 0.32f);
            DrawCircleV({sx, sy}, 1.6f, WHITE);
        }
    }
}
} // namespace

void Game::UpdateLanguageFlags(Vector2 mouse) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, BrazilFlagRect())) language = Lang::PT_BR;
        else if (CheckCollisionPointRec(mouse, UsaFlagRect())) language = Lang::EN;
    }
}

void Game::DrawLanguageFlags(Vector2 mouse) {
    Rectangle br = BrazilFlagRect();
    Rectangle us = UsaFlagRect();

    bool brSel = (language == Lang::PT_BR);
    bool usSel = (language == Lang::EN);

    DrawBrazilFlag(br);
    DrawRectangleLinesEx(br, brSel ? 3.0f : 1.5f, brSel ? Color{255, 210, 60, 255} : Color{30, 22, 12, 200});

    DrawUsaFlag(us);
    DrawRectangleLinesEx(us, usSel ? 3.0f : 1.5f, usSel ? Color{255, 210, 60, 255} : Color{30, 22, 12, 200});

    // leve destaque de hover
    if (CheckCollisionPointRec(mouse, br) && !brSel) DrawRectangleLinesEx(br, 2.0f, Color{255, 255, 255, 180});
    if (CheckCollisionPointRec(mouse, us) && !usSel) DrawRectangleLinesEx(us, 2.0f, Color{255, 255, 255, 180});
}

void Game::DrawMainMenu() {
    const char* title = T(TK::Title, language);
    int fs = 64;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 95, fs, Color{40, 30, 20, 255});

    Vector2 m = ::GetVirtualMouse();
    DrawVersionSwitch(m);
    DrawLanguageFlags(m);

    Rectangle btn1P = { cfg::SCREEN_WIDTH / 2.0f - 140, 265, 280, 56 };
    Rectangle btn2P = { cfg::SCREEN_WIDTH / 2.0f - 140, 341, 280, 56 };
    Rectangle btnOnline = { cfg::SCREEN_WIDTH / 2.0f - 140, 417, 280, 56 };
    Rectangle btnInstructions = { cfg::SCREEN_WIDTH / 2.0f - 140, 493, 280, 56 };
    Rectangle btnAbout = { cfg::SCREEN_WIDTH / 2.0f - 140, 569, 280, 56 };

    auto drawButton = [&](Rectangle r, const char* label) {
        bool hover = CheckCollisionPointRec(m, r);
        DrawRectangleRec(r, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
        DrawRectangleLinesEx(r, 2, Color{60, 40, 20, 255});
        int fs2 = 22;
        int tw2 = MeasureText(label, fs2);
        DrawText(label, static_cast<int>(r.x + r.width / 2 - tw2 / 2),
                 static_cast<int>(r.y + r.height / 2 - fs2 / 2), fs2, Color{40, 25, 10, 255});
    };

    drawButton(btn1P, T(TK::OnePlayer, language));
    drawButton(btn2P, T(TK::TwoPlayers, language));
    drawButton(btnOnline, T(TK::OnlineButton, language));
    drawButton(btnAbout, T(TK::AboutButton, language));
    drawButton(btnInstructions, T(TK::InstructionsButton, language));
}

void Game::DrawInstructions() {
    ClearBackground(Color{ 235, 214, 190, 255 });

    const char* title = T(TK::InstructionsTitle, language);
    int fs = 40;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 40, fs, Color{40, 30, 20, 255});

    int lineCount = 0;
    const char** lines = TextSplit(T(TK::InstructionsBody, language), '\n', &lineCount);

    int y = 108;
    int fs2 = 17;
    for (int i = 0; i < lineCount; ++i) {
        if (lines[i][0] != '\0') {
            int lw = MeasureText(lines[i], fs2);
            DrawText(lines[i], cfg::SCREEN_WIDTH / 2 - lw / 2, y, fs2, Color{60, 45, 30, 255});
        }
        y += 24;
    }

    Vector2 m = ::GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 60.0f, 200, 52 };
    bool hover = CheckCollisionPointRec(m, backBtn);
    DrawRectangleRec(backBtn, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
    DrawRectangleLinesEx(backBtn, 2, Color{60, 40, 20, 255});
    const char* backLabel = T(TK::InstructionsBack, language);
    int blw = MeasureText(backLabel, 22);
    DrawText(backLabel, static_cast<int>(backBtn.x + backBtn.width / 2 - blw / 2),
             static_cast<int>(backBtn.y + backBtn.height / 2 - 11), 22, Color{40, 25, 10, 255});
}

void Game::DrawAbout() {
    ClearBackground(Color{ 235, 214, 190, 255 });

    const char* title = T(TK::AboutTitle, language);
    int fs = 44;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 80, fs, Color{40, 30, 20, 255});

    int lineCount = 0;
    const char** lines = TextSplit(T(TK::AboutBody, language), '\n', &lineCount);

    int y = 160;
    int fs2 = 20;
    for (int i = 0; i < lineCount; ++i) {
        int lw = MeasureText(lines[i], fs2);
        DrawText(lines[i], cfg::SCREEN_WIDTH / 2 - lw / 2, y, fs2, Color{60, 45, 30, 255});
        y += 30;
    }

    const char* credit = T(TK::AboutCredit, language);
    int fsC = 26;
    int cw = MeasureText(credit, fsC);
    DrawText(credit, cfg::SCREEN_WIDTH / 2 - cw / 2, y + 20, fsC, Color{200, 120, 40, 255});

    Vector2 m = ::GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 100.0f, 200, 52 };
    bool hover = CheckCollisionPointRec(m, backBtn);
    DrawRectangleRec(backBtn, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
    DrawRectangleLinesEx(backBtn, 2, Color{60, 40, 20, 255});
    const char* backLabel = T(TK::AboutBack, language);
    int blw = MeasureText(backLabel, 22);
    DrawText(backLabel, static_cast<int>(backBtn.x + backBtn.width / 2 - blw / 2),
             static_cast<int>(backBtn.y + backBtn.height / 2 - 11), 22, Color{40, 25, 10, 255});
}

void Game::UpdateMenuConfirmDialog() {
    Vector2 m = ::GetVirtualMouse();
    float cx = cfg::SCREEN_WIDTH / 2.0f, cy = cfg::SCREEN_HEIGHT / 2.0f;
    Rectangle yesBtn = { cx - 130, cy + 20, 120, 48 };
    Rectangle noBtn  = { cx + 10,  cy + 20, 120, 48 };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, yesBtn)) {
            showMenuConfirm = false;
            projectile.Destroy();
            if (audioReady) StopMusicStream(musicTracks[currentMusicIndex]);
            if (mode == GameMode::Online) {
                if (netMatch.InMatch()) netMatch.AbandonMatch();
                else netMatch.LeaveMatch();
                onlineLobby.LeaveLobby();
            }
            state = GameState::MainMenu;
        } else if (CheckCollisionPointRec(m, noBtn)) {
            showMenuConfirm = false;
        }
    }
}

void Game::DrawMenuConfirmDialog() const {
    DrawRectangle(0, 0, cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT, Fade(BLACK, 0.6f));

    float cx = cfg::SCREEN_WIDTH / 2.0f, cy = cfg::SCREEN_HEIGHT / 2.0f;
    Rectangle panel = { cx - 200, cy - 90, 400, 200 };
    DrawRectangleRec(panel, Color{245, 240, 230, 255});
    DrawRectangleLinesEx(panel, 3, Color{30, 25, 20, 255});

    const char* msg = T(TK::ConfirmTitle, language);
    int fs = 24;
    int tw = MeasureText(msg, fs);
    DrawText(msg, static_cast<int>(cx - tw / 2), static_cast<int>(cy - 60), fs, Color{30, 25, 20, 255});

    const char* sub = T(TK::ConfirmSub, language);
    int fs2 = 16;
    int tw2 = MeasureText(sub, fs2);
    DrawText(sub, static_cast<int>(cx - tw2 / 2), static_cast<int>(cy - 28), fs2, Color{90, 80, 70, 255});

    Vector2 m = ::GetVirtualMouse();
    Rectangle yesBtn = { cx - 130, cy + 20, 120, 48 };
    Rectangle noBtn  = { cx + 10,  cy + 20, 120, 48 };

    bool hoverYes = CheckCollisionPointRec(m, yesBtn);
    bool hoverNo  = CheckCollisionPointRec(m, noBtn);

    DrawRectangleRec(yesBtn, hoverYes ? Color{220, 90, 80, 255} : Color{200, 70, 60, 255});
    DrawRectangleLinesEx(yesBtn, 2, Color{30, 25, 20, 255});
    const char* yesLabel = T(TK::ConfirmYes, language);
    int ytw = MeasureText(yesLabel, 18);
    DrawText(yesLabel, static_cast<int>(yesBtn.x + yesBtn.width / 2 - ytw / 2),
             static_cast<int>(yesBtn.y + 15), 18, WHITE);

    DrawRectangleRec(noBtn, hoverNo ? Color{140, 190, 130, 255} : Color{120, 170, 110, 255});
    DrawRectangleLinesEx(noBtn, 2, Color{30, 25, 20, 255});
    const char* noLabel = T(TK::ConfirmNo, language);
    int ntw = MeasureText(noLabel, 18);
    DrawText(noLabel, static_cast<int>(noBtn.x + noBtn.width / 2 - ntw / 2),
             static_cast<int>(noBtn.y + 15), 18, WHITE);
}

void Game::DrawMenuButton() const {
    Rectangle r = MenuButtonRect();
    Vector2 m = ::GetVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hover ? Color{235, 235, 235, 235} : Color{20, 20, 20, 170});
    DrawRectangleLinesEx(r, 2, hover ? Color{20, 20, 20, 255} : Color{235, 235, 235, 200});

    const char* label = T(TK::MenuButton, language);
    int fs = 20;
    int tw = MeasureText(label, fs);
    Color textColor = hover ? Color{20, 20, 20, 255} : Color{240, 240, 240, 255};
    DrawText(label, static_cast<int>(r.x + r.width / 2 - tw / 2),
             static_cast<int>(r.y + r.height / 2 - fs / 2), fs, textColor);
}

bool Game::HandleMenuButtonClick() {
    Rectangle r = MenuButtonRect();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(::GetVirtualMouse(), r)) {
        return true;
    }
    return false;
}

#if CANNON_DUEL_DEBUG_MODE
void Game::DrawDevPanelButton() const {
    Rectangle r = DevPanelButtonRect();
    Vector2 m = ::GetVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);
    bool active = devMode;

    Color bg = active ? Color{255, 210, 60, 255}
             : hover ? Color{235, 235, 235, 235}
             : Color{20, 20, 20, 170};
    Color border = active ? Color{120, 90, 20, 255}
                 : hover ? Color{20, 20, 20, 255}
                 : Color{255, 210, 60, 200};
    Color textColor = active ? Color{20, 20, 20, 255}
                      : hover ? Color{20, 20, 20, 255}
                      : Color{255, 210, 60, 255};

    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, border);

    const char* label = "DEV";
    int fs = 20;
    int tw = MeasureText(label, fs);
    DrawText(label, static_cast<int>(r.x + r.width / 2 - tw / 2),
             static_cast<int>(r.y + r.height / 2 - fs / 2), fs, textColor);
}

bool Game::HandleDevPanelButtonClick() {
    Rectangle r = DevPanelButtonRect();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(::GetVirtualMouse(), r)) {
        devMode = !devMode;
        if (!devMode) {
            devPanelScrollDragging = false;
        }
        return true;
    }
    return false;
}
#endif

void Game::DrawResetAngleButton() const {
    // Posicionado logo abaixo do botão de menu — botão in-game, disponível
    // tanto no PC (via mouse) quanto no Android (via toque), equivalente à
    // tecla B do atalho de teclado exclusivo de PC.
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 66.0f, 134.0f, 40.0f };
    Vector2 m = ::GetVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hover ? Color{235, 235, 235, 235} : Color{20, 20, 20, 170});
    DrawRectangleLinesEx(r, 2, hover ? Color{20, 20, 20, 255} : Color{235, 235, 235, 200});

    const char* label = T(TK::ResetAngleButton, language);
    int fs = 14;
    int tw = MeasureText(label, fs);
    Color textColor = hover ? Color{20, 20, 20, 255} : Color{240, 240, 240, 255};
    DrawText(label, static_cast<int>(r.x + r.width / 2 - tw / 2),
             static_cast<int>(r.y + r.height / 2 - fs / 2), fs, textColor);
}

bool Game::HandleResetAngleButtonClick() {
    Rectangle r = { cfg::SCREEN_WIDTH - 150.0f, 66.0f, 134.0f, 40.0f };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(::GetVirtualMouse(), r)) {
        return true;
    }
    return false;
}

void Game::DrawWindIndicator() const {
    int cx = cfg::SCREEN_WIDTH / 2;
    int cy = 40;
    DrawText(T(TK::WindLabel, language), cx - 30, cy - 22, 16, HudTextColor());

    float ratio = windForce / cfg::WIND_MAX_ACCEL; // -1..1
    int arrowLen = static_cast<int>(std::fabs(ratio) * 60.0f) + 10;
    Color c = (ratio >= 0) ? Color{200, 60, 60, 255} : Color{60, 120, 200, 255};

    if (ratio >= 0) {
        DrawTriangle({(float)(cx + arrowLen), (float)cy}, {(float)cx, (float)(cy - 8)}, {(float)cx, (float)(cy + 8)}, c);
        DrawLineEx({(float)cx, (float)cy}, {(float)(cx + arrowLen), (float)cy}, 4, c);
    } else {
        DrawTriangle({(float)(cx - arrowLen), (float)cy}, {(float)cx, (float)(cy + 8)}, {(float)cx, (float)(cy - 8)}, c);
        DrawLineEx({(float)cx, (float)cy}, {(float)(cx - arrowLen), (float)cy}, 4, c);
    }
}

void Game::DrawHUD() {
    DrawWindIndicator();

    const char* turnLabel = (mode == GameMode::PvAI && currentPlayer == 2)
        ? T(TK::TurnAI, language)
        : (currentPlayer == 1 ? T(TK::TurnPlayer1, language) : T(TK::TurnPlayer2, language));
    DrawText(turnLabel, 20, 20, 22, HudTextColor());

    Cannon& active = (currentPlayer == 1) ? player1 : player2;
    const char* angleForceFmt = (language == Lang::PT_BR) ? "Angulo: %.0f  Forca: %.0f%%" : "Angle: %.0f  Power: %.0f%%";
    std::string info = TextFormat(angleForceFmt, active.angleDeg, active.power01 * 100.0f);
    DrawText(info.c_str(), 20, 48, 18, HudTextColorDim());
}

void Game::DrawOnlineCannonLabels() const {
    auto drawLabel = [&](const Cannon& cannon, const std::string& name) {
        if (name.empty()) return;
        const int fs = 12;
        int tw = MeasureText(name.c_str(), fs);
        int tx = static_cast<int>(cannon.x - tw / 2);
        int ty = static_cast<int>(cannon.groundY - cfg::CANNON_BODY_RADIUS_PX - 50);
        DrawRectangle(tx - 4, ty - 2, tw + 8, fs + 4, Fade(BLACK, 0.45f));
        DrawText(name.c_str(), tx, ty, fs, WHITE);
    };

    drawLabel(player1, onlineP1Name);
    drawLabel(player2, onlineP2Name);
}
