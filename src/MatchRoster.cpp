#include "MatchRoster.h"

#include "Config.h"
#include "DebugLog.h"

#include <algorithm>
#include <cmath>

void MatchRoster::Setup(MatchFormat format, Terrain& terrain) {
    format_ = format;
    const int n = static_cast<int>(format);
    perTeamA_ = n;
    perTeamB_ = n;
    PlaceCannons(terrain);
}

void MatchRoster::SetupComposition(const MatchComposition& comp, Terrain& terrain) {
    format_ = MatchFormat::Duel1v1;
    perTeamA_ = std::clamp(comp.teamA, 1, kMaxPerTeam);
    perTeamB_ = std::clamp(comp.teamB, 1, kMaxPerTeam);
    for (auto& c : cannons_) {
        c = Cannon{};
    }
    PlaceCannons(terrain);
}

Cannon& MatchRoster::At(int slot) {
    return cannons_.at(static_cast<size_t>(slot));
}

const Cannon& MatchRoster::At(int slot) const {
    return cannons_.at(static_cast<size_t>(slot));
}

Cannon& MatchRoster::AtPlayerNum(int playerNum) {
    const int slot = playerNum - 1;
    if (slot < 0 || slot >= CannonCount()) {
        DebugLogf(LOG_ERROR, "ROSTER: playerNum %d invalido (count=%d)", playerNum, CannonCount());
        return At(std::clamp(slot, 0, std::max(0, CannonCount() - 1)));
    }
    return At(slot);
}

const Cannon& MatchRoster::AtPlayerNum(int playerNum) const {
    const int slot = playerNum - 1;
    if (slot < 0 || slot >= CannonCount()) {
        return At(std::clamp(slot, 0, std::max(0, CannonCount() - 1)));
    }
    return At(slot);
}

int MatchRoster::TeamOfSlot(int slot) const {
    return (slot < perTeamA_) ? 0 : 1;
}

int MatchRoster::TeamOfPlayerNum(int playerNum) const {
    return TeamOfSlot(playerNum - 1);
}

bool MatchRoster::AreAllies(int slotA, int slotB) const {
    return TeamOfSlot(slotA) == TeamOfSlot(slotB);
}

std::vector<int> MatchRoster::EnemySlots(int shooterSlot) const {
    const int team = TeamOfSlot(shooterSlot);
    std::vector<int> out;
    for (int i = 0; i < CannonCount(); ++i) {
        if (TeamOfSlot(i) != team) out.push_back(i);
    }
    return out;
}

std::vector<int> MatchRoster::AllySlots(int shooterSlot) const {
    const int team = TeamOfSlot(shooterSlot);
    std::vector<int> out;
    for (int i = 0; i < CannonCount(); ++i) {
        if (i != shooterSlot && TeamOfSlot(i) == team) out.push_back(i);
    }
    return out;
}

int MatchRoster::NextSlotInterleaved(int currentSlot) const {
    if (CannonCount() <= 1) return 0;

    std::vector<int> order;
    order.reserve(static_cast<size_t>(CannonCount()));
    const int rounds = std::max(perTeamA_, perTeamB_);
    for (int i = 0; i < rounds; ++i) {
        if (i < perTeamA_) order.push_back(i);
        if (i < perTeamB_) order.push_back(perTeamA_ + i);
    }

    for (size_t i = 0; i < order.size(); ++i) {
        if (order[i] == currentSlot) {
            return order[(i + 1) % order.size()];
        }
    }
    return order[0];
}

int MatchRoster::NextLivingSlotInterleaved(int currentSlot) const {
    if (CannonCount() <= 1) return 0;

    int slot = NextSlotInterleaved(currentSlot);
    for (int i = 0; i < CannonCount(); ++i) {
        if (At(slot).IsAlive()) return slot;
        slot = NextSlotInterleaved(slot);
    }
    return currentSlot;
}

int MatchRoster::NextLivingPlayerNum(int currentPlayerNum) const {
    const int slot = std::clamp(currentPlayerNum - 1, 0, std::max(0, CannonCount() - 1));
    return NextLivingSlotInterleaved(slot) + 1;
}

bool MatchRoster::IsPlayerAlive(int playerNum) const {
    const int slot = playerNum - 1;
    if (slot < 0 || slot >= CannonCount()) return false;
    return At(slot).IsAlive();
}

int MatchRoster::LowestHpEnemySlot(int shooterSlot) const {
    int best = -1;
    float bestHp = 1e9f;
    for (int slot : EnemySlots(shooterSlot)) {
        const Cannon& c = At(slot);
        if (!c.IsAlive()) continue;
        if (c.health < bestHp) {
            bestHp = c.health;
            best = slot;
        }
    }
    if (best < 0) {
        const auto enemies = EnemySlots(shooterSlot);
        return enemies.empty() ? shooterSlot : enemies[0];
    }
    return best;
}

bool MatchRoster::IsTeamEliminated(int team) const {
    const int start = (team == 0) ? 0 : perTeamA_;
    const int count = (team == 0) ? perTeamA_ : perTeamB_;
    for (int i = 0; i < count; ++i) {
        if (At(start + i).IsAlive()) return false;
    }
    return true;
}

bool MatchRoster::IsTeamBuried(int team, const Terrain& terrain) const {
    const int start = (team == 0) ? 0 : perTeamA_;
    const int count = (team == 0) ? perTeamA_ : perTeamB_;
    bool anyAlive = false;
    for (int i = 0; i < count; ++i) {
        const Cannon& c = At(start + i);
        if (!c.IsAlive()) continue;
        anyAlive = true;
        if (!terrain.IsFullyGone(c.x)) return false;
    }
    return anyAlive;
}

float MatchRoster::MaxEnemyDistancePx(int team) const {
    float maxDist = 0.0f;
    const int allyStart = (team == 0) ? 0 : perTeamA_;
    const int allyCount = (team == 0) ? perTeamA_ : perTeamB_;
    const int enemyStart = (team == 0) ? perTeamA_ : 0;
    const int enemyCount = (team == 0) ? perTeamB_ : perTeamA_;
    for (int a = 0; a < allyCount; ++a) {
        for (int e = 0; e < enemyCount; ++e) {
            maxDist = std::max(maxDist, std::fabs(At(allyStart + a).x - At(enemyStart + e).x));
        }
    }
    return maxDist;
}

bool MatchRoster::IsHumanSlot(int slot, GameMode mode) const {
    if (mode == GameMode::Online) return true;
    if (mode == GameMode::PvP) {
        if (IsTeamMode(format_)) return slot < perTeamA_;
        return true;
    }
    if (mode == GameMode::PvAI) return slot == 0;
    return false;
}

bool MatchRoster::IsAISlot(int slot, GameMode mode) const {
    if (mode == GameMode::PvAI) {
        if (format_ == MatchFormat::Duel1v1) return slot == 1;
        if (IsTeamMode(format_)) return slot != 0;
    }
    if (mode == GameMode::PvP && IsTeamMode(format_)) return slot >= perTeamA_;
    return false;
}

Color MatchRoster::ColorForSlot(int slot) const {
    static const Color palette[] = {
        { 55, 115, 220, 255 },  // A0 — azul
        { 35, 175, 195, 255 },  // A1 — ciano
        { 80, 200, 120, 255 },  // A2 — verde
        { 155, 75, 195, 255 },  // A3 — roxo
        { 120, 140, 220, 255 }, // A4 — azul claro
        { 215, 65, 55, 255 },   // B0 — vermelho
        { 235, 135, 45, 255 },  // B1 — laranja
        { 195, 55, 135, 255 },  // B2 — magenta
        { 220, 180, 60, 255 },  // B3 — amarelo
        { 180, 100, 70, 255 },  // B4 — marrom
    };
    return palette[static_cast<size_t>(slot) % (sizeof(palette) / sizeof(palette[0]))];
}

Texture2D* MatchRoster::SpriteForSlot(int slot, Texture2D* leftTex, Texture2D* rightTex) const {
    return (TeamOfSlot(slot) == 0) ? leftTex : rightTex;
}

Texture2D* MatchRoster::SpriteForTeamSlot(int slot, Texture2D* cannon1Tex, Texture2D* cannon2Tex,
                                          Texture2D* cannon3Tex, Texture2D* cannon4Tex) const {
    auto pick = [](Texture2D* preferred, Texture2D* fallback) -> Texture2D* {
        return (preferred && preferred->id != 0) ? preferred : fallback;
    };
    const int teamSlot = (TeamOfSlot(slot) == 0) ? slot : (slot - perTeamA_);
    switch (teamSlot % 4) {
        case 0: return pick(cannon1Tex, cannon1Tex);
        case 1: return pick(cannon2Tex, cannon1Tex);
        case 2: return pick(cannon3Tex, cannon1Tex);
        default: return pick(cannon4Tex, cannon1Tex);
    }
}

float MatchRoster::TeamSpacing(int count) const {
    float spacing = cfg::TEAM_CANNON_PAIR_SPACING_PX;
    if (count >= 4) spacing *= 0.88f;
    if (count >= 5) spacing *= 0.78f;
    return spacing;
}

void MatchRoster::PlaceCannons(Terrain& terrain) {
    const float outer = cfg::CANNON_MARGIN_PX;

    for (int t = 0; t < 2; ++t) {
        const int count = (t == 0) ? perTeamA_ : perTeamB_;
        const float spacing = TeamSpacing(count);
        for (int i = 0; i < count; ++i) {
            const int slot = (t == 0) ? i : (perTeamA_ + i);
            float x = 0.0f;
            CannonSide side = (t == 0) ? CannonSide::Left : CannonSide::Right;

            if (count == 1) {
                x = (t == 0) ? outer : (cfg::SCREEN_WIDTH - outer);
            } else if (t == 0) {
                x = outer + static_cast<float>(i) * spacing;
            } else {
                x = cfg::SCREEN_WIDTH - outer - static_cast<float>(count - 1 - i) * spacing;
            }

            const float groundY = terrain.HeightAt(x);
            Cannon& c = cannons_[static_cast<size_t>(slot)];
            c.Init(x, groundY, side);
            c.tintColor = WHITE;
        }
    }
}
