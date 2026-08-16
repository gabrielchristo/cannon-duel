#pragma once

#include <raylib.h>

Vector2 GetVirtualMouse();
void DrawVirtualScreenScaled(const RenderTexture2D& virtualScreen);

#if CANNON_DUEL_WEB_BUILD
void SyncWebCanvasSize();
#endif
