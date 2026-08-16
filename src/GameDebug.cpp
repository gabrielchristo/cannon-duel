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

#if CANNON_DUEL_DEBUG_MODE
namespace {

constexpr float kDevRowH = 28.0f;
constexpr float kDevRowGap = 5.0f;
constexpr float kDevHeaderH = 44.0f;
constexpr float kDevSectionGap = 22.0f;
constexpr float kDevSubSectionGap = 14.0f;

Rectangle DevPanelRect() {
    float maxH = cfg::SCREEN_HEIGHT - 80.0f;
    float h = std::min(520.0f, maxH);
    return { 16.0f, 90.0f, 260.0f, h };
}

float DevPanelContentHeight() {
    const int mainRows = 7;
    const int powerRows = 5;
    return mainRows * (kDevRowH + kDevRowGap)
         + kDevSectionGap + powerRows * (kDevRowH + kDevRowGap)
         + kDevSubSectionGap + kDevSectionGap + powerRows * (kDevRowH + kDevRowGap);
}

enum class DevAction {
    None,
    HealAll,
    SpawnPowerup,
    WindZero,
    SkipTurn,
    ToggleVersion,
    ToggleNight,
    QuickMatch,
    GrantP1,
    GrantP2,
};

struct DevPanelItem {
    DevAction action = DevAction::None;
    const char* label = "";
    bool enabled = false;
    bool highlight = false;
    PowerupType powerType = PowerupType::Heal;
    bool isSection = false;
};

const char* ScenarioDevLabel(Scenario scenario) {
    switch (scenario) {
        case Scenario::Night: return "Cenario: NOITE";
        case Scenario::ValleyOfTheEnd: return "Cenario: VALE DO FIM";
        default: return "Cenario: DIA";
    }
}

void CollectDevPanelItems(bool inMatch, bool isMainMenu, bool isAiming,
                          GameVersion version, Scenario scenario,
                          std::vector<DevPanelItem>& out) {
    out.clear();
    auto button = [&](DevAction act, const char* label, bool enabled, bool highlight = false,
                      PowerupType type = PowerupType::Heal) {
        out.push_back({ act, label, enabled, highlight, type, false });
    };
    auto section = [&](const char* label) {
        out.push_back({ DevAction::None, label, false, false, PowerupType::Heal, true });
    };

    button(DevAction::HealAll, "Curar os dois canhoes", inMatch);
    button(DevAction::SpawnPowerup, "Forcar spawn de power-up", inMatch);
    button(DevAction::WindZero, "Zerar vento", inMatch);
    button(DevAction::SkipTurn, "Pular turno", inMatch && isAiming);
    button(DevAction::ToggleVersion,
           version == GameVersion::Plus ? "Versao: PLUS" : "Versao: CLASSIC", true);
    button(DevAction::ToggleNight, ScenarioDevLabel(scenario), true);
    button(DevAction::QuickMatch, "Iniciar partida rapida (1J)", isMainMenu);

    section("Power-up -> Jogador 1:");
    const char* labels[] = { "2x Dano", "Trajetoria", "Teleguiado", "Cura", "Escudo" };
    PowerupType types[] = { PowerupType::DoubleDamage, PowerupType::TrajectoryPreview,
                            PowerupType::Guided, PowerupType::Heal, PowerupType::Shield };
    for (int i = 0; i < 5; ++i) {
        button(DevAction::GrantP1, labels[i], inMatch, false, types[i]);
    }

    section("Power-up -> Jogador 2:");
    for (int i = 0; i < 5; ++i) {
        button(DevAction::GrantP2, labels[i], inMatch, false, types[i]);
    }
}

} // namespace
void Game::DevBroadcastCommand(const std::string& action, int player, int type, float x, float value) {
    if (mode == GameMode::Online && netMatch.InMatch()) {
        netMatch.PublishDevCommand(action, player, type, x, value);
    }
}

void Game::ApplyDevCommand(const DevCommand& cmd) {
    if (!cmd.valid || cmd.action.empty()) return;

    if (cmd.action == "heal_all") {
        for (int i = 0; i < roster.CannonCount(); ++i) {
            roster.At(i).health = cfg::CANNON_MAX_HEALTH;
        }
    } else if (cmd.action == "wind_zero") {
        windForce = 0.0f;
    } else if (cmd.action == "spawn_powerup") {
        if (version != GameVersion::Plus) version = GameVersion::Plus;
        if (cmd.type >= 0 && cmd.type < static_cast<int>(PowerupType::COUNT)) {
            powerups.SpawnAt(cmd.x, static_cast<PowerupType>(cmd.type), roster);
        }
    } else if (cmd.action == "grant_powerup") {
        if (version != GameVersion::Plus) version = GameVersion::Plus;
        if (cmd.type < 0 || cmd.type >= static_cast<int>(PowerupType::COUNT)) return;
        Cannon& target = GetCannon(cmd.player > 0 ? cmd.player : 1);
        powerups.ApplyEffect(target, static_cast<PowerupType>(cmd.type), language);
    } else if (cmd.action == "skip_turn") {
        if (cmd.player == 1 || cmd.player == 2) {
            currentPlayer = cmd.player;
            netMatch.DevSyncTurnTo(cmd.player);
            aimPhase = AimPhase::Angle;
            aimOscTimer = 0.0f;
            stateTimer = 0.2f;
            state = GameState::TurnTransition;
        }
    } else if (cmd.action == "night_mode") {
        ApplyScenario(cmd.value >= 0.5f ? Scenario::Night : Scenario::Day, true);
    } else if (cmd.action == "scenario") {
        const int idx = std::clamp(static_cast<int>(cmd.value), 0, kScenarioCount - 1);
        ApplyScenario(static_cast<Scenario>(idx), true);
    }
}

void Game::DevHealAll() {
    for (int i = 0; i < roster.CannonCount(); ++i) {
        roster.At(i).health = cfg::CANNON_MAX_HEALTH;
    }
    DevBroadcastCommand("heal_all");
}

void Game::DevSetWindZero() {
    windForce = 0.0f;
    DevBroadcastCommand("wind_zero");
}

void Game::DevForceSpawnPowerup() {
    if (version != GameVersion::Plus) version = GameVersion::Plus;
    float margin = 160.0f;
    float x = RandF(margin, cfg::SCREEN_WIDTH - margin);
    PowerupType type = static_cast<PowerupType>(static_cast<int>(PowerupType::Heal));
    // sorteio local igual ao spawn normal
    struct Entry { PowerupType t; float w; };
    const Entry entries[] = {
        { PowerupType::DoubleDamage, cfg::POWERUP_WEIGHT_DOUBLE_DMG },
        { PowerupType::TrajectoryPreview, cfg::POWERUP_WEIGHT_TRAJECTORY },
        { PowerupType::Guided, cfg::POWERUP_WEIGHT_GUIDED },
        { PowerupType::Heal, cfg::POWERUP_WEIGHT_HEAL },
        { PowerupType::Shield, cfg::POWERUP_WEIGHT_SHIELD },
    };
    float total = 0.0f;
    for (const auto& e : entries) total += e.w;
    float roll = RandF(0.0f, total);
    type = entries[0].t;
    for (const auto& e : entries) {
        if (roll < e.w) { type = e.t; break; }
        roll -= e.w;
    }
    powerups.SpawnAt(x, type, roster);
    DevBroadcastCommand("spawn_powerup", 0, static_cast<int>(type), x);
}

void Game::DevGrantPowerup(int playerNumber, PowerupType type) {
    if (version != GameVersion::Plus) version = GameVersion::Plus;
    Cannon& target = GetCannon(playerNumber > 0 ? playerNumber : 1);
    powerups.ApplyEffect(target, type, language);
    DevBroadcastCommand("grant_powerup", playerNumber, static_cast<int>(type));
}

void Game::DevSkipTurn() {
    if (mode == GameMode::Online && netMatch.InMatch() && state == GameState::Aiming) {
        const int next = (netMatch.MyPlayerNumber() == 1) ? 2 : 1;
        netMatch.DevSyncTurnTo(next);
        currentPlayer = next;
        aimPhase = AimPhase::Angle;
        aimOscTimer = 0.0f;
        stateTimer = 0.2f;
        state = GameState::TurnTransition;
        DevBroadcastCommand("skip_turn", next);
    } else if (state == GameState::Aiming) {
        EndTurn();
    }
}

bool Game::UpdateDevPanel() {
    Vector2 m = ::GetVirtualMouse();
    Rectangle panel = DevPanelRect();
    const float contentTop = panel.y + kDevHeaderH;
    const float contentH = panel.height - kDevHeaderH - 4.0f;
    const float contentHeight = DevPanelContentHeight();
    const float maxScroll = std::max(0.0f, contentHeight - contentH);

    devPanelScrollY = std::clamp(devPanelScrollY, 0.0f, maxScroll);

    const float scrollBarW = 14.0f;
    Rectangle scrollTrack = {
        panel.x + panel.width - scrollBarW - 4.0f, contentTop, scrollBarW, contentH
    };
    const bool overPanel = CheckCollisionPointRec(m, panel);
    const bool overScroll = CheckCollisionPointRec(m, scrollTrack);

    if (overPanel) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            devPanelScrollY = std::clamp(devPanelScrollY - wheel * 36.0f, 0.0f, maxScroll);
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && overScroll && maxScroll > 0.0f) {
        devPanelScrollDragging = true;
        devPanelScrollDragStartY = m.y;
        devPanelScrollDragStart = devPanelScrollY;
    }
    if (devPanelScrollDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float thumbTravel = std::max(1.0f, contentH - 24.0f);
        float deltaY = m.y - devPanelScrollDragStartY;
        devPanelScrollY = std::clamp(
            devPanelScrollDragStart + (deltaY / thumbTravel) * maxScroll, 0.0f, maxScroll);
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        devPanelScrollDragging = false;
    }

    bool inMatch = (state == GameState::Aiming || state == GameState::ProjectileFlying ||
                    state == GameState::TurnTransition || state == GameState::RoundOver);

    std::vector<DevPanelItem> items;
    CollectDevPanelItems(inMatch, state == GameState::MainMenu, state == GameState::Aiming,
                         version, scenario, items);

    const float bw = panel.width - 20.0f - scrollBarW;
    float y = contentTop - devPanelScrollY;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && overPanel && !overScroll) {
        for (const DevPanelItem& item : items) {
            if (item.isSection) {
                y += kDevSectionGap;
                continue;
            }
            Rectangle r = { panel.x + 10.0f, y, bw, kDevRowH };
            if (item.enabled && CheckCollisionPointRec(m, r)) {
                switch (item.action) {
                    case DevAction::HealAll: DevHealAll(); break;
                    case DevAction::SpawnPowerup: DevForceSpawnPowerup(); break;
                    case DevAction::WindZero: DevSetWindZero(); break;
                    case DevAction::SkipTurn: DevSkipTurn(); break;
                    case DevAction::ToggleVersion:
                        version = (version == GameVersion::Classic) ? GameVersion::Plus : GameVersion::Classic;
                        break;
                    case DevAction::ToggleNight: {
                        const int next = (static_cast<int>(scenario) + 1) % kScenarioCount;
                        ApplyScenario(static_cast<Scenario>(next), true);
                        DevBroadcastCommand("scenario", 0, -1, 0.0f, static_cast<float>(next));
                        break;
                    }
                    case DevAction::QuickMatch: StartMatch(GameMode::PvAI); break;
                    case DevAction::GrantP1: DevGrantPowerup(1, item.powerType); break;
                    case DevAction::GrantP2: DevGrantPowerup(2, item.powerType); break;
                    default: break;
                }
                return true;
            }
            y += kDevRowH + kDevRowGap;
        }
    }

    return overPanel || devPanelScrollDragging;
}

void Game::DrawDevPanel() const {
    Rectangle panel = DevPanelRect();
    DrawRectangleRec(panel, Fade(Color{15, 15, 20, 255}, 0.9f));
    DrawRectangleLinesEx(panel, 2, Color{255, 210, 60, 255});

#if CANNON_DUEL_ANDROID_BUILD
    const char* title = "DEV PANEL";
#else
    const char* title = devMode ? "DEV PANEL (F9)" : "DEV PANEL";
#endif
    DrawText(title, static_cast<int>(panel.x + 10), static_cast<int>(panel.y + 6), 16,
             Color{255, 210, 60, 255});
    if (mode == GameMode::Online && netMatch.InMatch()) {
        DrawText("online: sync via Realtime", static_cast<int>(panel.x + 10),
                 static_cast<int>(panel.y + 24), 11, Color{140, 200, 255, 255});
    }

    const float contentTop = panel.y + kDevHeaderH;
    const float contentH = panel.height - kDevHeaderH - 4.0f;
    const float contentHeight = DevPanelContentHeight();
    const float maxScroll = std::max(0.0f, contentHeight - contentH);
    const float scrollBarW = 14.0f;

    bool inMatch = (state == GameState::Aiming || state == GameState::ProjectileFlying ||
                    state == GameState::TurnTransition || state == GameState::RoundOver);

    std::vector<DevPanelItem> items;
    CollectDevPanelItems(inMatch, state == GameState::MainMenu, state == GameState::Aiming,
                         version, scenario, items);

    Vector2 m = ::GetVirtualMouse();
    const float bw = panel.width - 20.0f - scrollBarW;

    BeginScissorMode(static_cast<int>(panel.x + 2), static_cast<int>(contentTop),
                     static_cast<int>(panel.width - 4), static_cast<int>(contentH));

    float y = contentTop - devPanelScrollY;
    for (const DevPanelItem& item : items) {
        if (item.isSection) {
            if (y + 4 >= contentTop && y - 18 <= contentTop + contentH) {
                DrawText(item.label, static_cast<int>(panel.x + 10), static_cast<int>(y - 18), 13,
                         Color{200, 200, 210, 255});
            }
            y += kDevSectionGap;
            continue;
        }

        Rectangle r = { panel.x + 10.0f, y, bw, kDevRowH };
        if (y + kDevRowH >= contentTop && y <= contentTop + contentH) {
            bool hover = item.enabled && CheckCollisionPointRec(m, r);
            Color bg = !item.enabled ? Color{40, 40, 45, 255}
                     : item.highlight ? Color{80, 160, 90, 255}
                     : hover ? Color{90, 90, 100, 255} : Color{55, 55, 65, 255};
            DrawRectangleRec(r, bg);
            DrawRectangleLinesEx(r, 1, Color{200, 200, 210, 150});
            Color txt = item.enabled ? WHITE : Color{130, 130, 135, 255};
            DrawText(item.label, static_cast<int>(r.x + 8), static_cast<int>(r.y + 6), 13, txt);
        }
        y += kDevRowH + kDevRowGap;
    }

    EndScissorMode();

    if (maxScroll > 0.0f) {
        Rectangle scrollTrack = {
            panel.x + panel.width - scrollBarW - 4.0f, contentTop, scrollBarW, contentH
        };
        DrawRectangleRec(scrollTrack, Color{35, 35, 42, 220});
        DrawRectangleLinesEx(scrollTrack, 1, Color{120, 120, 130, 180});

        float thumbH = std::max(24.0f, contentH * (contentH / contentHeight));
        float thumbTravel = std::max(1.0f, contentH - thumbH);
        float thumbY = contentTop + (devPanelScrollY / maxScroll) * thumbTravel;
        Rectangle thumb = { scrollTrack.x + 2.0f, thumbY, scrollTrack.width - 4.0f, thumbH };
        bool hoverThumb = CheckCollisionPointRec(m, thumb) || devPanelScrollDragging;
        DrawRectangleRec(thumb, hoverThumb ? Color{255, 210, 60, 255} : Color{180, 180, 190, 255});
    }
}
#endif // CANNON_DUEL_DEBUG_MODE
