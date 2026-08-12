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

#if CANNON_DUEL_DEBUG_MODE
void Game::DrawDebugLogOverlay() const {
    const int fs = 12;
    const int lineH = 15;
    const int panelW = 480;
    const int visibleLines = 16;
    const int headerH = 42;
    const Rectangle reopenBtn = { 8, 8, 70, 24 };

    if (!debugLogVisible) {
        bool hover = CheckCollisionPointRec(GetMousePosition(), reopenBtn);
        DrawRectangleRec(reopenBtn, Fade(hover ? YELLOW : Color{40, 40, 40, 255}, 0.85f));
        DrawRectangleLinesEx(reopenBtn, 2, YELLOW);
        DrawText("LOG", static_cast<int>(reopenBtn.x + 16), static_cast<int>(reopenBtn.y + 5), fs + 2,
                 hover ? BLACK : YELLOW);
        return;
    }

    int totalLines = static_cast<int>(DebugLog::Lines().size());
    int shown = std::min(std::max(totalLines, 1), visibleLines);
    int panelH = headerH + lineH * shown;
    Rectangle panel = { 8, 8, static_cast<float>(panelW), static_cast<float>(panelH) };
    Rectangle closeBtn = { panel.x + panel.width - 44, panel.y + 2, 40, 36 };

    DrawRectangleRec(panel, Fade(BLACK, 0.82f));
    DrawRectangleLinesEx(panel, 2, Fade(YELLOW, 0.9f));
    DrawText("LOG (overlay — sem logcat/terminal)", static_cast<int>(panel.x + 6), static_cast<int>(panel.y + 4), fs, YELLOW);
    DrawText("arraste p/ rolar", static_cast<int>(panel.x + 6), static_cast<int>(panel.y + 4 + fs + 2), fs - 2, Fade(YELLOW, 0.75f));

    bool hoverClose = CheckCollisionPointRec(GetMousePosition(), closeBtn);
    DrawRectangleRec(closeBtn, hoverClose ? RED : Color{80, 20, 20, 255});
    DrawRectangleLinesEx(closeBtn, 2, WHITE);
    int xFs = 22;
    int xw = MeasureText("X", xFs);
    DrawText("X", static_cast<int>(closeBtn.x + closeBtn.width / 2 - xw / 2),
             static_cast<int>(closeBtn.y + closeBtn.height / 2 - xFs / 2), xFs, WHITE);

    if (totalLines == 0) {
        DrawText("(aguardando logs...)", static_cast<int>(panel.x + 6), panel.y + headerH,
                 fs, Fade(WHITE, 0.6f));
        return;
    }

    int y = static_cast<int>(panel.y) + headerH;
    int start = std::clamp(debugLogScrollIndex, 0, std::max(0, totalLines - visibleLines));
    for (int i = start; i < std::min(totalLines, start + visibleLines); ++i) {
        DrawText(DebugLog::Lines()[i].c_str(), static_cast<int>(panel.x + 6), y, fs, WHITE);
        y += lineH;
    }

    // indicador simples de posição do scroll (se tiver mais linhas que cabem)
    if (totalLines > visibleLines) {
        int maxScroll = totalLines - visibleLines;
        float ratio = maxScroll > 0 ? static_cast<float>(start) / maxScroll : 0.0f;
        float barH = panel.height - headerH;
        float barY = panel.y + headerH + ratio * (barH - 20.0f);
        DrawRectangle(static_cast<int>(panel.x + panel.width - 4), static_cast<int>(barY), 3, 20, YELLOW);
    }
}

bool Game::UpdateDebugLogOverlay() {
    Vector2 m = GetMousePosition(); // espaço de tela REAL — este overlay não usa a resolução virtual

    const int lineH = 15;
    const int panelW = 480;
    const int visibleLines = 16;
    const int headerH = 42;
    const Rectangle reopenBtn = { 8, 8, 70, 24 };

    if (!debugLogVisible) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, reopenBtn)) {
            debugLogVisible = true;
            return true;
        }
        return false;
    }

    int totalLines = static_cast<int>(DebugLog::Lines().size());
    int shown = std::min(totalLines, visibleLines);
    int panelH = headerH + lineH * shown;
    Rectangle panel = { 8, 8, static_cast<float>(panelW), static_cast<float>(panelH) };
    Rectangle closeBtn = { panel.x + panel.width - 44, panel.y + 2, 40, 36 };
    int maxScroll = std::max(0, totalLines - visibleLines);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, closeBtn)) {
        debugLogVisible = false;
        debugLogDragging = false;
        return true;
    }

    bool overPanel = CheckCollisionPointRec(m, panel);

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && overPanel) {
        debugLogFollowTail = false;
        debugLogScrollIndex = std::clamp(debugLogScrollIndex - static_cast<int>(wheel * 2), 0, maxScroll);
    }

    // arrastar funciona tanto com mouse (desktop) quanto toque (Android — o
    // raylib mapeia toque de tela pra "mouse" automaticamente).
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && overPanel) {
        debugLogDragging = true;
        debugLogDragStartY = m.y;
        debugLogDragStartScroll = debugLogScrollIndex;
    }
    if (debugLogDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float deltaY = m.y - debugLogDragStartY;
        int deltaLines = static_cast<int>(deltaY / lineH);
        if (deltaLines != 0) debugLogFollowTail = false;
        debugLogScrollIndex = std::clamp(debugLogDragStartScroll - deltaLines, 0, maxScroll);
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        debugLogDragging = false;
    }

    // continua acompanhando as linhas mais novas até o usuário rolar manualmente
    if (debugLogFollowTail) {
        debugLogScrollIndex = maxScroll;
    } else {
        debugLogScrollIndex = std::clamp(debugLogScrollIndex, 0, maxScroll);
    }

    // consome o clique se ele começou (ou o arrasto continua) dentro do painel
    return overPanel || debugLogDragging;
}
#endif // CANNON_DUEL_DEBUG_MODE

#if CANNON_DUEL_DEBUG_MODE && !CANNON_DUEL_ANDROID_BUILD
void Game::DevForceSpawnPowerup() {
    if (version != GameVersion::Plus) version = GameVersion::Plus;
    powerups.ForceSpawnReady();
    powerups.MaybeSpawnRandom();
}

void Game::DevGrantPowerupToPlayer1(PowerupType type) {
    if (version != GameVersion::Plus) version = GameVersion::Plus;
    powerups.ApplyEffect(player1, type, language);
}

bool Game::UpdateDevPanel() {
    Vector2 m = ::GetVirtualMouse();
    Rectangle panel = { 16, 90, 240, 505 };
    if (!CheckCollisionPointRec(m, panel)) return false;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return true;

    bool inMatch = (state == GameState::Aiming || state == GameState::ProjectileFlying ||
                    state == GameState::TurnTransition || state == GameState::RoundOver);

    float y = panel.y + 34;
    float bw = panel.width - 20, bh = 28, gap = 5;
    auto hit = [&](float rowY) {
        Rectangle r = { panel.x + 10, rowY, bw, bh };
        return CheckCollisionPointRec(m, r);
    };

    if (hit(y) && inMatch) { player1.health = cfg::CANNON_MAX_HEALTH; player2.health = cfg::CANNON_MAX_HEALTH; return true; }
    y += bh + gap;
    if (hit(y) && inMatch) { DevForceSpawnPowerup(); return true; }
    y += bh + gap;
    if (hit(y) && inMatch) { windForce = 0.0f; return true; }
    y += bh + gap;
    if (hit(y) && inMatch && state == GameState::Aiming) { EndTurn(); return true; }
    y += bh + gap;
    if (hit(y)) { version = (version == GameVersion::Classic) ? GameVersion::Plus : GameVersion::Classic; return true; }
    y += bh + gap;
    if (hit(y)) { nightMode = !nightMode; return true; }
    y += bh + gap;
    if (hit(y) && state == GameState::MainMenu) { StartMatch(GameMode::PvAI); return true; }
    y += bh + gap;

    y += 22;
    PowerupType types[] = { PowerupType::DoubleDamage, PowerupType::TrajectoryPreview,
                             PowerupType::Guided, PowerupType::Heal, PowerupType::Shield };
    for (PowerupType t : types) {
        if (hit(y) && inMatch) { DevGrantPowerupToPlayer1(t); return true; }
        y += bh + gap;
    }

    return true;
}

void Game::DrawDevPanel() const {
    Rectangle panel = { 16, 90, 240, 505 };
    DrawRectangleRec(panel, Fade(Color{15, 15, 20, 255}, 0.9f));
    DrawRectangleLinesEx(panel, 2, Color{255, 210, 60, 255});

    const char* title = "DEV PANEL (F9)";
    DrawText(title, static_cast<int>(panel.x + 10), static_cast<int>(panel.y + 6), 16, Color{255, 210, 60, 255});

    bool inMatch = (state == GameState::Aiming || state == GameState::ProjectileFlying ||
                    state == GameState::TurnTransition || state == GameState::RoundOver);

    Vector2 m = ::GetVirtualMouse();
    float y = panel.y + 34;
    float bw = panel.width - 20, bh = 28, gap = 5;

    auto drawRow = [&](const char* label, bool enabled, bool highlight = false) {
        Rectangle r = { panel.x + 10, y, bw, bh };
        bool hover = enabled && CheckCollisionPointRec(m, r);
        Color bg = !enabled ? Color{40, 40, 45, 255}
                 : highlight ? Color{80, 160, 90, 255}
                 : hover ? Color{90, 90, 100, 255} : Color{55, 55, 65, 255};
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, Color{200, 200, 210, 150});
        Color txt = enabled ? WHITE : Color{130, 130, 135, 255};
        DrawText(label, static_cast<int>(r.x + 8), static_cast<int>(r.y + 6), 13, txt);
        y += bh + gap;
    };

    drawRow("Curar os dois canhoes", inMatch);
    drawRow("Forcar spawn de power-up", inMatch);
    drawRow("Zerar vento", inMatch);
    drawRow("Pular turno", inMatch && state == GameState::Aiming);
    drawRow(version == GameVersion::Plus ? "Versao: PLUS" : "Versao: CLASSIC", true);
    drawRow(nightMode ? "Cenario: NOITE" : "Cenario: DIA", true);
    drawRow("Iniciar partida rapida (1J)", state == GameState::MainMenu);

    y += 22;
    DrawText("Dar power-up ao Jogador 1:", static_cast<int>(panel.x + 10), static_cast<int>(y - 18), 13,
              Color{200, 200, 210, 255});

    const char* labels[] = { "2x Dano em dobro", "Trajetoria prevista",
                              "Teleguiado", "Cura", "Escudo" };
    for (const char* label : labels) {
        drawRow(label, inMatch);
    }
}
#endif // CANNON_DUEL_DEBUG_MODE && !CANNON_DUEL_ANDROID_BUILD
