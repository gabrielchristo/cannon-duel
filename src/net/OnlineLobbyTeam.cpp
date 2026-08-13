#include "OnlineLobby.h"
#include "../DebugLog.h"
#include "JsonHelpers.h"
#include "NetValidation.h"

#include <raylib.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <unordered_map>

using json = nlohmann::json;

bool OnlineLobby::PollEnterTeamRoom(std::string& roomIdOut) {
    if (!hasEnterTeamRoom_) return false;
    roomIdOut = enterTeamRoomId_;
    hasEnterTeamRoom_ = false;
    enterTeamRoomId_.clear();
    return true;
}

bool OnlineLobby::LoadTeamRoom(const std::string& roomId, TeamRoomView& out) {
    if (!identity || roomId.empty()) return false;

    json rows = client.Select("team_rooms", "select=*&id=eq." + roomId);
    if (!rows.is_array() || rows.empty()) return false;

    const json& r = rows[0];
    out.roomId = roomId;
    out.captainAId = json_helpers::Str(r, "captain_a_id");
    out.captainBId = json_helpers::Str(r, "captain_b_id");
    out.partnerAId = json_helpers::Str(r, "partner_a_id");
    out.partnerBId = json_helpers::Str(r, "partner_b_id");
    out.status = json_helpers::Str(r, "status", "recruiting");
    out.version = (json_helpers::Str(r, "version", "classic") == "plus")
        ? GameVersion::Plus : GameVersion::Classic;

    std::vector<std::string> ids;
    for (const std::string* pid : { &out.captainAId, &out.captainBId, &out.partnerAId, &out.partnerBId }) {
        if (!pid->empty()) ids.push_back(*pid);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    std::unordered_map<std::string, std::string> names;
    if (!ids.empty()) {
        std::string inList;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) inList += ",";
            inList += ids[i];
        }
        json prows = client.Select("players", "select=id,display_name&id=in.(" + inList + ")");
        if (prows.is_array()) {
            for (const auto& p : prows) {
                names[json_helpers::Str(p, "id")] = json_helpers::Str(p, "display_name", "???");
            }
        }
    }
    auto nameOf = [&](const std::string& id) -> std::string {
        if (id.empty()) return "";
        auto it = names.find(id);
        return (it != names.end()) ? it->second : "???";
    };
    out.captainAName = nameOf(out.captainAId);
    out.captainBName = nameOf(out.captainBId);
    out.partnerAName = nameOf(out.partnerAId);
    out.partnerBName = nameOf(out.partnerBId);

    const std::string me = identity->Id();
    out.amCaptain = (me == out.captainAId || me == out.captainBId);
    out.myTeam = '\0';
    if (me == out.captainAId || me == out.partnerAId) out.myTeam = 'a';
    else if (me == out.captainBId || me == out.partnerBId) out.myTeam = 'b';
    return true;
}

void OnlineLobby::RefreshTeamRoom() {
    if (activeTeamRoomId_.empty()) return;
    TeamRoomView loaded;
    if (!LoadTeamRoom(activeTeamRoomId_, loaded)) return;
    teamRoom_ = loaded;
}

void OnlineLobby::RefreshIncomingTeamInvites() {
    if (!identity) return;

    json rows = client.Select("team_invites",
        "select=id,room_id,from_player_id,team,status"
        "&to_player_id=eq." + identity->Id() +
        "&status=eq.pending&order=created_at.desc&limit=5");

    incomingTeamInvites.clear();
    if (!rows.is_array()) return;

    std::unordered_map<std::string, std::string> fromNames;
    for (const auto& row : rows) {
        const std::string fromId = json_helpers::Str(row, "from_player_id");
        if (!fromId.empty()) fromNames[fromId] = "";
    }
    if (!fromNames.empty()) {
        std::string inList;
        size_t idx = 0;
        for (const auto& [id, _] : fromNames) {
            (void)_;
            if (idx++ > 0) inList += ",";
            inList += id;
        }
        json prows = client.Select("players", "select=id,display_name&id=in.(" + inList + ")");
        if (prows.is_array()) {
            for (const auto& p : prows) {
                fromNames[json_helpers::Str(p, "id")] = json_helpers::Str(p, "display_name", "???");
            }
        }
    }

    for (const auto& row : rows) {
        IncomingTeamInvite inv;
        inv.inviteId = json_helpers::Str(row, "id");
        inv.roomId = json_helpers::Str(row, "room_id");
        inv.fromPlayerId = json_helpers::Str(row, "from_player_id");
        const std::string teamStr = json_helpers::Str(row, "team", "a");
        inv.team = teamStr.empty() ? 'a' : teamStr[0];
        auto it = fromNames.find(inv.fromPlayerId);
        inv.fromDisplayName = (it != fromNames.end()) ? it->second : "???";
        incomingTeamInvites.push_back(inv);
    }
}

void OnlineLobby::EnsureTeamRealtime() {
    if (!identity || teamRealtimeStarted_ || activeTeamRoomId_.empty()) return;
    teamRealtimeStarted_ = true;

    teamRealtime_.SetOnPostgres("team_rooms", [this](const json&) {
        dirtyTeamRoom_.store(true);
    });
    teamRealtime_.SetOnPostgres("team_invites", [this](const json&) {
        dirtyTeamRoom_.store(true);
    });

    std::vector<RealtimeClient::PostgresSub> subs = {
        { "*", "public", "team_rooms", "id=eq." + activeTeamRoomId_ },
        { "*", "public", "team_invites", "to_player_id=eq." + identity->Id() },
    };
    teamRealtime_.Start("team:" + identity->Id(), subs);
}

void OnlineLobby::EnterTeamRoom(const std::string& roomId) {
    activeTeamRoomId_ = roomId;
    inTeamRoom_ = true;
    teamRoomActive_ = true;
    teamPollTimer = 0.0f;
    teamPresenceTimer_ = 0.0f;
    RefreshTeamRoom();
    EnsureTeamRealtime();
    if (registeredPlayer) UpsertPresenceWithStatus("team_room", roomId);
    DebugLogf(LOG_INFO, "LOBBY: entrou na sala de equipes %s", roomId.c_str());
}

void OnlineLobby::LeaveTeamRoom() {
    inTeamRoom_ = false;
    teamRoomActive_ = false;
    activeTeamRoomId_.clear();
    teamPollTimer = 0.0f;
    teamPresenceTimer_ = 0.0f;
    teamRealtime_.Stop();
    teamRealtimeStarted_ = false;
    teamRoom_ = {};
    if (registeredPlayer && lobbyActive_) UpsertPresenceWithStatus("idle", "");
}

void OnlineLobby::UpdateTeamRoom(float dt) {
    if (!identity || !teamRoomActive_) return;

    EnsurePlayerRegistered();
    EnsureRealtime();
    realtime_.Drain();

    EnsureTeamRealtime();
    teamRealtime_.Drain();

    teamPresenceTimer_ += dt;
    if (teamPresenceTimer_ >= TEAM_PRESENCE_HEARTBEAT_SEC) {
        teamPresenceTimer_ = 0.0f;
        if (registeredPlayer && !activeTeamRoomId_.empty()) {
            UpsertPresenceWithStatus("team_room", activeTeamRoomId_);
        }
    }

    teamPollTimer += dt;
    TickPendingChallenge(dt);
    const bool dirty = dirtyTeamRoom_.exchange(false);
    if (!dirty && teamPollTimer < TEAM_POLL_SEC) return;
    teamPollTimer = 0.0f;

    RefreshTeamRoom();
    RefreshPlayerList();
    RefreshIncomingTeamInvites();

    if (teamRoom_.status == "cancelled") {
        DebugLogf(LOG_INFO, "LOBBY: sala de equipes cancelada");
        LeaveTeamRoom();
        return;
    }

    TryResolveTeamRoomMatchStart();
}

int OnlineLobby::MyPlayerNumberInRoom(const TeamRoomView& room) const {
    if (!identity) return 0;
    const std::string me = identity->Id();
    if (me == room.captainAId) return 1;
    if (me == room.partnerAId) return 2;
    if (me == room.captainBId) return 3;
    if (me == room.partnerBId) return 4;
    return 0;
}

void OnlineLobby::BuildMatchStartFromRow(const json& mrow, const TeamRoomView& room) {
    readyMatch.matchId = json_helpers::Str(mrow, "id");
    readyMatch.myPlayerNumber = MyPlayerNumberInRoom(room);
    readyMatch.terrainSeed = static_cast<unsigned int>(json_helpers::Int64(mrow, "terrain_seed", 0));
    readyMatch.isPlus = (json_helpers::Str(mrow, "version", "classic") == "plus");
    readyMatch.format = MatchFormat::Team2v2;

    readyMatch.playerNames[0] = room.captainAName;
    readyMatch.playerNames[1] = room.partnerAName;
    readyMatch.playerNames[2] = room.captainBName;
    readyMatch.playerNames[3] = room.partnerBName;

    const std::string me = identity->Id();
    if (me == room.captainAId || me == room.partnerAId) {
        readyMatch.opponentId = room.captainBId;
        readyMatch.opponentName = room.captainBName;
    } else {
        readyMatch.opponentId = room.captainAId;
        readyMatch.opponentName = room.captainAName;
    }
}

void OnlineLobby::TryResolveTeamRoomMatchStart() {
    if (!inTeamRoom_ || hasReadyMatch || activeTeamRoomId_.empty()) return;

    json rows = client.Select("team_rooms",
        "select=match_id,status&id=eq." + activeTeamRoomId_);
    if (!rows.is_array() || rows.empty()) return;

    const std::string status = json_helpers::Str(rows[0], "status");
    if (status != "started") return;

    const std::string matchId = json_helpers::Str(rows[0], "match_id");
    if (matchId.empty()) return;

    RefreshTeamRoom();
    const int myNum = MyPlayerNumberInRoom(teamRoom_);
    if (myNum == 0) {
        DebugLogf(LOG_WARNING, "LOBBY: partida 2x2 pronta mas jogador nao mapeado na sala");
        return;
    }

    json matchRows = client.Select("matches", "select=*&id=eq." + matchId);
    if (!matchRows.is_array() || matchRows.empty()) return;

    BuildMatchStartFromRow(matchRows[0], teamRoom_);
    hasReadyMatch = true;
    DebugLogf(LOG_INFO, "LOBBY: partida 2x2 pronta match=%s eu=P%d",
              matchId.c_str(), readyMatch.myPlayerNumber);
}

void OnlineLobby::SendTeamInvite(const LobbyPlayerCard& target) {
    if (!identity || !teamRoom_.amCaptain || activeTeamRoomId_.empty()) return;

    const char* teamStr = (teamRoom_.myTeam == 'b') ? "b" : "a";
    json body = {
        { "room_id", activeTeamRoomId_ },
        { "from_player_id", identity->Id() },
        { "to_player_id", target.playerId },
        { "team", teamStr },
        { "status", "pending" }
    };
    client.Insert("team_invites", body);
}

void OnlineLobby::AcceptTeamInvite(const IncomingTeamInvite& invite) {
    if (!identity) return;

    const std::string col = (invite.team == 'b') ? "partner_b_id" : "partner_a_id";
    client.Update("team_rooms", "id=eq." + invite.roomId, json{ { col, identity->Id() } });
    client.Update("team_invites", "id=eq." + invite.inviteId, json{ { "status", "accepted" } });

    enterTeamRoomId_ = invite.roomId;
    hasEnterTeamRoom_ = true;
    DebugLogf(LOG_INFO, "LOBBY: aceitei convite de equipe sala=%s", invite.roomId.c_str());
}

void OnlineLobby::DeclineTeamInvite(const IncomingTeamInvite& invite) {
    client.Update("team_invites", "id=eq." + invite.inviteId, json{ { "status", "declined" } });
}

void OnlineLobby::CancelTeamRoom() {
    if (!identity || !teamRoom_.amCaptain || activeTeamRoomId_.empty()) return;

    client.Update("team_rooms", "id=eq." + activeTeamRoomId_, json{ { "status", "cancelled" } });
    LeaveTeamRoom();
}

void OnlineLobby::LeaveTeamAsPartner() {
    if (!identity || activeTeamRoomId_.empty() || teamRoom_.amCaptain) return;
    if (teamRoom_.myTeam != 'a' && teamRoom_.myTeam != 'b') return;

    const std::string col = (teamRoom_.myTeam == 'b') ? "partner_b_id" : "partner_a_id";
    client.Update("team_rooms", "id=eq." + activeTeamRoomId_, json{ { col, nullptr } });

    json invites = client.Select("team_invites",
        "select=id&room_id=eq." + activeTeamRoomId_ +
        "&to_player_id=eq." + identity->Id() + "&status=eq.accepted");
    if (invites.is_array()) {
        for (const auto& row : invites) {
            const std::string inviteId = json_helpers::Str(row, "id");
            if (!inviteId.empty()) {
                client.Update("team_invites", "id=eq." + inviteId, json{ { "status", "left" } });
            }
        }
    }

    DebugLogf(LOG_INFO, "LOBBY: parceiro saiu da sala %s", activeTeamRoomId_.c_str());
    LeaveTeamRoom();
}

void OnlineLobby::StartTeamMatch() {
    if (!identity || !teamRoom_.amCaptain || activeTeamRoomId_.empty()) return;
    if (teamRoom_.partnerAId.empty() || teamRoom_.partnerBId.empty()) return;

    RefreshTeamRoom();
    if (teamRoom_.partnerAId.empty() || teamRoom_.partnerBId.empty()) return;

    json check = client.Select("team_rooms", "select=status&id=eq." + activeTeamRoomId_);
    if (!check.is_array() || check.empty()) return;
    if (json_helpers::Str(check[0], "status") != "recruiting") return;

    const unsigned int seed = static_cast<unsigned int>(rand()) ^ static_cast<unsigned int>(time(nullptr));
    const bool isPlus = (teamRoom_.version == GameVersion::Plus);

    json matchBody = {
        { "player1_id", teamRoom_.captainAId },
        { "player2_id", teamRoom_.partnerAId },
        { "player3_id", teamRoom_.captainBId },
        { "player4_id", teamRoom_.partnerBId },
        { "terrain_seed", static_cast<long long>(seed) },
        { "version", isPlus ? "plus" : "classic" },
        { "match_format", "team_2v2" },
        { "current_turn_player", 1 },
        { "wind", 0.0f },
        { "status", "active" }
    };
    json created = client.Insert("matches", matchBody);
    if (!created.is_array() || created.empty()) return;

    const std::string matchId = json_helpers::Str(created[0], "id");
    if (matchId.empty()) return;

    client.Update("team_rooms", "id=eq." + activeTeamRoomId_, json{
        { "status", "started" },
        { "match_id", matchId }
    });

    BuildMatchStartFromRow(created[0], teamRoom_);
    if (readyMatch.myPlayerNumber == 0) {
        DebugLogf(LOG_WARNING, "LOBBY: capitao nao mapeado ao iniciar partida 2x2");
        hasReadyMatch = false;
        return;
    }
    hasReadyMatch = true;
    DebugLogf(LOG_INFO, "LOBBY: capitão iniciou partida 2x2 match=%s", matchId.c_str());
}
