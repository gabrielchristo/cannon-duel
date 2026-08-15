#pragma once
#include "Config.h"
#include <raylib.h>

// Layout único do menu principal — título, toolbar (idioma + Classic/Plus) e botões.
namespace menu_layout {

inline constexpr float kTitleY = 48.0f;
inline constexpr int kTitleFontSize = 64;

inline constexpr float kFlagW = 64.0f;
inline constexpr float kFlagH = 44.0f;
inline constexpr float kFlagY = 16.0f;
inline constexpr float kFlagGap = 16.0f;

inline constexpr float kVersionToggleW = 280.0f;
inline constexpr float kVersionToggleH = 44.0f;
inline constexpr float kToolbarGap = 24.0f;

inline constexpr float kBtnW = 280.0f;
inline constexpr float kBtnH = 56.0f;
inline constexpr float kBtnGap = 18.0f;
inline constexpr int kMainMenuBtnCount = 6;

inline Rectangle BrazilFlagRect() {
    const float x = cfg::SCREEN_WIDTH - kFlagW * 2.0f - kFlagGap - 20.0f;
    return { x, kFlagY, kFlagW, kFlagH };
}

inline Rectangle UsaFlagRect() {
    const float x = cfg::SCREEN_WIDTH - kFlagW - 20.0f;
    return { x, kFlagY, kFlagW, kFlagH };
}

inline float VersionToggleY() {
    return kTitleY + static_cast<float>(kTitleFontSize) + kToolbarGap;
}

inline Rectangle VersionToggleRect() {
    return {
        cfg::SCREEN_WIDTH / 2.0f - kVersionToggleW / 2.0f,
        VersionToggleY(),
        kVersionToggleW,
        kVersionToggleH
    };
}

inline float MainMenuButtonsTopY() {
    return VersionToggleY() + kVersionToggleH + kToolbarGap;
}

inline Rectangle MainMenuButtonRect(int index) {
    const float top = MainMenuButtonsTopY();
    const float y = top + index * (kBtnH + kBtnGap);
    return { cfg::SCREEN_WIDTH / 2.0f - kBtnW / 2.0f, y, kBtnW, kBtnH };
}

inline float MainMenuButtonsBottomY() {
    return MainMenuButtonsTopY()
        + kMainMenuBtnCount * kBtnH
        + (kMainMenuBtnCount - 1) * kBtnGap;
}

} // namespace menu_layout
