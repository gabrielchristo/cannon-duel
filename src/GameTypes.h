#pragma once

enum class MatchFormat {
    Duel1v1 = 1,
    Team2v2 = 2,
    Team3v3 = 3,
};

inline bool IsTeamMode(MatchFormat format) {
    return format != MatchFormat::Duel1v1;
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

inline bool OnlineDidIWin(int winnerPlayer, int myPlayerNumber, MatchFormat fmt) {
    if (winnerPlayer == 0 || myPlayerNumber == 0) return false;
    if (!IsTeamMode(fmt)) return winnerPlayer == myPlayerNumber;
    if (winnerPlayer == 1) return myPlayerNumber == 1 || myPlayerNumber == 2;
    if (winnerPlayer == 2) return myPlayerNumber == 3 || myPlayerNumber == 4;
    return false;
}

inline RoundOutcome RoundOutcomeFromOnlineWinner(int winnerPlayer, MatchFormat fmt, bool buried) {
    if (!IsTeamMode(fmt)) {
        if (winnerPlayer == 1) return buried ? RoundOutcome::P1WinsBuried : RoundOutcome::P1Wins;
        if (winnerPlayer == 2) return buried ? RoundOutcome::P2WinsBuried : RoundOutcome::P2Wins;
        return RoundOutcome::Draw;
    }
    if (winnerPlayer == 1) return buried ? RoundOutcome::TeamAWinsBuried : RoundOutcome::TeamAWins;
    if (winnerPlayer == 2) return buried ? RoundOutcome::TeamBWinsBuried : RoundOutcome::TeamBWins;
    return RoundOutcome::Draw;
}

inline int WinnerWhenPlayerLeaves(int leavingPlayerNum, MatchFormat fmt) {
    if (leavingPlayerNum <= 0) return 0;
    if (!IsTeamMode(fmt)) return (leavingPlayerNum == 1) ? 2 : 1;
    return (leavingPlayerNum <= 2) ? 2 : 1;
}
