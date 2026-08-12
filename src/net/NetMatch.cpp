#include "NetMatch.h"


using json = nlohmann::json;

void NetMatch::Begin(const std::string& id, int myPlayerNum,
                      const std::string& oppId, const std::string& oppName) {
    matchId = id;
    myPlayerNumber = myPlayerNum;
    opponentId = oppId;
    opponentName = oppName;
    isMyTurn = (myPlayerNumber == 1); // current_turn_player começa em 1
    lastSeenTurnNumber = 0;
    pollTimer = 0.0f;
}

void NetMatch::SubmitMyTurn(float impactX, float impactY, float craterRadius,
                             float damageP1, float damageP2, float nextWind,
                             bool matchOver, int winnerPlayer) {
    lastSeenTurnNumber++;

    json body = {
        { "match_id", matchId },
        { "turn_number", lastSeenTurnNumber },
        { "shooter_player", myPlayerNumber },
        { "impact_x", impactX },
        { "impact_y", impactY },
        { "crater_radius", craterRadius },
        { "damage_p1", damageP1 },
        { "damage_p2", damageP2 },
        { "next_wind", nextWind },
        { "next_turn_player", matchOver ? myPlayerNumber : (myPlayerNumber == 1 ? 2 : 1) },
        { "match_over", matchOver },
        { "winner_player", winnerPlayer }
    };
    client.Insert("match_turns", body);

    json matchUpdate = {
        { "current_turn_player", matchOver ? myPlayerNumber : (myPlayerNumber == 1 ? 2 : 1) },
        { "wind", nextWind },
        { "status", matchOver ? "finished" : "active" }
    };
    if (matchOver) matchUpdate["winner_player"] = winnerPlayer;
    client.Update("matches", "id=eq." + matchId, matchUpdate);

    isMyTurn = false;
}

bool NetMatch::PollOpponentTurn(RemoteTurnResult& out, float dt) {
    if (isMyTurn) return false;

    pollTimer += dt;
    if (pollTimer < POLL_INTERVAL_SEC) return false;
    pollTimer = 0.0f;

    json rows = client.Select("match_turns",
        "select=*&match_id=eq." + matchId +
        "&turn_number=eq." + std::to_string(lastSeenTurnNumber + 1));

    if (!rows.is_array() || rows.empty()) return false;

    const json& row = rows[0];
    out.turnNumber = row.value("turn_number", 0);
    out.shooterPlayer = row.value("shooter_player", 1);
    out.impactX = row.value("impact_x", 0.0f);
    out.impactY = row.value("impact_y", 0.0f);
    out.craterRadius = row.value("crater_radius", 0.0f);
    out.damageP1 = row.value("damage_p1", 0.0f);
    out.damageP2 = row.value("damage_p2", 0.0f);
    out.nextWind = row.value("next_wind", 0.0f);
    out.nextTurnPlayer = row.value("next_turn_player", 1);
    out.matchOver = row.value("match_over", false);
    out.winnerPlayer = row.value("winner_player", 0);

    lastSeenTurnNumber = out.turnNumber;
    isMyTurn = (out.nextTurnPlayer == myPlayerNumber);
    return true;
}

void NetMatch::LeaveMatch() {
    matchId.clear();
    opponentId.clear();
    opponentName.clear();
    lastSeenTurnNumber = 0;
}

