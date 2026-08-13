#pragma once

#include "Cannon.h"
#include "GameTypes.h"
#include "Terrain.h"

#include <array>
#include <vector>

// Roster de canhões por equipe — extensível para 1v1, 2v2 e futuro 3v3.
class MatchRoster {
public:
    static constexpr int kMaxCannons = 6;

    void Setup(MatchFormat format, Terrain& terrain);

    MatchFormat Format() const { return format_; }
    int PerTeam() const { return perTeam_; }
    int CannonCount() const { return perTeam_ * 2; }

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

    int LowestHpEnemySlot(int shooterSlot) const;

    bool IsTeamEliminated(int team) const;
    bool IsTeamBuried(int team, const Terrain& terrain) const;

    float MaxEnemyDistancePx(int team) const;

    bool IsHumanSlot(int slot, GameMode mode) const;
    bool IsAISlot(int slot, GameMode mode) const;

    Color ColorForSlot(int slot) const;

    Texture2D* SpriteForSlot(int slot, Texture2D* leftTex, Texture2D* rightTex) const;

private:
    MatchFormat format_ = MatchFormat::Duel1v1;
    int perTeam_ = 1;
    std::array<Cannon, kMaxCannons> cannons_{};

    void PlaceCannons(Terrain& terrain);
};
