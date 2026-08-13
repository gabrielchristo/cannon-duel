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
    MainMenu, FormatSelect, About, Instructions, OnlineLobby,
    Aiming, ProjectileFlying, RemoteShotReplay, RemoteProjectileLive,
    TurnTransition, RoundOver
};
enum class AimPhase { Angle, Power };

enum class RoundOutcome {
    None, Draw, DrawBuried,
    P1Wins, P2Wins, P1WinsBuried, P2WinsBuried,
    TeamAWins, TeamBWins, TeamAWinsBuried, TeamBWinsBuried,
};
