#include "Game.h"
#include "Config.h"
#include "ShopCatalog.h"
#include "Cannon.h"
#include "CosmeticShaders.h"
#include "ScrollList.h"
#include "VirtualScreen.h"

#include <cstdio>
#include <cmath>
#include <vector>

namespace {

constexpr int kColumns = 3;
constexpr float kCardW = 260.0f;
constexpr float kCardH = 200.0f;
constexpr float kGapX = 30.0f;
constexpr float kGapY = 26.0f;
constexpr float kGridTop = 190.0f;
constexpr int kTabCount = 4;

struct ShopGridLayout {
    ScrollListLayout scroll{};
    int itemCount = 0;
};

std::vector<const ShopItem*> ItemsInCategory(ShopCategory cat) {
    std::vector<const ShopItem*> out;
    for (const auto& item : kShopCatalog) {
        if (item.category == cat) out.push_back(&item);
    }
    return out;
}

Rectangle ShopBackBtnRect() {
    return { cfg::SCREEN_WIDTH / 2.0f - 100, cfg::SCREEN_HEIGHT - 64.0f, 200, 48 };
}

Rectangle ShopTabRect(int tabIndex) {
    constexpr float tabW = 130.0f;
    constexpr float tabGap = 8.0f;
    float totalW = kTabCount * tabW + (kTabCount - 1) * tabGap;
    float startX = cfg::SCREEN_WIDTH / 2.0f - totalW / 2.0f;
    return { startX + tabIndex * (tabW + tabGap), 128, tabW, 40 };
}

ShopGridLayout BuildShopGridLayout(int itemCount) {
    ShopGridLayout grid{};
    const float bottom = ShopBackBtnRect().y - 12.0f;
    grid.scroll.viewport = { 24.0f, kGridTop, cfg::SCREEN_WIDTH - 48.0f, bottom - kGridTop };
    grid.scroll.rowHeight = kCardH + kGapY;
    grid.scroll.itemCount = (itemCount + kColumns - 1) / kColumns;
    grid.itemCount = itemCount;
    return grid;
}

Rectangle CardRect(const ShopGridLayout& grid, int index, float scrollY) {
    int col = index % kColumns;
    int row = index / kColumns;
    float gridW = kColumns * kCardW + (kColumns - 1) * kGapX;
    float startX = cfg::SCREEN_WIDTH / 2.0f - gridW / 2.0f;
    const float y = kGridTop + row * (kCardH + kGapY) - scrollY;
    return { startX + col * (kCardW + kGapX), y, kCardW, kCardH };
}

bool CardVisible(const ShopGridLayout& grid, const Rectangle& card) {
    return card.y + card.height >= grid.scroll.viewport.y
        && card.y <= grid.scroll.viewport.y + grid.scroll.viewport.height;
}

bool IsEquipped(const PlayerWallet& wallet, const ShopItem* item) {
    switch (item->category) {
        case ShopCategory::CannonColor: return wallet.EquippedCannonColor() == item->id;
        case ShopCategory::CannonSkin: return wallet.EquippedCannonSkin() == item->id;
        case ShopCategory::CannonEffect: return wallet.EquippedCannonEffect() == item->id;
        case ShopCategory::NameEffect: return wallet.EquippedNameEffect() == item->id;
    }
    return false;
}

void DrawCannonPreview(Vector2 center, Texture2D& baseTex, Texture2D* overlayTex,
                       CannonEffectStyle effect, Color effectPrimary, Color effectAccent) {
    const float scale = 56.0f / baseTex.width;
    const Vector2 origin = { baseTex.width * scale * 0.5f, baseTex.height * scale * 0.62f };
    const Rectangle src = { 0, 0, static_cast<float>(baseTex.width), static_cast<float>(baseTex.height) };
    const Rectangle dst = { center.x, center.y, baseTex.width * scale, baseTex.height * scale };
    const float effectR = 28.0f;
    if (effect != CannonEffectStyle::None) {
        gCosmeticShaders.DrawCannonEnergyField(center, effectR, effectPrimary, effectAccent, effect);
        if (effect == CannonEffectStyle::Kyuubi) {
            DrawCannonCosmeticEffect(center, effectR, effectPrimary, effectAccent, effect);
        }
        gCosmeticShaders.DrawCannonOutline(baseTex, src, dst, origin, effect, effectAccent);
        gCosmeticShaders.DrawCannonCoating(baseTex, src, dst, origin, effect, effectPrimary, effectAccent);
    }
    DrawTexturePro(baseTex, src, dst, origin, 0.0f, WHITE);
    if (overlayTex && overlayTex->id != 0) {
        DrawTexturePro(*overlayTex, src, dst, origin, 0.0f, WHITE);
    }
    if (effect != CannonEffectStyle::None && effect != CannonEffectStyle::Kyuubi) {
        DrawCannonCosmeticEffect(center, effectR, effectPrimary, effectAccent, effect);
    }
}

void DrawNameEffectPreview(const char* sampleText, Rectangle card, NameEffectStyle style,
                           Color primary, Color accent) {
    constexpr int pfs = 22;
    const int ptw = MeasureText(sampleText, pfs);
    const int px = static_cast<int>(card.x + card.width / 2 - ptw / 2);
    const int py = static_cast<int>(card.y + 48);
    gCosmeticShaders.DrawStyledName(sampleText, px, py, pfs, style, primary, accent);
}

} // namespace

void Game::DrawShopItemCard(const ShopItem* item, Rectangle card, Vector2 mouse) const {
    const bool owned = wallet.Owns(item->id);
    const bool equipped = owned && IsEquipped(wallet, item);
    const bool affordable = wallet.Coins() >= item->priceCoins;

    DrawRectangleRec(card, Color{250, 240, 222, 255});
    DrawRectangleLinesEx(card, 2, equipped ? Color{60, 160, 70, 255} : Color{60, 40, 20, 255});

    Vector2 previewCenter = { card.x + card.width / 2.0f, card.y + 62.0f };

    if (item->category == ShopCategory::NameEffect) {
        const std::string& playerName = playerIdentity.DisplayName();
        const char* sampleText = !playerName.empty()
            ? playerName.c_str()
            : T(TK::ShopNamePreviewSample, language);
        DrawNameEffectPreview(sampleText, card, item->nameEffect, item->primary, item->accent);
    } else {
        Texture2D* baseTex = const_cast<Texture2D*>(&texCannonLeft);
        if (item->category == ShopCategory::CannonColor && item->colorIndex > 0
            && item->colorIndex <= static_cast<int>(texCannonColors.size())
            && texCannonColors[static_cast<size_t>(item->colorIndex - 1)].id != 0) {
            baseTex = const_cast<Texture2D*>(&texCannonColors[static_cast<size_t>(item->colorIndex - 1)]);
        } else if (texCannonLeft.id == 0) {
            baseTex = const_cast<Texture2D*>(&texCannonRight);
        }

        Texture2D* overlayTex = nullptr;
        if (item->category == ShopCategory::CannonSkin && item->skinOverlayIndex > 0
            && item->skinOverlayIndex <= static_cast<int>(texCannonOverlays.size())) {
            overlayTex = const_cast<Texture2D*>(&texCannonOverlays[static_cast<size_t>(item->skinOverlayIndex - 1)]);
        }

        CannonEffectStyle previewEffect = CannonEffectStyle::None;
        Color previewPrimary = WHITE;
        Color previewAccent = WHITE;
        if (item->category == ShopCategory::CannonEffect) {
            previewEffect = item->cannonEffect;
            previewPrimary = item->primary;
            previewAccent = item->accent;
        }

        if (baseTex && baseTex->id != 0) {
            DrawCannonPreview(previewCenter, *baseTex, overlayTex,
                              previewEffect, previewPrimary, previewAccent);
        }
    }

    const char* name = T(item->nameKey, language);
    int nfs = 17;
    int ntw = MeasureText(name, nfs);
    DrawText(name, static_cast<int>(card.x + card.width / 2 - ntw / 2),
             static_cast<int>(card.y + 104), nfs, Color{40, 30, 20, 255});

    Rectangle actionBtn = { card.x + 20, card.y + card.height - 46, card.width - 40, 34 };
    if (equipped) {
        DrawRectangleRec(actionBtn, Color{190, 225, 190, 255});
        DrawRectangleLinesEx(actionBtn, 2, Color{60, 160, 70, 255});
        const char* lbl = T(TK::ShopEquippedLabel, language);
        int lw = MeasureText(lbl, 18);
        DrawText(lbl, static_cast<int>(actionBtn.x + actionBtn.width / 2 - lw / 2),
                 static_cast<int>(actionBtn.y + 8), 18, Color{30, 90, 40, 255});
    } else if (owned) {
        bool hover = CheckCollisionPointRec(mouse, actionBtn);
        DrawRectangleRec(actionBtn, hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
        DrawRectangleLinesEx(actionBtn, 2, Color{60, 40, 20, 255});
        const char* lbl = T(TK::ShopEquipButton, language);
        int lw = MeasureText(lbl, 18);
        DrawText(lbl, static_cast<int>(actionBtn.x + actionBtn.width / 2 - lw / 2),
                 static_cast<int>(actionBtn.y + 8), 18, Color{40, 25, 10, 255});
    } else {
        bool hover = affordable && CheckCollisionPointRec(mouse, actionBtn);
        Color fill = !affordable ? Color{190, 180, 170, 255} : (hover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
        DrawRectangleRec(actionBtn, fill);
        DrawRectangleLinesEx(actionBtn, 2, Color{60, 40, 20, 255});
        char priceBuf[48];
        std::snprintf(priceBuf, sizeof(priceBuf), "%s (%d)", T(TK::ShopBuyButton, language), item->priceCoins);
        int lw = MeasureText(priceBuf, 16);
        DrawText(priceBuf, static_cast<int>(actionBtn.x + actionBtn.width / 2 - lw / 2),
                 static_cast<int>(actionBtn.y + 9), 16,
                 affordable ? Color{40, 25, 10, 255} : Color{110, 100, 95, 255});
    }
}

void Game::UpdateShop() {
    Vector2 m = ::GetVirtualMouse();
    auto items = ItemsInCategory(shopCategory);
    ShopGridLayout grid = BuildShopGridLayout(static_cast<int>(items.size()));
    UpdateScrollList(shopScroll_, grid.scroll, m);

    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;

    if (CheckCollisionPointRec(m, ShopBackBtnRect())) {
        state = GameState::MainMenu;
        return;
    }

    const ShopCategory tabs[kTabCount] = {
        ShopCategory::CannonColor, ShopCategory::CannonSkin,
        ShopCategory::CannonEffect, ShopCategory::NameEffect
    };
    for (int t = 0; t < kTabCount; ++t) {
        if (CheckCollisionPointRec(m, ShopTabRect(t))) {
            if (shopCategory != tabs[t]) {
                shopCategory = tabs[t];
                shopScroll_.scrollY = 0.0f;
            }
            return;
        }
    }

    if (!ScrollListPointInViewport(grid.scroll, m)) return;

    for (size_t i = 0; i < items.size(); ++i) {
        Rectangle card = CardRect(grid, static_cast<int>(i), shopScroll_.scrollY);
        if (!CardVisible(grid, card)) continue;

        Rectangle actionBtn = { card.x + 20, card.y + card.height - 46, card.width - 40, 34 };
        if (!CheckCollisionPointRec(m, actionBtn)) continue;

        const ShopItem* item = items[i];
        const bool owned = wallet.Owns(item->id);
        const bool equipped = owned && IsEquipped(wallet, item);
        if (equipped) continue;
        if (owned) {
            wallet.Equip(item->id);
        } else if (wallet.Coins() >= item->priceCoins) {
            wallet.TryPurchase(item->id);
        }
        break;
    }
}

void Game::DrawShop() {
    ClearBackground(Color{ 235, 214, 190, 255 });

    const char* title = T(TK::ShopTitle, language);
    int fs = 44;
    int tw = MeasureText(title, fs);
    DrawText(title, cfg::SCREEN_WIDTH / 2 - tw / 2, 36, fs, Color{40, 30, 20, 255});

    char coinsBuf[64];
    std::snprintf(coinsBuf, sizeof(coinsBuf), "%s: %d", T(TK::ShopCoinsLabel, language), wallet.Coins());
    int cfs = 22;
    int ctw = MeasureText(coinsBuf, cfs);
    DrawText(coinsBuf, cfg::SCREEN_WIDTH - ctw - 30, 40, cfs, Color{150, 110, 20, 255});

    Vector2 m = ::GetVirtualMouse();

    const ShopCategory tabs[kTabCount] = {
        ShopCategory::CannonColor, ShopCategory::CannonSkin,
        ShopCategory::CannonEffect, ShopCategory::NameEffect
    };
    const TK tabKeys[kTabCount] = {
        TK::ShopTabColors, TK::ShopTabSkins, TK::ShopTabEffects, TK::ShopTabNames
    };
    for (int t = 0; t < kTabCount; ++t) {
        Rectangle r = ShopTabRect(t);
        bool active = (shopCategory == tabs[t]);
        bool hover = CheckCollisionPointRec(m, r);
        Color fill = active ? Color{200, 150, 70, 255} : (hover ? Color{225, 200, 170, 255} : Color{215, 190, 160, 255});
        DrawRectangleRec(r, fill);
        DrawRectangleLinesEx(r, 2, Color{60, 40, 20, 255});
        const char* label = T(tabKeys[t], language);
        int lfs = 18;
        int ltw = MeasureText(label, lfs);
        DrawText(label, static_cast<int>(r.x + r.width / 2 - ltw / 2),
                 static_cast<int>(r.y + r.height / 2 - lfs / 2), lfs, Color{40, 25, 10, 255});
    }

    auto items = ItemsInCategory(shopCategory);
    ShopGridLayout grid = BuildShopGridLayout(static_cast<int>(items.size()));

    DrawRectangleRec(grid.scroll.viewport, Fade(Color{225, 205, 180, 255}, 0.35f));
    DrawRectangleLinesEx(grid.scroll.viewport, 1, Color{120, 95, 70, 180});

    BeginScissorMode(static_cast<int>(grid.scroll.viewport.x),
                     static_cast<int>(grid.scroll.viewport.y),
                     static_cast<int>(grid.scroll.viewport.width),
                     static_cast<int>(grid.scroll.viewport.height));
    for (size_t i = 0; i < items.size(); ++i) {
        Rectangle card = CardRect(grid, static_cast<int>(i), shopScroll_.scrollY);
        if (!CardVisible(grid, card)) continue;
        DrawShopItemCard(items[i], card, m);
    }
    EndScissorMode();

    DrawScrollListBar(shopScroll_, grid.scroll);

    bool backHover = CheckCollisionPointRec(m, ShopBackBtnRect());
    Rectangle backBtn = ShopBackBtnRect();
    DrawRectangleRec(backBtn, backHover ? Color{230, 180, 90, 255} : Color{200, 150, 70, 255});
    DrawRectangleLinesEx(backBtn, 2, Color{60, 40, 20, 255});
    const char* backLbl = T(TK::ShopBack, language);
    int blw = MeasureText(backLbl, 22);
    DrawText(backLbl, static_cast<int>(backBtn.x + backBtn.width / 2 - blw / 2),
             static_cast<int>(backBtn.y + backBtn.height / 2 - 11), 22, Color{40, 25, 10, 255});
}
