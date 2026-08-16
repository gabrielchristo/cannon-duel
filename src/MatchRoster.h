#pragma once

#include "Cannon.h"
#include "GameTypes.h"
#include "Terrain.h"

#include <array>
#include <vector>

// Roster de canhões por equipe — extensível até 5v5 online.
class MatchRoster {
public:
    static constexpr int kMaxCannons = 10;
    static constexpr int kMaxPerTeam = 5;

    void Setup(MatchFormat format, Terrain& terrain);
    void SetupComposition(const MatchComposition& comp, Terrain& terrain);

    MatchFormat Format() const { return format_; }
    MatchComposition Composition() const { return { perTeamA_, perTeamB_ }; }
    int PerTeam() const { return std::max(perTeamA_, perTeamB_); }
    int PerTeamA() const { return perTeamA_; }
    int PerTeamB() const { return perTeamB_; }
    int CannonCount() const { return perTeamA_ + perTeamB_; }

    Cannon& At(int slot);
    const Cannon& At(int slot) const;
    Cannon& AtPlayerNum(int playerNum);
    const Cannon& AtPlayerNum(int playerNum) const;

    int TeamOfSlot(int slot) const;
    int TeamOfPlayerNum(int playerNum) const;
    bool AreAllies(int slotA, int slotB) const;

    std::vector<int> EnemySlots(int shooterSlot) const;
    std::vector<int> AllySlots(int shooterSlot) const;

    // Opção B: A0 → B0 → A1 → B1 → … (generalizado para N por equipe).
    int NextSlotInterleaved(int currentSlot) const;
    int NextLivingSlotInterleaved(int currentSlot) const;
    int NextLivingPlayerNum(int currentPlayerNum) const;
    bool IsPlayerAlive(int playerNum) const;

    int LowestHpEnemySlot(int shooterSlot) const;

    bool IsTeamEliminated(int team) const;
    bool IsTeamBuried(int team, const Terrain& terrain) const;

    float MaxEnemyDistancePx(int team) const;

    bool IsHumanSlot(int slot, GameMode mode) const;
    bool IsAISlot(int slot, GameMode mode) const;

    Color ColorForSlot(int slot) const;

    Texture2D* SpriteForSlot(int slot, Texture2D* leftTex, Texture2D* rightTex) const;
    Texture2D* SpriteForTeamSlot(int slot, std::array<Texture2D, kMaxPerTeam>& teamATex,
                                 std::array<Texture2D, kMaxPerTeam>& teamBTex) const;

private:
    MatchFormat format_ = MatchFormat::Duel1v1;
    int perTeamA_ = 1;
    int perTeamB_ = 1;
    std::array<Cannon, kMaxCannons> cannons_{};

    void PlaceCannons(Terrain& terrain);
    float TeamSpacing(int count) const;
};
