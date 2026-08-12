#pragma once

enum class GameMode { PvP, PvAI, Online };
enum class GameVersion { Classic, Plus };
enum class GameState {
    MainMenu, About, Instructions, OnlineLobby,
    Aiming, ProjectileFlying, RemoteShotReplay, RemoteProjectileLive,
    TurnTransition, RoundOver
};
enum class AimPhase { Angle, Power };

enum class RoundOutcome {
    None, Draw, DrawBuried, P1Wins, P2Wins, P1WinsBuried, P2WinsBuried
};
