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
        if (CheckCollisionPointRec(m, btn1P)) StartMatch(GameMode::PvAI);
        else if (CheckCollisionPointRec(m, btn2P)) StartMatch(GameMode::PvP);
        else if (CheckCollisionPointRec(m, btnOnline)) {
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
// Layout compartilhado entre Update/Draw do lobby, pra garantir que a área
// clicável e a área desenhada do botão "DESAFIAR" de cada card batem certinho.
Rectangle OnlineCardRect(int index, float startY) {
    return { cfg::SCREEN_WIDTH / 2.0f - 350.0f, startY + index * 74.0f, 700.0f, 66.0f };
}
Rectangle OnlineChallengeBtnRect(int index, float startY) {
    Rectangle card = OnlineCardRect(index, startY);
    return { card.x + card.width - 150.0f, card.y + 10.0f, 130.0f, 46.0f };
}
} // namespace

void Game::UpdateOnlineLobby() {
    onlineLobby.Update(GetFrameTime());

    MatchStart ms;
    if (onlineLobby.PollMatchStart(ms)) {
        StartOnlineMatch(ms);
        return;
    }

    Vector2 m = ::GetVirtualMouse();
    Rectangle backBtn = { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 60.0f, 200, 52 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, backBtn)) {
        onlineLobby.LeaveLobby();
        state = GameState::MainMenu;
        return;
    }

    const auto& incoming = onlineLobby.IncomingChallenges();
    float listStartY = 150.0f;
    if (!incoming.empty()) {
        listStartY = 226.0f;
        Rectangle acceptBtn = { cfg::SCREEN_WIDTH / 2.0f - 160, 160, 150, 46 };
        Rectangle declineBtn = { cfg::SCREEN_WIDTH / 2.0f + 10, 160, 150, 46 };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(m, acceptBtn)) onlineLobby.AcceptChallenge(incoming[0], version == GameVersion::Plus);
            else if (CheckCollisionPointRec(m, declineBtn)) onlineLobby.DeclineChallenge(incoming[0]);
        }
    }

    const auto& players = onlineLobby.Players();
    int maxCards = 6;
    if (!onlineLobby.HasPendingOutgoingChallenge() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        for (int i = 0; i < static_cast<int>(players.size()) && i < maxCards; ++i) {
            Rectangle btn = OnlineChallengeBtnRect(i, listStartY);
            if (CheckCollisionPointRec(m, btn)) {
                onlineLobby.SendChallenge(players[i]);
                break;
            }
        }
    }
}

void Game::DrawOnlineLobby() {
    ClearBackground(Color{ 235, 214, 190, 255 });

    const char* title = T(TK::OnlineTitle, language);
    int fs = 40;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 30, fs, Color{40, 30, 20, 255});

    std::string myLine = std::string(T(TK::OnlineYourName, language)) + " " + playerIdentity.DisplayName()
        + "  [" + (version == GameVersion::Plus ? T(TK::PlusLabel, language) : T(TK::ClassicLabel, language)) + "]";
    int myTw = MeasureText(myLine.c_str(), 18);
    DrawText(myLine.c_str(), cfg::SCREEN_WIDTH / 2 - myTw / 2, 84, 18, Color{80, 65, 45, 255});

    if (onlineLobby.HasPendingOutgoingChallenge()) {
        const char* waiting = T(TK::OnlineChallengeSent, language);
        int ww = MeasureText(waiting, 18);
        DrawText(waiting, cfg::SCREEN_WIDTH / 2 - ww / 2, 108, 18, Color{200, 120, 30, 255});
    }

    Vector2 m = ::GetVirtualMouse();
    const auto& incoming = onlineLobby.IncomingChallenges();
    float listStartY = 150.0f;

    if (!incoming.empty()) {
        listStartY = 226.0f;
        const IncomingChallenge& c = incoming[0];
        std::string msg = c.fromDisplayName + " " + T(TK::OnlineIncomingChallenge, language);
        int mw = MeasureText(msg.c_str(), 20);
        DrawText(msg.c_str(), cfg::SCREEN_WIDTH / 2 - mw / 2, 130, 20, Color{40, 30, 20, 255});

        Rectangle acceptBtn = { cfg::SCREEN_WIDTH / 2.0f - 160, 160, 150, 46 };
        Rectangle declineBtn = { cfg::SCREEN_WIDTH / 2.0f + 10, 160, 150, 46 };

        bool hoverA = CheckCollisionPointRec(m, acceptBtn);
        DrawRectangleRec(acceptBtn, hoverA ? Color{100, 190, 110, 255} : Color{70, 160, 85, 255});
        DrawRectangleLinesEx(acceptBtn, 2, Color{20, 45, 25, 255});
        const char* acceptLbl = T(TK::OnlineAcceptButton, language);
        int alw = MeasureText(acceptLbl, 18);
        DrawText(acceptLbl, static_cast<int>(acceptBtn.x + acceptBtn.width / 2 - alw / 2),
                 static_cast<int>(acceptBtn.y + 14), 18, WHITE);

        bool hoverD = CheckCollisionPointRec(m, declineBtn);
        DrawRectangleRec(declineBtn, hoverD ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
        DrawRectangleLinesEx(declineBtn, 2, Color{50, 15, 10, 255});
        const char* declineLbl = T(TK::OnlineDeclineButton, language);
        int dlw = MeasureText(declineLbl, 18);
        DrawText(declineLbl, static_cast<int>(declineBtn.x + declineBtn.width / 2 - dlw / 2),
                 static_cast<int>(declineBtn.y + 14), 18, WHITE);
    }

    const auto& players = onlineLobby.Players();
    if (players.empty()) {
        const char* none = T(TK::OnlineNoPlayers, language);
        int nw = MeasureText(none, 18);
        DrawText(none, cfg::SCREEN_WIDTH / 2 - nw / 2, static_cast<int>(listStartY) + 20, 18, Color{100, 85, 65, 255});
    } else {
        int maxCards = 6;
        for (int i = 0; i < static_cast<int>(players.size()) && i < maxCards; ++i) {
            const LobbyPlayerCard& p = players[i];
            Rectangle card = OnlineCardRect(i, listStartY);
            DrawRectangleRec(card, Color{250, 240, 225, 255});
            DrawRectangleLinesEx(card, 2, Color{60, 45, 30, 200});

            DrawText(p.displayName.c_str(), static_cast<int>(card.x + 16), static_cast<int>(card.y + 10), 20,
                     Color{35, 25, 15, 255});

            char recordBuf[64];
            snprintf(recordBuf, sizeof(recordBuf), T(TK::OnlineRecordFmt, language), p.wins, p.losses);
            DrawText(recordBuf, static_cast<int>(card.x + 16), static_cast<int>(card.y + 36), 15,
                     Color{100, 85, 65, 255});

            Rectangle btn = OnlineChallengeBtnRect(i, listStartY);
            bool disabled = onlineLobby.HasPendingOutgoingChallenge();
            bool hover = !disabled && CheckCollisionPointRec(m, btn);
            Color btnColor = disabled ? Color{160, 150, 135, 255}
                            : hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255};
            DrawRectangleRec(btn, btnColor);
            DrawRectangleLinesEx(btn, 2, Color{60, 40, 20, 255});
            const char* lbl = T(TK::OnlineChallengeButton, language);
            int lw = MeasureText(lbl, 16);
            DrawText(lbl, static_cast<int>(btn.x + btn.width / 2 - lw / 2),
                     static_cast<int>(btn.y + 14), 16, Color{40, 25, 10, 255});
        }
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
