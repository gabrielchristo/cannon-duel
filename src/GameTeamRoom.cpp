#include "Game.h"
#include "Config.h"
#include "ScrollList.h"
#include "VirtualScreen.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* ChallengeVersionLabel(GameVersion v, Lang lang) {
    return (v == GameVersion::Plus) ? T(TK::PlusLabel, lang) : T(TK::ClassicLabel, lang);
}

std::string CompositionLabel(const MatchComposition& comp) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%dx%d", comp.teamA, comp.teamB);
    return buf;
}

constexpr float kPanelW = 260.0f;
constexpr float kSlotH = 52.0f;
constexpr float kSlotGap = 8.0f;
constexpr float kTeamsTop = 112.0f;
constexpr float kFooterY = cfg::SCREEN_HEIGHT - 64.0f;

int DisplaySlotCount(int memberCount) {
    return std::max(1, std::min(memberCount + 1, MatchRoster::kMaxPerTeam));
}

float TeamSlotsHeight(int slotCount) {
    if (slotCount <= 0) return 0.0f;
    return static_cast<float>(slotCount) * kSlotH + static_cast<float>(slotCount - 1) * kSlotGap;
}

float TeamPanelX(int team) {
    return (team == 0) ? cfg::SCREEN_WIDTH / 2.0f - kPanelW - 20.0f
                       : cfg::SCREEN_WIDTH / 2.0f + 20.0f;
}

Rectangle TeamSlotRect(int team, int slotIndex, int slotCount) {
    return { TeamPanelX(team), kTeamsTop + slotIndex * (kSlotH + kSlotGap), kPanelW, kSlotH };
}

float InviteSectionTop(int slotsA, int slotsB) {
    const float teamsBottom = kTeamsTop + TeamSlotsHeight(std::max(slotsA, slotsB));
    return teamsBottom + 18.0f;
}

ScrollListLayout TeamInviteListLayout(float inviteTop, int candidateCount) {
    ScrollListLayout layout;
    const float listTop = inviteTop + 28.0f;
    layout.viewport = {
        cfg::SCREEN_WIDTH / 2.0f - 340.0f,
        listTop,
        694.0f,
        std::max(0.0f, kFooterY - 8.0f - listTop)
    };
    layout.rowHeight = 58.0f;
    layout.itemCount = candidateCount;
    return layout;
}

Rectangle InviteCardRect(const Rectangle& row) {
    return { row.x, row.y + 3.0f, row.width, 52.0f };
}

Rectangle InviteBtnRect(const Rectangle& card) {
    return { card.x + card.width - 140.0f, card.y + 6.0f, 130.0f, 40.0f };
}

void DrawInviteRow(const LobbyPlayerCard& p, const Rectangle& card, Lang lang, Vector2 mouse) {
    DrawRectangleRec(card, Color{250, 240, 225, 255});
    DrawRectangleLinesEx(card, 2, Color{60, 45, 30, 200});
    DrawText(p.displayName.c_str(), static_cast<int>(card.x + 14), static_cast<int>(card.y + 16), 16,
             Color{35, 25, 15, 255});

    Rectangle btn = InviteBtnRect(card);
    bool hover = CheckCollisionPointRec(mouse, btn);
    DrawRectangleRec(btn, hover ? Color{100, 190, 110, 255} : Color{70, 160, 85, 255});
    DrawRectangleLinesEx(btn, 2, Color{20, 45, 25, 255});
    const char* lbl = T(TK::OnlineTeamInviteButton, lang);
    int lw = MeasureText(lbl, 15);
    DrawText(lbl, static_cast<int>(btn.x + btn.width / 2 - lw / 2),
             static_cast<int>(btn.y + 12), 15, WHITE);
}

Rectangle CancelBtnRect() {
    return { cfg::SCREEN_WIDTH / 2.0f - 230.0f, kFooterY, 200.0f, 48.0f };
}

Rectangle StartBtnRect() {
    return { cfg::SCREEN_WIDTH / 2.0f + 30.0f, kFooterY, 200.0f, 48.0f };
}

Rectangle LeaveBtnRect() {
    return { cfg::SCREEN_WIDTH / 2.0f - 120.0f, kFooterY, 240.0f, 48.0f };
}

bool PlayerInTeamRoom(const TeamRoomView& room, const std::string& playerId) {
    for (const TeamRoomMember& m : room.teamA) {
        if (m.playerId == playerId) return true;
    }
    for (const TeamRoomMember& m : room.teamB) {
        if (m.playerId == playerId) return true;
    }
    return false;
}

bool CanInviteFromLobby(const LobbyPlayerCard& p, const TeamRoomView& room) {
    if (p.inLiveMatch) return false;
    if (PlayerInTeamRoom(room, p.playerId)) return false;
    if (p.inTeamRoom && p.teamRoomId != room.roomId) return false;
    return true;
}

bool MyTeamCanInvite(const TeamRoomView& room) {
    return room.amCaptain && room.CanInvite(room.myTeam);
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

void DrawHeader(const TeamRoomView& room, Lang lang) {
    const MatchComposition comp = room.Composition();
    std::string titleStr = std::string(T(TK::OnlineTeamRoomTitle, lang)) + "  "
        + CompositionLabel(comp);
    int tw = MeasureText(titleStr.c_str(), 22);
    DrawText(titleStr.c_str(), cfg::SCREEN_WIDTH / 2 - tw / 2, 20, 22, Color{40, 30, 20, 255});

    const char* verLbl = ChallengeVersionLabel(room.version, lang);
    int vw = MeasureText(verLbl, 16);
    DrawText(verLbl, cfg::SCREEN_WIDTH / 2 - vw / 2, 50, 16,
             room.version == GameVersion::Plus ? Color{200, 90, 30, 255} : Color{80, 65, 45, 255});

    const char* hint = T(TK::OnlineTeamCompositionHint, lang);
    int hw = MeasureText(hint, 14);
    DrawText(hint, cfg::SCREEN_WIDTH / 2 - hw / 2, 74, 14, Color{110, 95, 75, 255});
}

void DrawTeamColumn(int team, const std::vector<TeamRoomMember>& members, Lang lang) {
    const int slots = DisplaySlotCount(static_cast<int>(members.size()));
    const char* lbl = (team == 0) ? T(TK::OnlineTeamLabelA, lang) : T(TK::OnlineTeamLabelB, lang);
    const float panelX = TeamPanelX(team);
    int law = MeasureText(lbl, 16);
    DrawText(lbl, static_cast<int>(panelX + kPanelW / 2 - law / 2), 92, 16,
             team == 0 ? Color{60, 90, 160, 255} : Color{180, 70, 50, 255});

    for (int s = 0; s < slots; ++s) {
        Rectangle slot = TeamSlotRect(team, s, slots);
        const bool filled = s < static_cast<int>(members.size());
        DrawRectangleRec(slot, filled ? Color{250, 240, 225, 255} : Color{220, 210, 195, 255});
        DrawRectangleLinesEx(slot, 2, Color{60, 45, 30, 200});
        if (filled) {
            const TeamRoomMember& mem = members[static_cast<size_t>(s)];
            DrawText(mem.displayName.c_str(), static_cast<int>(slot.x + 12), static_cast<int>(slot.y + 16), 16,
                     Color{35, 25, 15, 255});
            if (mem.slot == 0) {
                const char* cap = (lang == Lang::PT_BR) ? "Capitao" : "Captain";
                DrawText(cap, static_cast<int>(slot.x + slot.width - 72), static_cast<int>(slot.y + 6), 13,
                         Color{100, 85, 65, 255});
            }
        } else {
            const char* empty = T(TK::OnlineTeamSlotEmpty, lang);
            int ew = MeasureText(empty, 14);
            DrawText(empty, static_cast<int>(slot.x + slot.width / 2 - ew / 2),
                     static_cast<int>(slot.y + 18), 14, Color{120, 105, 85, 255});
        }
    }
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
    const int slotsA = DisplaySlotCount(room->TeamACount());
    const int slotsB = DisplaySlotCount(room->TeamBCount());
    const float inviteTop = InviteSectionTop(slotsA, slotsB);

    if (MyTeamCanInvite(*room)) {
        const auto candidates = BuildInviteCandidates(onlineLobby.Players(), *room);
        ScrollListLayout inviteLayout = TeamInviteListLayout(inviteTop, static_cast<int>(candidates.size()));
        UpdateScrollList(onlineTeamInviteScroll_, inviteLayout, m);
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (room->amCaptain && CheckCollisionPointRec(m, CancelBtnRect())) {
            onlineLobby.CancelTeamRoom();
            state = GameState::OnlineLobby;
            return;
        }
        if (!room->amCaptain && CheckCollisionPointRec(m, LeaveBtnRect())) {
            onlineLobby.LeaveTeamAsPartner();
            state = GameState::OnlineLobby;
            return;
        }
        if (room->amCaptain && CheckCollisionPointRec(m, StartBtnRect())
            && !room->teamA.empty() && !room->teamB.empty()) {
            onlineLobby.StartTeamMatch();
            return;
        }

        if (MyTeamCanInvite(*room)) {
            const auto candidates = BuildInviteCandidates(onlineLobby.Players(), *room);
            ScrollListLayout inviteLayout = TeamInviteListLayout(inviteTop, static_cast<int>(candidates.size()));

            if (ScrollListPointInViewport(inviteLayout, m)
                && !CheckCollisionPointRec(m, ScrollListTrack(inviteLayout))) {
                for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
                    Rectangle row = ScrollListRowRect(inviteLayout, onlineTeamInviteScroll_, i);
                    if (!ScrollListRowVisible(inviteLayout, row)) continue;
                    Rectangle card = InviteCardRect(row);
                    if (CheckCollisionPointRec(m, InviteBtnRect(card))) {
                        onlineLobby.SendTeamInvite(*candidates[static_cast<size_t>(i)]);
                        break;
                    }
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

    DrawHeader(*room, language);

    DrawTeamColumn(0, room->teamA, language);
    DrawTeamColumn(1, room->teamB, language);

    const int slotsA = DisplaySlotCount(room->TeamACount());
    const int slotsB = DisplaySlotCount(room->TeamBCount());
    const float inviteTop = InviteSectionTop(slotsA, slotsB);

    if (MyTeamCanInvite(*room)) {
        const char* listTitle = T(TK::OnlineTeamLobbyInviteTitle, language);
        int ltw = MeasureText(listTitle, 15);
        DrawText(listTitle, cfg::SCREEN_WIDTH / 2 - ltw / 2, static_cast<int>(inviteTop), 15,
                 Color{80, 65, 45, 255});

        const auto candidates = BuildInviteCandidates(onlineLobby.Players(), *room);
        if (candidates.empty()) {
            const char* none = T(TK::OnlineNoPlayers, language);
            int nw = MeasureText(none, 15);
            DrawText(none, cfg::SCREEN_WIDTH / 2 - nw / 2, static_cast<int>(inviteTop + 34), 15,
                     Color{100, 85, 65, 255});
        } else {
            ScrollListLayout inviteLayout = TeamInviteListLayout(inviteTop, static_cast<int>(candidates.size()));
            const Rectangle& vp = inviteLayout.viewport;
            BeginScissorMode(static_cast<int>(vp.x), static_cast<int>(vp.y),
                             static_cast<int>(vp.width), static_cast<int>(vp.height));
            for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
                Rectangle row = ScrollListRowRect(inviteLayout, onlineTeamInviteScroll_, i);
                if (!ScrollListRowVisible(inviteLayout, row)) continue;
                DrawInviteRow(*candidates[static_cast<size_t>(i)], InviteCardRect(row), language, m);
            }
            EndScissorMode();
            DrawScrollListBar(onlineTeamInviteScroll_, inviteLayout);
        }
    } else if (room->amCaptain && !room->CanInvite(room->myTeam)) {
        const char* fullMsg = (language == Lang::PT_BR)
            ? "Sua equipe esta completa (max 5)." : "Your team is full (max 5).";
        int fw = MeasureText(fullMsg, 15);
        DrawText(fullMsg, cfg::SCREEN_WIDTH / 2 - fw / 2, static_cast<int>(inviteTop + 8), 15,
                 Color{80, 120, 70, 255});
    } else if (!room->amCaptain) {
        const char* waitMsg = (language == Lang::PT_BR)
            ? "Aguardando o capitao iniciar a partida..." : "Waiting for captain to start the match...";
        int ww = MeasureText(waitMsg, 15);
        DrawText(waitMsg, cfg::SCREEN_WIDTH / 2 - ww / 2, static_cast<int>(inviteTop + 8), 15,
                 Color{100, 85, 65, 255});
    }

    if (room->amCaptain) {
        bool canStart = !room->teamA.empty() && !room->teamB.empty();

        Rectangle cancelBtn = CancelBtnRect();
        bool hoverC = CheckCollisionPointRec(m, cancelBtn);
        DrawRectangleRec(cancelBtn, hoverC ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
        DrawRectangleLinesEx(cancelBtn, 2, Color{50, 15, 10, 255});
        const char* cLbl = T(TK::OnlineTeamCancel, language);
        int clw = MeasureText(cLbl, 18);
        DrawText(cLbl, static_cast<int>(cancelBtn.x + cancelBtn.width / 2 - clw / 2),
                 static_cast<int>(cancelBtn.y + 14), 18, WHITE);

        Rectangle startBtn = StartBtnRect();
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
        Rectangle leaveBtn = LeaveBtnRect();
        bool hoverL = CheckCollisionPointRec(m, leaveBtn);
        DrawRectangleRec(leaveBtn, hoverL ? Color{210, 90, 80, 255} : Color{180, 65, 55, 255});
        DrawRectangleLinesEx(leaveBtn, 2, Color{50, 15, 10, 255});
        const char* lLbl = T(TK::OnlineTeamLeave, language);
        int llw = MeasureText(lLbl, 18);
        DrawText(lLbl, static_cast<int>(leaveBtn.x + leaveBtn.width / 2 - llw / 2),
                 static_cast<int>(leaveBtn.y + 14), 18, WHITE);
    }
}
