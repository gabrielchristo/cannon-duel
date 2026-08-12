#pragma once

#include "Cannon.h"
#include "Config.h"

namespace cannon_ui {

// Topo da caixa do nome online (DrawOnlineCannonLabels).
inline float NameLabelTopY(const Cannon& c) {
    return c.groundY - cfg::CANNON_BODY_RADIUS_PX - 52.0f;
}

inline float NameLabelCenterY(const Cannon& c) {
    return c.groundY - cfg::CANNON_BODY_RADIUS_PX - 50.0f;
}

// Barra de força fica acima do nome (multiplayer e local).
inline float PowerBarY(const Cannon& c, float barH = 16.0f, float gap = 8.0f) {
    return NameLabelTopY(c) - gap - barH;
}

} // namespace cannon_ui
