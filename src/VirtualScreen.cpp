#include "VirtualScreen.h"

#include "Config.h"

#include <algorithm>

Vector2 GetVirtualMouse() {
    Vector2 mouse = GetMousePosition();
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float scale = std::min(screenW / cfg::SCREEN_WIDTH, screenH / cfg::SCREEN_HEIGHT);
    float offsetX = (screenW - cfg::SCREEN_WIDTH * scale) * 0.5f;
    float offsetY = (screenH - cfg::SCREEN_HEIGHT * scale) * 0.5f;

    Vector2 v;
    v.x = (mouse.x - offsetX) / scale;
    v.y = (mouse.y - offsetY) / scale;
    v.x = std::clamp(v.x, 0.0f, static_cast<float>(cfg::SCREEN_WIDTH));
    v.y = std::clamp(v.y, 0.0f, static_cast<float>(cfg::SCREEN_HEIGHT));
    return v;
}

void DrawVirtualScreenScaled(const RenderTexture2D& virtualScreen) {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float scale = std::min(screenW / cfg::SCREEN_WIDTH, screenH / cfg::SCREEN_HEIGHT);

    Rectangle src = { 0, 0, static_cast<float>(virtualScreen.texture.width),
                      -static_cast<float>(virtualScreen.texture.height) };
    Rectangle dst = {
        (screenW - cfg::SCREEN_WIDTH * scale) * 0.5f,
        (screenH - cfg::SCREEN_HEIGHT * scale) * 0.5f,
        cfg::SCREEN_WIDTH * scale,
        cfg::SCREEN_HEIGHT * scale
    };
    DrawTexturePro(virtualScreen.texture, src, dst, {0, 0}, 0.0f, WHITE);
}
