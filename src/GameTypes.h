#pragma once

#include <string>

enum class MatchFormat {
    Duel1v1 = 1,
    Team2v2 = 2,
    Team3v3 = 3,
};

struct MatchComposition {
    int teamA = 1;
    int teamB = 1;

    int TotalPlayers() const { return teamA + teamB; }
    bool IsTeamGame() const { return teamA > 1 || teamB > 1; }
};

inline bool IsTeamMode(MatchFormat format) {
    return format != MatchFormat::Duel1v1;
}

inline int TeamOfPlayerNum(int playerNum, const MatchComposition& comp) {
    if (playerNum <= 0) return -1;
    return (playerNum <= comp.teamA) ? 0 : 1;
}

inline MatchComposition CompositionFromFormat(MatchFormat format) {
    const int n = static_cast<int>(format);
    return { n, n };
}

inline MatchComposition ParseMatchComposition(const std::string& matchFormat,
                                              int teamACount, int teamBCount) {
    if (matchFormat.size() > 5 && matchFormat.rfind("team_", 0) == 0) {
        const size_t x = matchFormat.find('x', 5);
        if (x != std::string::npos && x + 1 < matchFormat.size()) {
            try {
                const int a = std::stoi(matchFormat.substr(5, x - 5));
                const int b = std::stoi(matchFormat.substr(x + 1));
                if (a >= 1 && a <= 5 && b >= 1 && b <= 5) {
                    return { a, b };
                }
            } catch (...) {}
        }
    }
    if (teamACount >= 1 && teamBCount >= 1) {
        return { teamACount, teamBCount };
    }
    if (matchFormat == "duel_1v1") return { 1, 1 };
    if (matchFormat == "team_2v2") return { 2, 2 };
    return CompositionFromFormat(MatchFormat::Duel1v1);
}

enum class GameMode { PvP, PvAI, Online };
enum class GameVersion { Classic, Plus };
enum class GameState {
    MainMenu, FormatSelect, About, Instructions, OnlineLobby, OnlineTeamRoom,
    Aiming, ProjectileFlying, RemoteShotReplay, RemoteProjectileLive,
    TurnTransition, RoundOver
};
enum class AimPhase { Angle, Power };

enum class RoundOutcome {
    None, Draw, DrawBuried,
    P1Wins, P2Wins, P1WinsBuried, P2WinsBuried,
    TeamAWins, TeamBWins, TeamAWinsBuried, TeamBWinsBuried,
};

inline bool OnlineDidIWin(int winnerPlayer, int myPlayerNumber, const MatchComposition& comp) {
    if (winnerPlayer == 0 || myPlayerNumber == 0) return false;
    if (!comp.IsTeamGame()) return winnerPlayer == myPlayerNumber;
    const int myTeam = TeamOfPlayerNum(myPlayerNumber, comp);
    if (myTeam < 0) return false;
    return (winnerPlayer == 1 && myTeam == 0) || (winnerPlayer == 2 && myTeam == 1);
}

inline RoundOutcome RoundOutcomeFromOnlineWinner(int winnerPlayer, const MatchComposition& comp,
                                                 bool buried) {
    if (!comp.IsTeamGame()) {
        if (winnerPlayer == 1) return buried ? RoundOutcome::P1WinsBuried : RoundOutcome::P1Wins;
        if (winnerPlayer == 2) return buried ? RoundOutcome::P2WinsBuried : RoundOutcome::P2Wins;
        return RoundOutcome::Draw;
    }
    if (winnerPlayer == 1) return buried ? RoundOutcome::TeamAWinsBuried : RoundOutcome::TeamAWins;
    if (winnerPlayer == 2) return buried ? RoundOutcome::TeamBWinsBuried : RoundOutcome::TeamBWins;
    return RoundOutcome::Draw;
}

inline int WinnerWhenPlayerLeaves(int leavingPlayerNum, const MatchComposition& comp) {
    if (leavingPlayerNum <= 0) return 0;
    if (!comp.IsTeamGame()) return (leavingPlayerNum == 1) ? 2 : 1;
    const int team = TeamOfPlayerNum(leavingPlayerNum, comp);
    return (team == 0) ? 2 : 1;
}

inline std::string MatchFormatDb(const MatchComposition& comp) {
    if (comp.teamA == 1 && comp.teamB == 1) return "duel_1v1";
    return "team_" + std::to_string(comp.teamA) + "x" + std::to_string(comp.teamB);
}
