#pragma once
#include "Config.h"
#include "ScrollList.h"
#include <algorithm>
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

inline constexpr int kInfoTitleFont = 48;
inline constexpr int kInfoBodyFont = 26;
inline constexpr float kInfoLineH = 38.0f;
inline constexpr float kInfoTitleY = 36.0f;
inline constexpr float kInfoViewportTop = 108.0f;

inline Rectangle InfoPageBackBtn() {
    return { cfg::SCREEN_WIDTH / 2.0f - 100.0f, cfg::SCREEN_HEIGHT - 80.0f, 200.0f, 52.0f };
}

inline ScrollListLayout InfoPageLayout(int lineCount) {
    const Rectangle back = InfoPageBackBtn();
    ScrollListLayout layout;
    layout.viewport = {
        32.0f,
        kInfoViewportTop,
        static_cast<float>(cfg::SCREEN_WIDTH) - 64.0f,
        back.y - 16.0f - kInfoViewportTop
    };
    layout.rowHeight = kInfoLineH;
    layout.itemCount = std::max(0, lineCount);
    layout.scrollBarW = 0.0f;
    return layout;
}

} // namespace menu_layout
