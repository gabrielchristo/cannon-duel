#pragma once
#include <raylib.h>
#include "Localization.h"

enum class PowerupType {
    DoubleDamage,   // 1. dano em dobro no próximo acerto (projétil em chamas)
    TrajectoryPreview, // trajetória prevista por 1 rodada
    Guided,         // 3. próximo tiro teleguiado, dano menor (mais raro)
    Heal,           // 4. cura entre 1/4 e 1/2 da vida total
    Shield,         // 5. escudo por 2 rodadas
    COUNT
};

struct Powerup {
    bool active = false;
    PowerupType type = PowerupType::Heal;
    float x = 0.0f; // a altura Y é sempre recalculada a partir do terreno
};

inline const char* PowerupLabel(PowerupType t) {
    switch (t) {
        case PowerupType::DoubleDamage:     return "2X";
        case PowerupType::TrajectoryPreview:return "TR";
        case PowerupType::Guided:           return "GD";
        case PowerupType::Heal:             return "+";
        case PowerupType::Shield:           return "SH";
        default: return "?";
    }
}

inline const char* PowerupDescription(PowerupType t, Lang lang) {
    switch (t) {
        case PowerupType::DoubleDamage:      return T(TK::PowerupDoubleDamage, lang);
        case PowerupType::TrajectoryPreview: return T(TK::PowerupTrajectory, lang);
        case PowerupType::Guided:            return T(TK::PowerupGuided, lang);
        case PowerupType::Heal:              return T(TK::PowerupHeal, lang);
        case PowerupType::Shield:            return T(TK::PowerupShield, lang);
        default: return "";
    }
}

inline Color PowerupColor(PowerupType t) {
    switch (t) {
        case PowerupType::DoubleDamage:      return Color{220, 60, 40, 255};
        case PowerupType::TrajectoryPreview: return Color{60, 130, 220, 255};
        case PowerupType::Guided:            return Color{150, 70, 200, 255};
        case PowerupType::Heal:              return Color{70, 190, 90, 255};
        case PowerupType::Shield:            return Color{60, 200, 210, 255};
        default: return WHITE;
    }
}
