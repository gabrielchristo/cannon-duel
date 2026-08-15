#include "OnlineLobby.h"
#include "../DebugLog.h"
#include "../ShopCatalog.h"
#include "JsonHelpers.h"
#include "NetValidation.h"

#include <raylib.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;

namespace {

void SortMembers(std::vector<TeamRoomMember>& members) {
    std::sort(members.begin(), members.end(),
              [](const TeamRoomMember& a, const TeamRoomMember& b) { return a.slot < b.slot; });
}

std::vector<std::string> CollectRoomPlayerIds(const TeamRoomView& room) {
    std::vector<std::string> ids;
    for (const TeamRoomMember& m : room.teamA) {
        if (!m.playerId.empty()) ids.push_back(m.playerId);
    }
    for (const TeamRoomMember& m : room.teamB) {
        if (!m.playerId.empty()) ids.push_back(m.playerId);
    }
    return ids;
}

} // namespace

bool OnlineLobby::PollEnterTeamRoom(std::string& roomIdOut) {
    if (!hasEnterTeamRoom_) return false;
    roomIdOut = enterTeamRoomId_;
    hasEnterTeamRoom_ = false;
    enterTeamRoomId_.clear();
    return true;
}

void OnlineLobby::InsertRoomCaptains(const std::string& roomId, const std::string& captainA,
                                     const std::string& captainB) {
    client.Insert("team_room_members", json{
        { "room_id", roomId }, { "team", "a" }, { "slot", 0 }, { "player_id", captainA }
    });
    client.Insert("team_room_members", json{
        { "room_id", roomId }, { "team", "b" }, { "slot", 0 }, { "player_id", captainB }
    });
}

bool OnlineLobby::LoadTeamRoom(const std::string& roomId, TeamRoomView& out) {
    if (!identity || roomId.empty()) return false;

    json rows = client.Select("team_rooms", "select=*&id=eq." + roomId);
    if (!rows.is_array() || rows.empty()) return false;

    const json& r = rows[0];
    out.roomId = roomId;
    out.status = json_helpers::Str(r, "status", "recruiting");
    out.version = (json_helpers::Str(r, "version", "classic") == "plus")
        ? GameVersion::Plus : GameVersion::Classic;
    out.teamA.clear();
    out.teamB.clear();

    json members = client.Select("team_room_members",
        "select=team,slot,player_id&room_id=eq." + roomId + "&order=slot.asc");
    if (members.is_array() && !members.empty()) {
        for (const auto& m : members) {
            TeamRoomMember tm;
            tm.playerId = json_helpers::Str(m, "player_id");
            tm.slot = json_helpers::Int(m, "slot", 0);
            const std::string team = json_helpers::Str(m, "team", "a");
            if (team == "b") out.teamB.push_back(tm);
            else out.teamA.push_back(tm);
        }
    } else {
        // Fallback legado partner_a/b
        const std::string capA = json_helpers::Str(r, "captain_a_id");
        const std::string capB = json_helpers::Str(r, "captain_b_id");
        const std::string partA = json_helpers::Str(r, "partner_a_id");
        const std::string partB = json_helpers::Str(r, "partner_b_id");
        if (!capA.empty()) out.teamA.push_back({ capA, "", 0 });
        if (!partA.empty()) out.teamA.push_back({ partA, "", 1 });
        if (!capB.empty()) out.teamB.push_back({ capB, "", 0 });
        if (!partB.empty()) out.teamB.push_back({ partB, "", 1 });
    }

    SortMembers(out.teamA);
    SortMembers(out.teamB);

    std::vector<std::string> ids = CollectRoomPlayerIds(out);
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
    for (TeamRoomMember& m : out.teamA) m.displayName = nameOf(m.playerId);
    for (TeamRoomMember& m : out.teamB) m.displayName = nameOf(m.playerId);

    const std::string me = identity->Id();
    out.amCaptain = false;
    out.myTeam = '\0';
    for (const TeamRoomMember& m : out.teamA) {
        if (m.playerId == me) {
            out.myTeam = 'a';
            if (m.slot == 0) out.amCaptain = true;
        }
    }
    for (const TeamRoomMember& m : out.teamB) {
        if (m.playerId == me) {
            out.myTeam = 'b';
            if (m.slot == 0) out.amCaptain = true;
        }
    }
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

    std::unordered_set<std::string> myRoomIds;
    json memberRows = client.Select("team_room_members",
        "select=room_id&player_id=eq." + identity->Id());
    if (memberRows.is_array()) {
        for (const auto& mrow : memberRows) {
            const std::string rid = json_helpers::Str(mrow, "room_id");
            if (!rid.empty()) myRoomIds.insert(rid);
        }
    }

    std::unordered_map<std::string, std::string> roomVersions;
    std::unordered_map<std::string, std::string> roomStatuses;
    std::unordered_set<std::string> roomIds;
    for (const auto& row : rows) {
        const std::string rid = json_helpers::Str(row, "room_id");
        if (!rid.empty()) roomIds.insert(rid);
    }
    if (!roomIds.empty()) {
        std::string roomInList;
        size_t ridx = 0;
        for (const std::string& rid : roomIds) {
            if (ridx++ > 0) roomInList += ",";
            roomInList += rid;
        }
        json roomRows = client.Select("team_rooms", "select=id,version,status&id=in.(" + roomInList + ")");
        if (roomRows.is_array()) {
            for (const auto& rr : roomRows) {
                const std::string rid = json_helpers::Str(rr, "id");
                roomVersions[rid] = json_helpers::Str(rr, "version", "classic");
                roomStatuses[rid] = json_helpers::Str(rr, "status", "recruiting");
            }
        }
    }

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
        const std::string roomId = json_helpers::Str(row, "room_id");
        if (myRoomIds.count(roomId) > 0) {
            const std::string inviteId = json_helpers::Str(row, "id");
            if (!inviteId.empty()) {
                client.Update("team_invites", "id=eq." + inviteId, json{ { "status", "accepted" } });
            }
            continue;
        }
        const auto statusIt = roomStatuses.find(roomId);
        if (statusIt != roomStatuses.end() &&
            (statusIt->second == "started" || statusIt->second == "cancelled")) {
            continue;
        }

        IncomingTeamInvite inv;
        inv.inviteId = json_helpers::Str(row, "id");
        inv.roomId = json_helpers::Str(row, "room_id");
        inv.fromPlayerId = json_helpers::Str(row, "from_player_id");
        const std::string teamStr = json_helpers::Str(row, "team", "a");
        inv.team = teamStr.empty() ? 'a' : teamStr[0];
        auto it = fromNames.find(inv.fromPlayerId);
        inv.fromDisplayName = (it != fromNames.end()) ? it->second : "???";
        const auto vit = roomVersions.find(inv.roomId);
        inv.roomVersion = (vit != roomVersions.end() && vit->second == "plus")
            ? GameVersion::Plus : GameVersion::Classic;
        incomingTeamInvites.push_back(inv);
    }
}

void OnlineLobby::EnsureTeamRealtime() {
    if (!identity || teamRealtimeStarted_ || activeTeamRoomId_.empty()) return;
    teamRealtimeStarted_ = true;

    teamRealtime_.SetOnPostgres("team_rooms", [this](const json&) {
        dirtyTeamRoom_.store(true);
    });
    teamRealtime_.SetOnPostgres("team_room_members", [this](const json&) {
        dirtyTeamRoom_.store(true);
    });
    teamRealtime_.SetOnPostgres("team_invites", [this](const json&) {
        dirtyTeamRoom_.store(true);
    });

    std::vector<RealtimeClient::PostgresSub> subs = {
        { "*", "public", "team_rooms", "id=eq." + activeTeamRoomId_ },
        { "*", "public", "team_room_members", "room_id=eq." + activeTeamRoomId_ },
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
    DebugLogf(LOG_INFO, "LOBBY: entrou na sala de composicao %s", roomId.c_str());
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
        DebugLogf(LOG_INFO, "LOBBY: sala cancelada");
        LeaveTeamRoom();
        return;
    }

    TryResolveTeamRoomMatchStart();
}

int OnlineLobby::MyPlayerNumberInRoom(const TeamRoomView& room) const {
    if (!identity) return 0;
    const std::string me = identity->Id();
    int playerNum = 1;
    for (const TeamRoomMember& m : room.teamA) {
        if (m.playerId == me) return playerNum;
        ++playerNum;
    }
    for (const TeamRoomMember& m : room.teamB) {
        if (m.playerId == me) return playerNum;
        ++playerNum;
    }
    return 0;
}

void OnlineLobby::BuildMatchStartFromRow(const json& mrow, const TeamRoomView& room) {
    readyMatch.matchId = json_helpers::Str(mrow, "id");
    readyMatch.myPlayerNumber = MyPlayerNumberInRoom(room);
    readyMatch.terrainSeed = static_cast<unsigned int>(json_helpers::Int64(mrow, "terrain_seed", 0));
    readyMatch.isPlus = (json_helpers::Str(mrow, "version", "classic") == "plus");
    const MatchComposition roomComp = room.Composition();
    readyMatch.composition = ParseMatchComposition(
        json_helpers::Str(mrow, "match_format", MatchFormatDb(roomComp).c_str()),
        json_helpers::Int(mrow, "team_a_count", roomComp.teamA),
        json_helpers::Int(mrow, "team_b_count", roomComp.teamB));
    if (readyMatch.composition.TotalPlayers() != roomComp.TotalPlayers()) {
        DebugLogf(LOG_WARNING,
                  "LOBBY: composicao DB (%dx%d) difere da sala (%dx%d) — usando sala",
                  readyMatch.composition.teamA, readyMatch.composition.teamB,
                  roomComp.teamA, roomComp.teamB);
        readyMatch.composition = roomComp;
    }

    for (auto& name : readyMatch.playerNames) {
        name.clear();
    }
    for (auto& color : readyMatch.equippedCannonColors) {
        color = kDefaultCannonColorId;
    }
    for (auto& skin : readyMatch.equippedCannonSkins) {
        skin = kDefaultCannonSkinId;
    }
    for (auto& effect : readyMatch.equippedCannonEffects) {
        effect = kDefaultCannonEffectId;
    }
    for (auto& effect : readyMatch.equippedNameEffects) {
        effect = kDefaultNameEffectId;
    }

    int idx = 0;
    for (const TeamRoomMember& m : room.teamA) {
        if (idx < MatchRoster::kMaxCannons) readyMatch.playerNames[idx++] = m.displayName;
    }
    for (const TeamRoomMember& m : room.teamB) {
        if (idx < MatchRoster::kMaxCannons) readyMatch.playerNames[idx++] = m.displayName;
    }

    const int totalPlayers = readyMatch.composition.TotalPlayers();
    if (idx < totalPlayers) {
        std::string pidList;
        for (int p = idx; p < totalPlayers && p < MatchRoster::kMaxCannons; ++p) {
            const std::string key = "player" + std::to_string(p + 1) + "_id";
            const std::string pid = json_helpers::Str(mrow, key.c_str());
            if (pid.empty()) continue;
            if (!pidList.empty()) pidList += ",";
            pidList += pid;
        }
        if (!pidList.empty()) {
            std::unordered_map<std::string, std::string> names;
            json prows = client.Select("players", "select=id,display_name&id=in.(" + pidList + ")");
            if (prows.is_array()) {
                for (const auto& p : prows) {
                    names[json_helpers::Str(p, "id")] = json_helpers::Str(p, "display_name", "???");
                }
            }
            for (int p = idx; p < totalPlayers && p < MatchRoster::kMaxCannons; ++p) {
                const std::string key = "player" + std::to_string(p + 1) + "_id";
                const std::string pid = json_helpers::Str(mrow, key.c_str());
                readyMatch.playerNames[p] = names.count(pid) ? names.at(pid) : "???";
            }
        }
    }

    const std::string me = identity->Id();
    if (TeamOfPlayerNum(readyMatch.myPlayerNumber, readyMatch.composition) == 0) {
        readyMatch.opponentId = room.teamB.empty() ? "" : room.teamB[0].playerId;
        readyMatch.opponentName = room.teamB.empty() ? "???" : room.teamB[0].displayName;
    } else {
        readyMatch.opponentId = room.teamA.empty() ? "" : room.teamA[0].playerId;
        readyMatch.opponentName = room.teamA.empty() ? "???" : room.teamA[0].displayName;
    }

    FillMatchStartCosmetics(mrow, readyMatch.composition.TotalPlayers());
}

void OnlineLobby::FillMatchStartCosmetics(const json& mrow, int totalPlayers) {
    if (!identity || totalPlayers <= 0) return;

    std::string pidList;
    for (int p = 0; p < totalPlayers && p < MatchRoster::kMaxCannons; ++p) {
        const std::string key = "player" + std::to_string(p + 1) + "_id";
        const std::string pid = json_helpers::Str(mrow, key.c_str());
        if (pid.empty()) continue;
        if (!pidList.empty()) pidList += ",";
        pidList += pid;
    }
    if (pidList.empty()) return;

    json prows = client.Select("players",
        "select=id,equipped_cannon_color,equipped_cannon_skin,equipped_cannon_effect,equipped_name_effect&id=in.("
        + pidList + ")");
    if (!prows.is_array()) return;

    struct SlotCosmetics {
        std::string color;
        std::string skin;
        std::string effect;
        std::string nameEffect;
    };
    std::unordered_map<std::string, SlotCosmetics> cosmetics;
    for (const auto& row : prows) {
        const std::string id = json_helpers::Str(row, "id");
        cosmetics[id] = {
            json_helpers::Str(row, "equipped_cannon_color", kDefaultCannonColorId),
            json_helpers::Str(row, "equipped_cannon_skin", kDefaultCannonSkinId),
            json_helpers::Str(row, "equipped_cannon_effect", kDefaultCannonEffectId),
            json_helpers::Str(row, "equipped_name_effect", kDefaultNameEffectId)
        };
    }

    for (int p = 0; p < totalPlayers && p < MatchRoster::kMaxCannons; ++p) {
        const std::string key = "player" + std::to_string(p + 1) + "_id";
        const std::string pid = json_helpers::Str(mrow, key.c_str());
        if (pid.empty() || !cosmetics.count(pid)) continue;
        readyMatch.equippedCannonColors[p] = cosmetics.at(pid).color;
        readyMatch.equippedCannonSkins[p] = cosmetics.at(pid).skin;
        readyMatch.equippedCannonEffects[p] = cosmetics.at(pid).effect;
        readyMatch.equippedNameEffects[p] = cosmetics.at(pid).nameEffect;
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
        DebugLogf(LOG_WARNING, "LOBBY: partida pronta mas jogador nao mapeado na sala");
        return;
    }

    json matchRows = client.Select("matches", "select=*&id=eq." + matchId);
    if (!matchRows.is_array() || matchRows.empty()) return;

    BuildMatchStartFromRow(matchRows[0], teamRoom_);
    hasReadyMatch = true;
    DebugLogf(LOG_INFO, "LOBBY: partida pronta match=%s eu=P%d (%dx%d)",
              matchId.c_str(), readyMatch.myPlayerNumber,
              readyMatch.composition.teamA, readyMatch.composition.teamB);
}

void OnlineLobby::SendTeamInvite(const LobbyPlayerCard& target) {
    if (!identity || !teamRoom_.amCaptain || activeTeamRoomId_.empty()) return;
    if (!teamRoom_.CanInvite(teamRoom_.myTeam)) return;

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

    RefreshTeamRoom();
    TeamRoomView room;
    if (!LoadTeamRoom(invite.roomId, room)) return;

    const char* teamStr = (invite.team == 'b') ? "b" : "a";
    const std::vector<TeamRoomMember>& teamMembers = (invite.team == 'b') ? room.teamB : room.teamA;
    if (static_cast<int>(teamMembers.size()) >= MatchRoster::kMaxPerTeam) return;

    int nextSlot = 0;
    for (const TeamRoomMember& m : teamMembers) {
        nextSlot = std::max(nextSlot, m.slot + 1);
    }

    client.Insert("team_room_members", json{
        { "room_id", invite.roomId },
        { "team", teamStr },
        { "slot", nextSlot },
        { "player_id", identity->Id() }
    });
    client.Update("team_invites", "id=eq." + invite.inviteId, json{ { "status", "accepted" } });

    incomingTeamInvites.erase(
        std::remove_if(incomingTeamInvites.begin(), incomingTeamInvites.end(),
                       [&](const IncomingTeamInvite& i) { return i.inviteId == invite.inviteId; }),
        incomingTeamInvites.end());

    enterTeamRoomId_ = invite.roomId;
    hasEnterTeamRoom_ = true;
    DebugLogf(LOG_INFO, "LOBBY: aceitei convite sala=%s slot=%d", invite.roomId.c_str(), nextSlot);
}

void OnlineLobby::DeclineTeamInvite(const IncomingTeamInvite& invite) {
    client.Update("team_invites", "id=eq." + invite.inviteId, json{ { "status", "declined" } });
    incomingTeamInvites.erase(
        std::remove_if(incomingTeamInvites.begin(), incomingTeamInvites.end(),
                       [&](const IncomingTeamInvite& i) { return i.inviteId == invite.inviteId; }),
        incomingTeamInvites.end());
}

void OnlineLobby::CancelTeamRoom() {
    if (!identity || !teamRoom_.amCaptain || activeTeamRoomId_.empty()) return;

    client.Update("team_rooms", "id=eq." + activeTeamRoomId_, json{ { "status", "cancelled" } });
    LeaveTeamRoom();
}

void OnlineLobby::LeaveTeamAsPartner() {
    if (!identity || activeTeamRoomId_.empty() || teamRoom_.amCaptain) return;
    if (teamRoom_.myTeam != 'a' && teamRoom_.myTeam != 'b') return;

    client.Delete("team_room_members",
        "room_id=eq." + activeTeamRoomId_ + "&player_id=eq." + identity->Id());

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

    RefreshTeamRoom();
    if (teamRoom_.teamA.empty() || teamRoom_.teamB.empty()) return;

    json check = client.Select("team_rooms", "select=status&id=eq." + activeTeamRoomId_);
    if (!check.is_array() || check.empty()) return;
    if (json_helpers::Str(check[0], "status") != "recruiting") return;

    const MatchComposition comp = teamRoom_.Composition();
    const unsigned int seed = static_cast<unsigned int>(rand()) ^ static_cast<unsigned int>(time(nullptr));
    const bool isPlus = (teamRoom_.version == GameVersion::Plus);

    json matchBody = {
        { "terrain_seed", static_cast<long long>(seed) },
        { "version", isPlus ? "plus" : "classic" },
        { "match_format", MatchFormatDb(comp) },
        { "team_a_count", comp.teamA },
        { "team_b_count", comp.teamB },
        { "current_turn_player", 1 },
        { "wind", 0.0f },
        { "status", "active" }
    };

    int playerIdx = 1;
    for (const TeamRoomMember& m : teamRoom_.teamA) {
        if (playerIdx > MatchRoster::kMaxCannons) break;
        matchBody["player" + std::to_string(playerIdx) + "_id"] = m.playerId;
        ++playerIdx;
    }
    for (const TeamRoomMember& m : teamRoom_.teamB) {
        if (playerIdx > MatchRoster::kMaxCannons) break;
        matchBody["player" + std::to_string(playerIdx) + "_id"] = m.playerId;
        ++playerIdx;
    }

    json created = client.Insert("matches", matchBody);
    if (!created.is_array() || created.empty()) return;

    const std::string matchId = json_helpers::Str(created[0], "id");
    if (matchId.empty()) return;

    client.Update("team_rooms", "id=eq." + activeTeamRoomId_, json{
        { "status", "started" },
        { "match_id", matchId }
    });

    json matchRows = client.Select("matches", "select=*&id=eq." + matchId);
    if (!matchRows.is_array() || matchRows.empty()) return;

    BuildMatchStartFromRow(matchRows[0], teamRoom_);
    if (readyMatch.myPlayerNumber == 0) {
        DebugLogf(LOG_WARNING, "LOBBY: capitao nao mapeado ao iniciar partida");
        hasReadyMatch = false;
        return;
    }
    hasReadyMatch = true;
    DebugLogf(LOG_INFO, "LOBBY: capitao iniciou partida %dx%d match=%s",
              comp.teamA, comp.teamB, matchId.c_str());
}
