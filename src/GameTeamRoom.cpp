#include "Game.h"
#include "Config.h"
#include "VirtualScreen.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* ChallengeFormatLabel(OnlineChallengeFormat f, Lang lang) {
    return (f == OnlineChallengeFormat::Team2v2)
        ? T(TK::OnlineFormat2v2, lang) : T(TK::OnlineFormat1v1, lang);
}

const char* ChallengeVersionLabel(GameVersion v, Lang lang) {
    return (v == GameVersion::Plus) ? T(TK::PlusLabel, lang) : T(TK::ClassicLabel, lang);
}

Rectangle OnlineTeamSlotRect(int team, int slotIndex) {
    const float panelW = 280.0f;
    const float x = (team == 0) ? cfg::SCREEN_WIDTH / 2.0f - panelW - 24.0f
                                : cfg::SCREEN_WIDTH / 2.0f + 24.0f;
    return { x, 130.0f + slotIndex * 78.0f, panelW, 68.0f };
}

Rectangle OnlineTeamInviteBtnRect(int listIndex) {
    return { cfg::SCREEN_WIDTH / 2.0f + 220.0f, 350.0f + listIndex * 74.0f, 130.0f, 46.0f };
}

Rectangle OnlineTeamCardRect(int listIndex) {
    return { cfg::SCREEN_WIDTH / 2.0f - 350.0f, 350.0f + listIndex * 74.0f, 700.0f, 66.0f };
}

bool PlayerInTeamRoom(const TeamRoomView& room, const std::string& playerId) {
    return playerId == room.captainAId || playerId == room.partnerAId
        || playerId == room.captainBId || playerId == room.partnerBId;
}

bool CanInviteFromLobby(const LobbyPlayerCard& p, const TeamRoomView& room) {
    if (p.inLiveMatch) return false;
    if (PlayerInTeamRoom(room, p.playerId)) return false;
    if (p.inTeamRoom && p.teamRoomId != room.roomId) return false;
    return true;
}

bool MyTeamSlotFull(const TeamRoomView& room) {
    if (room.myTeam == 'a') return !room.partnerAId.empty();
    if (room.myTeam == 'b') return !room.partnerBId.empty();
    return true;
}

std::vector<const LobbyPlayerCard*> BuildInviteCandidates(const std::vector<LobbyPlayerCard>& players,
                                                          const TeamRoomView& room) {
    std::vector<const LobbyPlayerCard*> out;
    out.reserve(players.size());
    for (const LobbyPlayerCard& p : players) {
        if (CanInviteFromLobby(p, room)) out.push_back(&p);
    }
    return out;
}

} // namespace

void Game::UpdateOnlineTeamRoom() {
    onlineLobby.UpdateTeamRoom(GetFrameTime());

    MatchStart ms;
    if (onlineLobby.PollMatchStart(ms)) {
        StartOnlineMatch(ms);
        onlineLobby.LeaveTeamRoom();
        return;
    }

    const TeamRoomView* room = onlineLobby.ActiveTeamRoom();
    if (!room) {
        state = GameState::OnlineLobby;
        return;
    }

    Vector2 m = ::GetVirtualMouse();

    Rectangle cancelBtn = { cfg::SCREEN_WIDTH / 2.0f - 230.0f, cfg::SCREEN_HEIGHT - 68.0f, 200.0f, 48.0f };
    Rectangle startBtn  = { cfg::SCREEN_WIDTH / 2.0f + 30.0f,  cfg::SCREEN_HEIGHT - 68.0f, 200.0f, 48.0f };
    Rectangle leaveBtn  = { cfg::SCREEN_WIDTH / 2.0f - 120.0f, cfg::SCREEN_HEIGHT - 68.0f, 240.0f, 48.0f };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (room->amCaptain && CheckCollisionPointRec(m, cancelBtn)) {
            onlineLobby.CancelTeamRoom();
            state = GameState::OnlineLobby;
            return;
        }
        if (!room->amCaptain && CheckCollisionPointRec(m, leaveBtn)) {
            onlineLobby.LeaveTeamAsPartner();
            state = GameState::OnlineLobby;
            return;
        }
        if (room->amCaptain && CheckCollisionPointRec(m, startBtn)
            && !room->partnerAId.empty() && !room->partnerBId.empty()) {
            onlineLobby.StartTeamMatch();
            return;
        }

        if (room->amCaptain && !MyTeamSlotFull(*room)) {
            const auto candidates = BuildInviteCandidates(onlineLobby.Players(), *room);
            for (int i = 0; i < static_cast<int>(candidates.size()) && i < 6; ++i) {
                Rectangle btn = OnlineTeamInviteBtnRect(i);
                if (CheckCollisionPointRec(m, btn)) {
                    onlineLobby.SendTeamInvite(*candidates[static_cast<size_t>(i)]);
                    break;
                }
            }
        }
    }
}

void Game::DrawOnlineTeamRoom() {
    ClearBackground(Color{ 235, 214, 190, 255 });
    Vector2 m = ::GetVirtualMouse();

    const TeamRoomView* room = onlineLobby.ActiveTeamRoom();
    if (!room) return;

    const char* title = T(TK::OnlineTeamRoomTitle, language);
    int tw = MeasureText(title, 26);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 24, 26, Color{40, 30, 20, 255});

    const char* verLbl = ChallengeVersionLabel(room->version, language);
    int vw = MeasureText(verLbl, 18);
    DrawText(verLbl, cfg::SCREEN_WIDTH / 2 - vw / 2, 58, 18,
             room->version == GameVersion::Plus ? Color{200, 90, 30, 255} : Color{80, 65, 45, 255});

    auto drawSlot = [&](int team, int slotIndex, const std::string& name, bool filled) {
        Rectangle slot = OnlineTeamSlotRect(team, slotIndex);
        DrawRectangleRec(slot, filled ? Color{250, 240, 225, 255} : Color{220, 210, 195, 255});
        DrawRectangleLinesEx(slot, 2, Color{60, 45, 30, 200});
        if (filled) {
            DrawText(name.c_str(), static_cast<int>(slot.x + 14), static_cast<int>(slot.y + 22), 18,
                     Color{35, 25, 15, 255});
        } else {
            const char* empty = T(TK::OnlineTeamSlotEmpty, language);
            int ew = MeasureText(empty, 16);
            DrawText(empty, static_cast<int>(slot.x + slot.width / 2 - ew / 2),
                     static_cast<int>(slot.y + 24), 16, Color{120, 105, 85, 255});
        }
    };

    const char* lblA = T(TK::OnlineTeamLabelA, language);
    const char* lblB = T(TK::OnlineTeamLabelB, language);
    int law = MeasureText(lblA, 18);
    int lbw = MeasureText(lblB, 18);
    DrawText(lblA, static_cast<int>(OnlineTeamSlotRect(0, 0).x + 140 - law / 2), 104, 18, Color{60, 90, 160, 255});
    DrawText(lblB, static_cast<int>(OnlineTeamSlotRect(1, 0).x + 140 - lbw / 2), 104, 18, Color{180, 70, 50, 255});

    drawSlot(0, 0, room->captainAName, true);
    drawSlot(0, 1, room->partnerAName, !room->partnerAId.empty());
    drawSlot(1, 0, room->captainBName, true);
    drawSlot(1, 1, room->partnerBName, !room->partnerBId.empty());

    if (room->partnerAId.empty() || room->partnerBId.empty()) {
        const char* wait = T(TK::OnlineTeamWaitingPartners, language);
        int ww = MeasureText(wait, 16);
        DrawText(wait, cfg::SCREEN_WIDTH / 2 - ww / 2, 292, 16, Color{100, 85, 65, 255});
    }

    if (room->amCaptain && !MyTeamSlotFull(*room)) {
        const char* listTitle = T(TK::OnlineTeamLobbyInviteTitle, language);
        int ltw = MeasureText(listTitle, 16);
        DrawText(listTitle, cfg::SCREEN_WIDTH / 2 - ltw / 2, 322, 16, Color{80, 65, 45, 255});

        const auto candidates = BuildInviteCandidates(onlineLobby.Players(), *room);
        if (candidates.empty()) {
            const char* none = T(TK::OnlineNoPlayers, language);
            int nw = MeasureText(none, 16);
            DrawText(none, cfg::SCREEN_WIDTH / 2 - nw / 2, 360, 16, Color{100, 85, 65, 255});
        }
        for (int i = 0; i < static_cast<int>(candidates.size()) && i < 6; ++i) {
            const LobbyPlayerCard& p = *candidates[static_cast<size_t>(i)];

            Rectangle card = OnlineTeamCardRect(i);
            DrawRectangleRec(card, Color{250, 240, 225, 255});
            DrawRectangleLinesEx(card, 2, Color{60, 45, 30, 200});
            DrawText(p.displayName.c_str(), static_cast<int>(card.x + 16), static_cast<int>(card.y + 22), 18,
                     Color{35, 25, 15, 255});

            Rectangle btn = OnlineTeamInviteBtnRect(i);
            bool hover = CheckCollisionPointRec(m, btn);
            DrawRectangleRec(btn, hover ? Color{100, 190, 110, 255} : Color{70, 160, 85, 255});
            DrawRectangleLinesEx(btn, 2, Color{20, 45, 25, 255});
            const char* lbl = T(TK::OnlineTeamInviteButton, language);
            int lw = MeasureText(lbl, 16);
            DrawText(lbl, static_cast<int>(btn.x + btn.width / 2 - lw / 2),
                     static_cast<int>(btn.y + 14), 16, WHITE);
        }
    } else if (room->amCaptain && MyTeamSlotFull(*room)) {
        const char* fullMsg = (language == Lang::PT_BR) ? "Sua equipe esta completa." : "Your team is full.";
        int fw = MeasureText(fullMsg, 16);
        DrawText(fullMsg, cfg::SCREEN_WIDTH / 2 - fw / 2, 340, 16, Color{80, 120, 70, 255});
    }

    if (room->amCaptain) {
        Rectangle cancelBtn = { cfg::SCREEN_WIDTH / 2.0f - 230.0f, cfg::SCREEN_HEIGHT - 68.0f, 200.0f, 48.0f };
        Rectangle startBtn  = { cfg::SCREEN_WIDTH / 2.0f + 30.0f,  cfg::SCREEN_HEIGHT - 68.0f, 200.0f, 48.0f };
        bool canStart = !room->partnerAId.empty() && !room->partnerBId.empty();

        bool hoverC = CheckCollisionPointRec(m, cancelBtn);
        DrawRectangleRec(cancelBtn, hoverC ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
        DrawRectangleLinesEx(cancelBtn, 2, Color{50, 15, 10, 255});
        const char* cLbl = T(TK::OnlineTeamCancel, language);
        int clw = MeasureText(cLbl, 18);
        DrawText(cLbl, static_cast<int>(cancelBtn.x + cancelBtn.width / 2 - clw / 2),
                 static_cast<int>(cancelBtn.y + 14), 18, WHITE);

        bool hoverS = canStart && CheckCollisionPointRec(m, startBtn);
        DrawRectangleRec(startBtn, canStart
            ? (hoverS ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255})
            : Color{160, 150, 135, 255});
        DrawRectangleLinesEx(startBtn, 2, Color{60, 40, 20, 255});
        const char* sLbl = T(TK::OnlineTeamStart, language);
        int slw = MeasureText(sLbl, 18);
        DrawText(sLbl, static_cast<int>(startBtn.x + startBtn.width / 2 - slw / 2),
                 static_cast<int>(startBtn.y + 14), 18, Color{40, 25, 10, 255});
    } else {
        Rectangle leaveBtn = { cfg::SCREEN_WIDTH / 2.0f - 120.0f, cfg::SCREEN_HEIGHT - 68.0f, 240.0f, 48.0f };
        bool hoverL = CheckCollisionPointRec(m, leaveBtn);
        DrawRectangleRec(leaveBtn, hoverL ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
        DrawRectangleLinesEx(leaveBtn, 2, Color{50, 15, 10, 255});
        const char* lLbl = T(TK::OnlineTeamLeave, language);
        int llw = MeasureText(lLbl, 18);
        DrawText(lLbl, static_cast<int>(leaveBtn.x + leaveBtn.width / 2 - llw / 2),
                 static_cast<int>(leaveBtn.y + 14), 18, WHITE);
    }
}
