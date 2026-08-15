#include "VirtualScreen.h"

#include "Config.h"
#include "Platform.h"

#include <algorithm>
#include <cmath>

#if CANNON_DUEL_WEB_BUILD
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace {

// Cor do céu/menu (GameDraw ClearBackground da textura virtual).
constexpr Color kWebLetterboxColor = { 235, 214, 190, 255 };

struct ViewportMap {
    float scale = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float drawW = 0.0f;
    float drawH = 0.0f;
};

ViewportMap ComputeViewportMap(float screenW, float screenH) {
    ViewportMap m;
    // Contain em todas as plataformas web: jogo inteiro visível; barras bege (não preto).
    m.scale = std::min(screenW / cfg::SCREEN_WIDTH, screenH / cfg::SCREEN_HEIGHT);
    m.drawW = cfg::SCREEN_WIDTH * m.scale;
    m.drawH = cfg::SCREEN_HEIGHT * m.scale;
    m.offsetX = (screenW - m.drawW) * 0.5f;
    m.offsetY = (screenH - m.drawH) * 0.5f;
    return m;
}

} // namespace

Vector2 GetVirtualMouse() {
    Vector2 mouse = GetMousePosition();
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const ViewportMap vp = ComputeViewportMap(screenW, screenH);

    Vector2 v;
    v.x = (mouse.x - vp.offsetX) / vp.scale;
    v.y = (mouse.y - vp.offsetY) / vp.scale;
    v.x = std::clamp(v.x, 0.0f, static_cast<float>(cfg::SCREEN_WIDTH));
    v.y = std::clamp(v.y, 0.0f, static_cast<float>(cfg::SCREEN_HEIGHT));
    return v;
}

void DrawVirtualScreenScaled(const RenderTexture2D& virtualScreen) {
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const ViewportMap vp = ComputeViewportMap(screenW, screenH);

#if CANNON_DUEL_WEB_BUILD
    ClearBackground(kWebLetterboxColor);
#endif

    const Rectangle src = { 0, 0, static_cast<float>(virtualScreen.texture.width),
                            -static_cast<float>(virtualScreen.texture.height) };
    const Rectangle dst = { vp.offsetX, vp.offsetY, vp.drawW, vp.drawH };
    DrawTexturePro(virtualScreen.texture, src, dst, {0, 0}, 0.0f, WHITE);
}

#if CANNON_DUEL_WEB_BUILD
void SyncWebCanvasSize() {
    const int cssW = EM_ASM_INT({
        var vv = window.visualViewport;
        return Math.max(1, (vv ? vv.width : window.innerWidth) | 0);
    });
    const int cssH = EM_ASM_INT({
        var vv = window.visualViewport;
        return Math.max(1, (vv ? vv.height : window.innerHeight) | 0);
    });
    if (cssW <= 0 || cssH <= 0) return;

    // CSS px ≠ px físicos em telas HiDPI. Sem devicePixelRatio o canvas
    // fica em meia resolução e o browser estica — daí o blur na web.
    const double dpr = EM_ASM_DOUBLE({
        var d = window.devicePixelRatio || 1;
        if (d < 1) d = 1;
        if (d > 3) d = 3;
        return d;
    });
    const int fbW = std::max(1, static_cast<int>(std::lround(static_cast<double>(cssW) * dpr)));
    const int fbH = std::max(1, static_cast<int>(std::lround(static_cast<double>(cssH) * dpr)));

    emscripten_set_canvas_element_size("#canvas", fbW, fbH);
    emscripten_set_element_css_size("#canvas", static_cast<double>(cssW), static_cast<double>(cssH));
    if (GetScreenWidth() != fbW || GetScreenHeight() != fbH) {
        SetWindowSize(fbW, fbH);
    }
}
#endif
