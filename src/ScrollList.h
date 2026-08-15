#pragma once

#include <raylib.h>

#include <algorithm>

struct ScrollListState {
    float scrollY = 0.0f;
    bool dragging = false;
    float dragStartY = 0.0f;
    float dragStartScroll = 0.0f;
};

struct ScrollListLayout {
    Rectangle viewport{};
    float rowHeight = 74.0f;
    int itemCount = 0;
    float scrollBarW = 14.0f;
};

inline float ScrollListContentHeight(const ScrollListLayout& layout) {
    return layout.rowHeight * static_cast<float>(std::max(0, layout.itemCount));
}

inline float ScrollListMaxScroll(const ScrollListLayout& layout) {
    return std::max(0.0f, ScrollListContentHeight(layout) - layout.viewport.height);
}

inline Rectangle ScrollListTrack(const ScrollListLayout& layout) {
    return {
        layout.viewport.x + layout.viewport.width - layout.scrollBarW - 4.0f,
        layout.viewport.y,
        layout.scrollBarW,
        layout.viewport.height
    };
}

inline Rectangle ScrollListRowRect(const ScrollListLayout& layout, const ScrollListState& state, int index) {
    return {
        layout.viewport.x,
        layout.viewport.y + static_cast<float>(index) * layout.rowHeight - state.scrollY,
        layout.viewport.width - layout.scrollBarW - 8.0f,
        layout.rowHeight
    };
}

inline bool ScrollListRowVisible(const ScrollListLayout& layout, const Rectangle& row) {
    return row.y + row.height >= layout.viewport.y
        && row.y <= layout.viewport.y + layout.viewport.height;
}

inline bool ScrollListPointInViewport(const ScrollListLayout& layout, Vector2 p) {
    return CheckCollisionPointRec(p, layout.viewport);
}

inline void UpdateScrollList(ScrollListState& state, const ScrollListLayout& layout, Vector2 mouse) {
    const float maxScroll = ScrollListMaxScroll(layout);
    state.scrollY = std::clamp(state.scrollY, 0.0f, maxScroll);

    const Rectangle track = ScrollListTrack(layout);
    const bool overViewport = CheckCollisionPointRec(mouse, layout.viewport);
    const bool overScroll = CheckCollisionPointRec(mouse, track);

    if (overViewport) {
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            state.scrollY = std::clamp(state.scrollY - wheel * layout.rowHeight * 0.75f, 0.0f, maxScroll);
        }
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && overScroll && maxScroll > 0.0f) {
        state.dragging = true;
        state.dragStartY = mouse.y;
        state.dragStartScroll = state.scrollY;
    }
    if (state.dragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        const float thumbH = std::max(24.0f, track.height * (layout.viewport.height / ScrollListContentHeight(layout)));
        const float thumbTravel = std::max(1.0f, track.height - thumbH);
        const float deltaY = mouse.y - state.dragStartY;
        state.scrollY = std::clamp(
            state.dragStartScroll + (deltaY / thumbTravel) * maxScroll, 0.0f, maxScroll);
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state.dragging = false;
    }
}

// Roda + arraste no viewport, sem depender da barra.
inline void UpdateInvisibleScroll(ScrollListState& state, const ScrollListLayout& layout, Vector2 mouse) {
    const float maxScroll = ScrollListMaxScroll(layout);
    state.scrollY = std::clamp(state.scrollY, 0.0f, maxScroll);

    const bool overViewport = CheckCollisionPointRec(mouse, layout.viewport);
    if (overViewport) {
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            state.scrollY = std::clamp(state.scrollY - wheel * layout.rowHeight, 0.0f, maxScroll);
        }
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && overViewport && maxScroll > 0.0f) {
        state.dragging = true;
        state.dragStartY = mouse.y;
        state.dragStartScroll = state.scrollY;
    }
    if (state.dragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        const float deltaY = mouse.y - state.dragStartY;
        state.scrollY = std::clamp(state.dragStartScroll - deltaY, 0.0f, maxScroll);
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state.dragging = false;
    }
}

inline void DrawScrollListBar(const ScrollListState& state, const ScrollListLayout& layout) {
    const float maxScroll = ScrollListMaxScroll(layout);
    if (maxScroll <= 0.0f || layout.itemCount <= 0) return;

    const Rectangle track = ScrollListTrack(layout);
    DrawRectangleRec(track, Color{180, 165, 145, 200});
    DrawRectangleLinesEx(track, 1, Color{100, 85, 65, 200});

    const float contentH = ScrollListContentHeight(layout);
    const float thumbH = std::max(24.0f, track.height * (layout.viewport.height / contentH));
    const float thumbTravel = std::max(1.0f, track.height - thumbH);
    const float thumbY = track.y + (state.scrollY / maxScroll) * thumbTravel;
    const Rectangle thumb = { track.x + 2.0f, thumbY, track.width - 4.0f, thumbH };
    DrawRectangleRec(thumb, Color{120, 100, 80, 255});
}
