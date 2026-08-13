#include "Game.h"
#include "AssetPath.h"
#include "Config.h"
#include "DebugLog.h"
#include "GameRand.h"
#include "ScrollList.h"
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

void Game::UpdateMainMenu() {
    Vector2 m = ::GetVirtualMouse();
    UpdateVersionSwitch(m);
    UpdateLanguageFlags(m);

    Rectangle btn1P = { cfg::SCREEN_WIDTH / 2.0f - 140, 265, 280, 56 };
    Rectangle btn2P = { cfg::SCREEN_WIDTH / 2.0f - 140, 341, 280, 56 };
    Rectangle btnOnline = { cfg::SCREEN_WIDTH / 2.0f - 140, 417, 280, 56 };
    Rectangle btnInstructions = { cfg::SCREEN_WIDTH / 2.0f - 140, 493, 280, 56 };
    Rectangle btnAbout = { cfg::SCREEN_WIDTH / 2.0f - 140, 569, 280, 56 };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, btn1P)) {
            pendingMatchMode = GameMode::PvAI;
            state = GameState::FormatSelect;
        } else if (CheckCollisionPointRec(m, btn2P)) {
            pendingMatchMode = GameMode::PvP;
            state = GameState::FormatSelect;
        }
        else if (CheckCollisionPointRec(m, btnOnline)) {
            onlineLobbyScroll_ = {};
            onlineLobby.EnterLobby();
            state = GameState::OnlineLobby;
        }
        else if (CheckCollisionPointRec(m, btnAbout)) state = GameState::About;
        else if (CheckCollisionPointRec(m, btnInstructions)) state = GameState::Instructions;
    }
}

void Game::UpdateAbout() {
    Vector2 m = ::GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 100.0f, 200, 52 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, backBtn)) {
        state = GameState::MainMenu;
    }
}

void Game::UpdateInstructions() {
    Vector2 m = ::GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 60.0f, 200, 52 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, backBtn)) {
        state = GameState::MainMenu;
    }
}

namespace {

Rectangle OnlineEditNameBtnRect() {
    return { 20.0f, 72.0f, 120.0f, 28.0f };
}

constexpr float kLobbyRowH = 74.0f;
constexpr float kLobbyListBottomY = cfg::SCREEN_HEIGHT - 68.0f;

ScrollListLayout OnlineLobbyListLayout(float listStartY, int playerCount) {
    ScrollListLayout layout;
    layout.viewport = {
        cfg::SCREEN_WIDTH / 2.0f - 350.0f,
        listStartY,
        714.0f,
        std::max(0.0f, kLobbyListBottomY - listStartY)
    };
    layout.rowHeight = kLobbyRowH;
    layout.itemCount = playerCount;
    return layout;
}

Rectangle LobbyCardRect(const Rectangle& row) {
    return { row.x, row.y + 4.0f, row.width, 66.0f };
}

Rectangle LobbyChallengeBtnRect(const Rectangle& card) {
    return { card.x + card.width - 150.0f, card.y + 10.0f, 130.0f, 46.0f };
}

Rectangle LobbyWatchBtnRect(const Rectangle& card) {
    return { card.x + card.width - 290.0f, card.y + 10.0f, 130.0f, 46.0f };
}

enum class LobbyRowAction { None, Spectate, Challenge };

LobbyRowAction HandleLobbyPlayerClick(const LobbyPlayerCard& p, const Rectangle& card, Vector2 mouse,
                                      OnlineLobby& lobby) {
    if (p.inLiveMatch) {
        if (CheckCollisionPointRec(mouse, LobbyWatchBtnRect(card))) {
            return LobbyRowAction::Spectate;
        }
    }
    if (!p.inLiveMatch && !lobby.HasPendingOutgoingChallenge() && !p.inTeamRoom) {
        if (CheckCollisionPointRec(mouse, LobbyChallengeBtnRect(card))) {
            return LobbyRowAction::Challenge;
        }
    }
    return LobbyRowAction::None;
}

void DrawLobbyPlayerCard(const LobbyPlayerCard& p, const Rectangle& card, Lang lang,
                         Vector2 mouse, OnlineLobby& lobby) {
    DrawRectangleRec(card, Color{250, 240, 225, 255});
    DrawRectangleLinesEx(card, 2, Color{60, 45, 30, 200});

    DrawText(p.displayName.c_str(), static_cast<int>(card.x + 16), static_cast<int>(card.y + 10), 20,
             Color{35, 25, 15, 255});

    char recordBuf[64];
    snprintf(recordBuf, sizeof(recordBuf), T(TK::OnlineRecordFmt, lang), p.wins, p.losses);
    DrawText(recordBuf, static_cast<int>(card.x + 16), static_cast<int>(card.y + 36), 15,
             Color{100, 85, 65, 255});

    if (p.inLiveMatch) {
        const char* playingLbl = T(TK::OnlinePlayingLabel, lang);
        int plw = MeasureText(playingLbl, 14);
        DrawText(playingLbl,
                 static_cast<int>(card.x + card.width - 320.0f - plw),
                 static_cast<int>(card.y + 14), 14, Color{180, 90, 40, 255});

        Rectangle watchBtn = LobbyWatchBtnRect(card);
        bool watchHover = CheckCollisionPointRec(mouse, watchBtn);
        DrawRectangleRec(watchBtn, watchHover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
        DrawRectangleLinesEx(watchBtn, 2, Color{60, 40, 20, 255});
        const char* watchLbl = T(TK::OnlineWatchButton, lang);
        int wlw = MeasureText(watchLbl, 16);
        DrawText(watchLbl, static_cast<int>(watchBtn.x + watchBtn.width / 2 - wlw / 2),
                 static_cast<int>(watchBtn.y + 14), 16, Color{40, 25, 10, 255});
    } else if (p.inTeamRoom) {
        const char* prepLbl = T(TK::OnlineTeamPrepLabel, lang);
        int plw = MeasureText(prepLbl, 14);
        DrawText(prepLbl,
                 static_cast<int>(card.x + card.width - 320.0f - plw),
                 static_cast<int>(card.y + 14), 14, Color{60, 110, 180, 255});
    }

    Rectangle btn = LobbyChallengeBtnRect(card);
    bool challengeDisabled = p.inLiveMatch || p.inTeamRoom || lobby.HasPendingOutgoingChallenge();
    bool hover = !challengeDisabled && CheckCollisionPointRec(mouse, btn);
    Color btnColor = challengeDisabled ? Color{160, 150, 135, 255}
                    : hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255};
    DrawRectangleRec(btn, btnColor);
    DrawRectangleLinesEx(btn, 2, Color{60, 40, 20, 255});
    const char* lbl = T(TK::OnlineChallengeButton, lang);
    int lw = MeasureText(lbl, 16);
    DrawText(lbl, static_cast<int>(btn.x + btn.width / 2 - lw / 2),
             static_cast<int>(btn.y + 14), 16, Color{40, 25, 10, 255});
}

ActiveMatchCard ToActiveMatchCard(const LobbyPlayerCard& p) {
    ActiveMatchCard m;
    m.matchId = p.liveMatchId;
    m.player1Name = p.liveMatchP1Name;
    m.player2Name = p.liveMatchP2Name;
    m.currentTurnPlayer = p.liveMatchCurrentTurn;
    m.isPlus = p.liveMatchIsPlus;
    return m;
}

Rectangle OnlineNameEditPanelRect() {
    return { cfg::SCREEN_WIDTH / 2.0f - 260.0f, 96.0f, 520.0f, 132.0f };
}

Rectangle OnlineNameFieldRect() {
    Rectangle panel = OnlineNameEditPanelRect();
    return { panel.x + 16.0f, panel.y + 38.0f, panel.width - 32.0f, 40.0f };
}

Rectangle OnlineNameSaveBtnRect() {
    Rectangle panel = OnlineNameEditPanelRect();
    return { panel.x + 72.0f, panel.y + 86.0f, 150.0f, 36.0f };
}

Rectangle OnlineNameCancelBtnRect() {
    Rectangle panel = OnlineNameEditPanelRect();
    return { panel.x + panel.width - 222.0f, panel.y + 86.0f, 150.0f, 36.0f };
}

float OnlineListStartY(bool hasIncoming, bool hasWaiting, bool hasTeamInvite, bool hasChallengeResult) {
    float y = 96.0f;
    if (hasWaiting || hasChallengeResult) y = std::max(y, 120.0f);
    if (hasTeamInvite) y = std::max(y, 96.0f + 112.0f + 14.0f);
    if (hasIncoming) {
        const float incomingTop = hasTeamInvite ? 96.0f + 112.0f + 8.0f : 96.0f;
        y = std::max(y, incomingTop + 118.0f + 14.0f);
    }
    return y;
}

const char* ChallengeVersionLabel(GameVersion v, Lang lang) {
    return (v == GameVersion::Plus) ? T(TK::PlusLabel, lang) : T(TK::ClassicLabel, lang);
}

float DrawIncomingTeamInviteBanner(const IncomingTeamInvite& inv, float bannerY, Lang lang, Vector2 mouse) {
    const float panelW = 520.0f;
    const float panelH = 112.0f;
    const float panelX = cfg::SCREEN_WIDTH / 2.0f - panelW / 2.0f;
    Rectangle panel = { panelX, bannerY, panelW, panelH };
    DrawRectangleRec(panel, Color{250, 240, 225, 255});
    DrawRectangleLinesEx(panel, 2, Color{60, 45, 30, 200});

    std::string headline = inv.fromDisplayName + " " + T(TK::OnlineTeamInviteIncoming, lang);
    int hw = MeasureText(headline.c_str(), 18);
    DrawText(headline.c_str(), cfg::SCREEN_WIDTH / 2 - hw / 2, static_cast<int>(bannerY + 14), 18,
             Color{40, 30, 20, 255});

    char modeBuf[64];
    snprintf(modeBuf, sizeof(modeBuf), T(TK::OnlineTeamInviteVersionFmt, lang),
             ChallengeVersionLabel(inv.roomVersion, lang));
    int mw = MeasureText(modeBuf, 16);
    Color modeColor = (inv.roomVersion == GameVersion::Plus)
        ? Color{200, 90, 30, 255} : Color{70, 120, 70, 255};
    DrawText(modeBuf, cfg::SCREEN_WIDTH / 2 - mw / 2, static_cast<int>(bannerY + 42), 16, modeColor);

    Rectangle acceptBtn = { cfg::SCREEN_WIDTH / 2.0f - 160, bannerY + 66, 150, 40 };
    Rectangle declineBtn = { cfg::SCREEN_WIDTH / 2.0f + 10, bannerY + 66, 150, 40 };

    bool hoverA = CheckCollisionPointRec(mouse, acceptBtn);
    DrawRectangleRec(acceptBtn, hoverA ? Color{100, 190, 110, 255} : Color{70, 160, 85, 255});
    DrawRectangleLinesEx(acceptBtn, 2, Color{20, 45, 25, 255});
    const char* acceptLbl = T(TK::OnlineAcceptButton, lang);
    int alw = MeasureText(acceptLbl, 16);
    DrawText(acceptLbl, static_cast<int>(acceptBtn.x + acceptBtn.width / 2 - alw / 2),
             static_cast<int>(acceptBtn.y + 11), 16, WHITE);

    bool hoverD = CheckCollisionPointRec(mouse, declineBtn);
    DrawRectangleRec(declineBtn, hoverD ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
    DrawRectangleLinesEx(declineBtn, 2, Color{50, 15, 10, 255});
    const char* declineLbl = T(TK::OnlineDeclineButton, lang);
    int dlw = MeasureText(declineLbl, 16);
    DrawText(declineLbl, static_cast<int>(declineBtn.x + declineBtn.width / 2 - dlw / 2),
             static_cast<int>(declineBtn.y + 11), 16, WHITE);

    return bannerY + panelH + 8.0f;
}

void DrawLobbyToggle(const Rectangle& full, const char* leftLbl, const char* rightLbl,
                     bool leftSelected, Color rightSelectedColor) {
    Rectangle leftHalf  = { full.x, full.y, full.width / 2.0f, full.height };
    Rectangle rightHalf = { full.x + full.width / 2.0f, full.y, full.width / 2.0f, full.height };

    DrawRectangleRec(full, Color{60, 45, 30, 255});
    DrawRectangleRec(leftHalf, leftSelected ? Color{230, 180, 90, 255} : Color{90, 75, 55, 255});
    DrawRectangleRec(rightHalf, !leftSelected ? rightSelectedColor : Color{90, 75, 55, 255});
    DrawRectangleLinesEx(full, 2, Color{30, 22, 12, 255});
    DrawLineEx({full.x + full.width / 2.0f, full.y},
               {full.x + full.width / 2.0f, full.y + full.height}, 2, Color{30, 22, 12, 255});

    const int fs = 16;
    int w1 = MeasureText(leftLbl, fs);
    int w2 = MeasureText(rightLbl, fs);
    DrawText(leftLbl, static_cast<int>(leftHalf.x + leftHalf.width / 2 - w1 / 2),
             static_cast<int>(leftHalf.y + leftHalf.height / 2 - fs / 2), fs,
             leftSelected ? Color{40, 25, 10, 255} : Color{230, 220, 210, 255});
    DrawText(rightLbl, static_cast<int>(rightHalf.x + rightHalf.width / 2 - w2 / 2),
             static_cast<int>(rightHalf.y + rightHalf.height / 2 - fs / 2), fs,
             !leftSelected ? Color{40, 25, 10, 255} : Color{230, 220, 210, 255});
}

Rectangle OnlineVersionToggleRect() {
    return { cfg::SCREEN_WIDTH - 184.0f, 36.0f, 168.0f, 44.0f };
}

void DrawChallengeVersionBadge(const char* verLbl, GameVersion ver, int centerX, int y) {
    Color verColor = (ver == GameVersion::Plus) ? Color{200, 90, 30, 255} : Color{70, 120, 70, 255};
    int vw = MeasureText(verLbl, 18);
    DrawText(verLbl, centerX - vw / 2, y, 18, verColor);
}

float DrawIncomingChallengeBanner(const IncomingChallenge& c, float bannerY, Lang lang, Vector2 mouse) {
    const float panelW = 540.0f;
    const float panelH = 118.0f;
    const float panelX = cfg::SCREEN_WIDTH / 2.0f - panelW / 2.0f;
    Rectangle panel = { panelX, bannerY, panelW, panelH };
    DrawRectangleRec(panel, Color{250, 240, 225, 255});
    DrawRectangleLinesEx(panel, 2, Color{60, 45, 30, 200});

    std::string headline = c.fromDisplayName + " " + T(TK::OnlineIncomingChallenge, lang);
    int hw = MeasureText(headline.c_str(), 20);
    DrawText(headline.c_str(), cfg::SCREEN_WIDTH / 2 - hw / 2, static_cast<int>(bannerY + 14), 20,
             Color{40, 30, 20, 255});

    DrawChallengeVersionBadge(ChallengeVersionLabel(c.challengeVersion, lang),
                              c.challengeVersion, cfg::SCREEN_WIDTH / 2,
                              static_cast<int>(bannerY + 44));

    Rectangle acceptBtn = { cfg::SCREEN_WIDTH / 2.0f - 160, bannerY + 72, 150, 46 };
    Rectangle declineBtn = { cfg::SCREEN_WIDTH / 2.0f + 10, bannerY + 72, 150, 46 };

    bool hoverA = CheckCollisionPointRec(mouse, acceptBtn);
    DrawRectangleRec(acceptBtn, hoverA ? Color{100, 190, 110, 255} : Color{70, 160, 85, 255});
    DrawRectangleLinesEx(acceptBtn, 2, Color{20, 45, 25, 255});
    const char* acceptLbl = T(TK::OnlineAcceptButton, lang);
    int alw = MeasureText(acceptLbl, 18);
    DrawText(acceptLbl, static_cast<int>(acceptBtn.x + acceptBtn.width / 2 - alw / 2),
             static_cast<int>(acceptBtn.y + 14), 18, WHITE);

    bool hoverD = CheckCollisionPointRec(mouse, declineBtn);
    DrawRectangleRec(declineBtn, hoverD ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
    DrawRectangleLinesEx(declineBtn, 2, Color{50, 15, 10, 255});
    const char* declineLbl = T(TK::OnlineDeclineButton, lang);
    int dlw = MeasureText(declineLbl, 18);
    DrawText(declineLbl, static_cast<int>(declineBtn.x + declineBtn.width / 2 - dlw / 2),
             static_cast<int>(declineBtn.y + 14), 18, WHITE);

    return bannerY + panelH + 8.0f;
}

} // namespace

void Game::UpdateOnlineLobby() {
    onlineLobby.Update(GetFrameTime());
    onlineNameEditCursorBlink += GetFrameTime();

    std::string roomId;
    if (onlineLobby.PollEnterTeamRoom(roomId)) {
        onlineTeamInviteScroll_ = {};
        onlineLobby.EnterTeamRoom(roomId);
        state = GameState::OnlineTeamRoom;
        return;
    }

    MatchStart ms;
    if (onlineLobby.PollMatchStart(ms)) {
        StartOnlineMatch(ms);
        return;
    }

    Vector2 m = ::GetVirtualMouse();

    if (onlineNameEditing) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 126 && onlineNameEditBuffer.size() < 24) {
                onlineNameEditBuffer.push_back(static_cast<char>(key));
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !onlineNameEditBuffer.empty()) {
            onlineNameEditBuffer.pop_back();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            onlineNameEditing = false;
            onlineNameEditBuffer.clear();
            return;
        }
        if (IsKeyPressed(KEY_ENTER)) {
            std::string saved;
            if (onlineLobby.UpdateDisplayName(onlineNameEditBuffer, saved)) {
                onlineNameEditing = false;
                onlineNameEditBuffer.clear();
            }
            return;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(m, OnlineNameSaveBtnRect())) {
                std::string saved;
                if (onlineLobby.UpdateDisplayName(onlineNameEditBuffer, saved)) {
                    onlineNameEditing = false;
                    onlineNameEditBuffer.clear();
                }
                return;
            }
            if (CheckCollisionPointRec(m, OnlineNameCancelBtnRect())) {
                onlineNameEditing = false;
                onlineNameEditBuffer.clear();
                return;
            }
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, OnlineEditNameBtnRect())) {
        onlineNameEditing = true;
        onlineNameEditBuffer = playerIdentity.DisplayName();
        onlineNameEditCursorBlink = 0.0f;
        return;
    }

    Rectangle verToggle = OnlineVersionToggleRect();
    Rectangle verLeft = { verToggle.x, verToggle.y, verToggle.width / 2.0f, verToggle.height };
    Rectangle verRight = { verToggle.x + verToggle.width / 2.0f, verToggle.y, verToggle.width / 2.0f, verToggle.height };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, verLeft)) version = GameVersion::Classic;
        else if (CheckCollisionPointRec(m, verRight)) version = GameVersion::Plus;
    }

    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 60.0f, 200, 52 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, backBtn)) {
        onlineNameEditing = false;
        onlineNameEditBuffer.clear();
        onlineLobby.LeaveLobby();
        state = GameState::MainMenu;
        return;
    }

    const auto& incoming = onlineLobby.IncomingChallenges();
    const auto& teamInvites = onlineLobby.IncomingTeamInvites();
    float listStartY = OnlineListStartY(!incoming.empty(), onlineLobby.HasPendingOutgoingChallenge(),
                                        !teamInvites.empty(), onlineLobby.HasChallengeResultNotice());

    if (!teamInvites.empty()) {
        const float bannerY = 96.0f;
        Rectangle acceptBtn = { cfg::SCREEN_WIDTH / 2.0f - 160, bannerY + 66, 150, 40 };
        Rectangle declineBtn = { cfg::SCREEN_WIDTH / 2.0f + 10, bannerY + 66, 150, 40 };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(m, acceptBtn)) onlineLobby.AcceptTeamInvite(teamInvites[0]);
            else if (CheckCollisionPointRec(m, declineBtn)) onlineLobby.DeclineTeamInvite(teamInvites[0]);
        }
    }

    if (!incoming.empty()) {
        const float bannerY = teamInvites.empty() ? 96.0f : 96.0f + 112.0f + 8.0f;
        Rectangle acceptBtn = { cfg::SCREEN_WIDTH / 2.0f - 160, bannerY + 72, 150, 46 };
        Rectangle declineBtn = { cfg::SCREEN_WIDTH / 2.0f + 10, bannerY + 72, 150, 46 };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(m, acceptBtn)) onlineLobby.AcceptChallenge(incoming[0]);
            else if (CheckCollisionPointRec(m, declineBtn)) onlineLobby.DeclineChallenge(incoming[0]);
        }
    }

    const auto& players = onlineLobby.Players();
    ScrollListLayout listLayout = OnlineLobbyListLayout(listStartY, static_cast<int>(players.size()));
    UpdateScrollList(onlineLobbyScroll_, listLayout, m);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && ScrollListPointInViewport(listLayout, m)
        && !CheckCollisionPointRec(m, ScrollListTrack(listLayout))) {
        for (int i = 0; i < static_cast<int>(players.size()); ++i) {
            Rectangle row = ScrollListRowRect(listLayout, onlineLobbyScroll_, i);
            if (!ScrollListRowVisible(listLayout, row)) continue;
            Rectangle card = LobbyCardRect(row);
            const LobbyPlayerCard& p = players[static_cast<size_t>(i)];
            switch (HandleLobbyPlayerClick(p, card, m, onlineLobby)) {
                case LobbyRowAction::Spectate:
                    StartSpectating(ToActiveMatchCard(p));
                    return;
                case LobbyRowAction::Challenge:
                    onlineLobby.SendChallenge(p, version);
                    return;
                default: break;
            }
        }
    }
}

void Game::DrawOnlineLobby() {
    ClearBackground(Color{ 235, 214, 190, 255 });
    Vector2 m = ::GetVirtualMouse();

    // Canto superior esquerdo: nome + editar
    std::string nameLine = std::string(T(TK::OnlineYourName, language)) + " " + playerIdentity.DisplayName();
    DrawText(nameLine.c_str(), 20, 44, 18, Color{40, 30, 20, 255});

    if (!onlineNameEditing) {
        Rectangle editBtn = OnlineEditNameBtnRect();
        bool hoverEdit = CheckCollisionPointRec(m, editBtn);
        DrawRectangleRec(editBtn, hoverEdit ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
        DrawRectangleLinesEx(editBtn, 2, Color{60, 40, 20, 255});
        const char* editLbl = T(TK::OnlineEditNameButton, language);
        int elw = MeasureText(editLbl, 14);
        DrawText(editLbl, static_cast<int>(editBtn.x + editBtn.width / 2 - elw / 2),
                 static_cast<int>(editBtn.y + 7), 14, Color{40, 25, 10, 255});
    }

    const char* title = T(TK::OnlineTitle, language);
    int titleW = MeasureText(title, 28);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - titleW / 2, 28, 28, Color{40, 30, 20, 255});

    Rectangle verToggle = OnlineVersionToggleRect();
    DrawLobbyToggle(verToggle, T(TK::ClassicLabel, language), T(TK::PlusLabel, language),
                    version == GameVersion::Classic, Color{230, 130, 60, 255});

    const auto& incoming = onlineLobby.IncomingChallenges();
    const auto& teamInvites = onlineLobby.IncomingTeamInvites();
    const bool hasIncoming = !incoming.empty();
    const bool hasTeamInvite = !teamInvites.empty();
    const bool hasWaiting = onlineLobby.HasPendingOutgoingChallenge();
    const bool hasChallengeResult = onlineLobby.HasChallengeResultNotice();
    float listStartY = OnlineListStartY(hasIncoming, hasWaiting, hasTeamInvite, hasChallengeResult);

    if (hasWaiting) {
        char waitingBuf[128];
        snprintf(waitingBuf, sizeof(waitingBuf), T(TK::OnlineChallengeSentVersionFmt, language),
                 ChallengeVersionLabel(onlineLobby.PendingChallengeVersion(), language));
        int ww = MeasureText(waitingBuf, 18);
        DrawText(waitingBuf, cfg::SCREEN_WIDTH / 2 - ww / 2, 88, 18, Color{200, 120, 30, 255});
    } else if (hasChallengeResult) {
        char resultBuf[160];
        const char* fmt = (onlineLobby.ChallengeResult() == OutgoingChallengeResult::Declined)
            ? T(TK::OnlineChallengeDeclinedFmt, language)
            : T(TK::OnlineChallengeExpiredFmt, language);
        snprintf(resultBuf, sizeof(resultBuf), fmt,
                 onlineLobby.ChallengeResultOpponentName().c_str());
        int rw = MeasureText(resultBuf, 18);
        Color resultColor = (onlineLobby.ChallengeResult() == OutgoingChallengeResult::Declined)
            ? Color{200, 90, 70, 255}
            : Color{170, 120, 60, 255};
        DrawText(resultBuf, cfg::SCREEN_WIDTH / 2 - rw / 2, 88, 18, resultColor);
    }

    float bannerY = 96.0f;
    if (hasTeamInvite) {
        bannerY = DrawIncomingTeamInviteBanner(teamInvites[0], bannerY, language, m);
    }

    if (hasIncoming) {
        DrawIncomingChallengeBanner(incoming[0], bannerY, language, m);
    }

    const auto& players = onlineLobby.Players();
    if (players.empty()) {
        const char* none = T(TK::OnlineNoPlayers, language);
        int nw = MeasureText(none, 18);
        DrawText(none, cfg::SCREEN_WIDTH / 2 - nw / 2, static_cast<int>(listStartY) + 20, 18,
                 Color{100, 85, 65, 255});
    } else {
        ScrollListLayout listLayout = OnlineLobbyListLayout(listStartY, static_cast<int>(players.size()));
        const Rectangle& vp = listLayout.viewport;
        BeginScissorMode(static_cast<int>(vp.x), static_cast<int>(vp.y),
                         static_cast<int>(vp.width), static_cast<int>(vp.height));
        for (int i = 0; i < static_cast<int>(players.size()); ++i) {
            Rectangle row = ScrollListRowRect(listLayout, onlineLobbyScroll_, i);
            if (!ScrollListRowVisible(listLayout, row)) continue;
            DrawLobbyPlayerCard(players[static_cast<size_t>(i)], LobbyCardRect(row), language, m,
                                onlineLobby);
        }
        EndScissorMode();
        DrawScrollListBar(onlineLobbyScroll_, listLayout);
    }

    if (onlineNameEditing) {
        DrawRectangle(0, 0, cfg::SCREEN_WIDTH, cfg::SCREEN_HEIGHT,
                      Fade(Color{25, 20, 15, 255}, 0.42f));

        Rectangle panel = OnlineNameEditPanelRect();
        DrawRectangleRec(panel, Color{252, 246, 236, 255});
        DrawRectangleLinesEx(panel, 2, Color{60, 45, 30, 255});

        const char* panelTitle = T(TK::OnlineEditNameButton, language);
        DrawText(panelTitle, static_cast<int>(panel.x + 16), static_cast<int>(panel.y + 10), 18,
                 Color{40, 30, 20, 255});

        Rectangle field = OnlineNameFieldRect();
        DrawRectangleRec(field, Color{255, 255, 255, 255});
        DrawRectangleLinesEx(field, 2, Color{80, 65, 50, 255});

        std::string shown = onlineNameEditBuffer;
        bool showCursor = fmodf(onlineNameEditCursorBlink, 1.0f) < 0.55f;
        if (showCursor) shown += "|";

        BeginScissorMode(static_cast<int>(field.x + 2), static_cast<int>(field.y + 2),
                         static_cast<int>(field.width - 4), static_cast<int>(field.height - 4));
        DrawText(shown.c_str(), static_cast<int>(field.x + 10), static_cast<int>(field.y + 10), 18,
                 Color{30, 20, 10, 255});
        EndScissorMode();

        Rectangle saveBtn = OnlineNameSaveBtnRect();
        Rectangle cancelBtn = OnlineNameCancelBtnRect();
        bool hoverSave = CheckCollisionPointRec(m, saveBtn);
        bool hoverCancel = CheckCollisionPointRec(m, cancelBtn);

        DrawRectangleRec(saveBtn, hoverSave ? Color{100, 190, 110, 255} : Color{70, 160, 85, 255});
        DrawRectangleLinesEx(saveBtn, 2, Color{20, 45, 25, 255});
        const char* saveLbl = T(TK::OnlineNameSave, language);
        int slw = MeasureText(saveLbl, 18);
        DrawText(saveLbl, static_cast<int>(saveBtn.x + saveBtn.width / 2 - slw / 2),
                 static_cast<int>(saveBtn.y + 8), 18, WHITE);

        DrawRectangleRec(cancelBtn, hoverCancel ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
        DrawRectangleLinesEx(cancelBtn, 2, Color{50, 15, 10, 255});
        const char* cancelLbl = T(TK::OnlineNameCancel, language);
        int clw = MeasureText(cancelLbl, 18);
        DrawText(cancelLbl, static_cast<int>(cancelBtn.x + cancelBtn.width / 2 - clw / 2),
                 static_cast<int>(cancelBtn.y + 8), 18, WHITE);

        const char* hint = T(TK::OnlineNameHint, language);
        int hw = MeasureText(hint, 13);
        DrawText(hint, static_cast<int>(panel.x + panel.width - hw - 16),
                 static_cast<int>(panel.y + 12), 13, Color{110, 95, 75, 255});
    }

    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 60.0f, 200, 52 };
    bool hoverBack = CheckCollisionPointRec(m, backBtn);
    DrawRectangleRec(backBtn, hoverBack ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
    DrawRectangleLinesEx(backBtn, 2, Color{60, 40, 20, 255});
    const char* backLabel = T(TK::OnlineBack, language);
    int blw = MeasureText(backLabel, 22);
    DrawText(backLabel, static_cast<int>(backBtn.x + backBtn.width / 2 - blw / 2),
             static_cast<int>(backBtn.y + backBtn.height / 2 - 11), 22, Color{40, 25, 10, 255});
}
