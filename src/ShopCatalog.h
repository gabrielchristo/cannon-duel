#pragma once
#include <raylib.h>
#include <string>
#include "Localization.h"

// Catálogo estático da loja — puramente cosmético (sem dano/defesa).
enum class ShopCategory { CannonColor, CannonSkin, CannonEffect, NameEffect };

// Efeitos de canhão — partículas + shader líquido nos tiers premium.
enum class CannonEffectStyle {
    None, AuraSoft, AuraFire, ImbueHoly, DebuffGlow, ArcaneSpark,
    LiquidInferno, LiquidFrost, LiquidVoid, AuraCunt
};
enum class NameEffectStyle { Plain, Flame, DarkSmoke, PurpleGlow, Cunt, OceanWave, Sakura };

struct ShopItem {
    const char* id;
    ShopCategory category;
    int priceCoins;
    TK nameKey;
    Color primary = WHITE;
    Color accent = WHITE;
    int colorIndex = 0;        // 1–11 → sprites/cannon_N.png
    int skinOverlayIndex = 0;  // 1–4 → sprites/skin_*.png
    CannonEffectStyle cannonEffect = CannonEffectStyle::None;
    NameEffectStyle nameEffect = NameEffectStyle::Plain;
};

inline constexpr const char* kDefaultCannonColorId = "color_default";
inline constexpr const char* kDefaultCannonSkinId = "skin_default";
inline constexpr const char* kDefaultCannonEffectId = "effect_default";
inline constexpr const char* kDefaultNameEffectId = "name_default";

inline constexpr int kCannonColorPrice = 25;

inline constexpr ShopItem kShopCatalog[] = {
    // --- Cor ---
    { kDefaultCannonColorId, ShopCategory::CannonColor, 0,
      TK::ShopItemDefaultColor, WHITE, WHITE },
    { "color_blue", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorBlue, WHITE, Color{55, 115, 220, 255}, 1 },
    { "color_cyan", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorCyan, WHITE, Color{35, 175, 195, 255}, 2 },
    { "color_green", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorGreen, WHITE, Color{80, 200, 120, 255}, 3 },
    { "color_purple", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorPurple, WHITE, Color{155, 75, 195, 255}, 4 },
    { "color_red", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorRed, WHITE, Color{215, 65, 55, 255}, 6 },
    { "color_orange", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorOrange, WHITE, Color{235, 135, 45, 255}, 7 },
    { "color_gold", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorGold, WHITE, Color{220, 180, 60, 255}, 9 },
    { "color_black", ShopCategory::CannonColor, kCannonColorPrice,
      TK::ShopColorBlack, WHITE, Color{45, 48, 55, 255}, 11 },

    // --- Skin gráfica (overlay sobre a cor equipada) ---
    { kDefaultCannonSkinId, ShopCategory::CannonSkin, 0,
      TK::ShopItemDefaultSkin, WHITE, WHITE },
    { "skin_kuromi", ShopCategory::CannonSkin, 55,
      TK::ShopSkinKuromi, Color{30, 25, 35, 255}, Color{255, 140, 200, 255}, 0, 1 },
    { "skin_gothic", ShopCategory::CannonSkin, 55,
      TK::ShopSkinGothic, Color{40, 35, 50, 255}, Color{180, 160, 200, 255}, 0, 2 },
    { "skin_samurai", ShopCategory::CannonSkin, 70,
      TK::ShopSkinSamurai, Color{120, 30, 30, 255}, Color{220, 180, 70, 255}, 0, 3 },
    { "skin_pirate", ShopCategory::CannonSkin, 80,
      TK::ShopSkinPirate, Color{120, 30, 35, 255}, Color{230, 190, 60, 255}, 0, 4 },

    // --- Efeito de canhão (simples → premium) ---
    { kDefaultCannonEffectId, ShopCategory::CannonEffect, 0,
      TK::ShopItemDefaultEffect, WHITE, WHITE },
    { "effect_aura_soft", ShopCategory::CannonEffect, 35,
      TK::ShopEffectAuraSoft, WHITE, Color{180, 210, 255, 255}, 0, 0, CannonEffectStyle::AuraSoft },
    { "effect_aura_fire", ShopCategory::CannonEffect, 45,
      TK::ShopEffectAuraFire, WHITE, Color{255, 120, 50, 255}, 0, 0, CannonEffectStyle::AuraFire },
    { "effect_debuff_glow", ShopCategory::CannonEffect, 50,
      TK::ShopEffectDebuffGlow, WHITE, Color{120, 255, 160, 255}, 0, 0, CannonEffectStyle::DebuffGlow },
    { "effect_imbue_holy", ShopCategory::CannonEffect, 60,
      TK::ShopEffectImbueHoly, WHITE, Color{255, 230, 140, 255}, 0, 0, CannonEffectStyle::ImbueHoly },
    { "effect_arcane", ShopCategory::CannonEffect, 75,
      TK::ShopEffectArcane, WHITE, Color{170, 90, 255, 255}, 0, 0, CannonEffectStyle::ArcaneSpark },
    { "effect_aura_cunt", ShopCategory::CannonEffect, 80,
      TK::ShopEffectAuraCunt, WHITE, Color{255, 90, 180, 255}, 0, 0, CannonEffectStyle::AuraCunt },
    { "effect_liquid_frost", ShopCategory::CannonEffect, 85,
      TK::ShopEffectLiquidFrost, WHITE, Color{140, 210, 255, 255}, 0, 0, CannonEffectStyle::LiquidFrost },
    { "effect_liquid_inferno", ShopCategory::CannonEffect, 90,
      TK::ShopEffectLiquidInferno, WHITE, Color{255, 90, 25, 255}, 0, 0, CannonEffectStyle::LiquidInferno },
    { "effect_liquid_void", ShopCategory::CannonEffect, 100,
      TK::ShopEffectLiquidVoid, WHITE, Color{90, 40, 160, 255}, 0, 0, CannonEffectStyle::LiquidVoid },

    // --- Efeito de nome (simples → premium) ---
    { kDefaultNameEffectId, ShopCategory::NameEffect, 0,
      TK::ShopItemDefaultName, WHITE, WHITE, 0, 0, CannonEffectStyle::None, NameEffectStyle::Plain },
    { "name_ember", ShopCategory::NameEffect, 20,
      TK::ShopItemEmberName, Color{255, 140, 60, 255}, Color{255, 210, 90, 255},
      0, 0, CannonEffectStyle::None, NameEffectStyle::Flame },
    { "name_shadow", ShopCategory::NameEffect, 30,
      TK::ShopItemShadowName, Color{80, 80, 90, 255}, Color{20, 20, 25, 255},
      0, 0, CannonEffectStyle::None, NameEffectStyle::DarkSmoke },
    { "name_amethyst", ShopCategory::NameEffect, 45,
      TK::ShopItemAmethystName, Color{170, 90, 230, 255}, Color{230, 190, 255, 255},
      0, 0, CannonEffectStyle::None, NameEffectStyle::PurpleGlow },
    { "name_ocean", ShopCategory::NameEffect, 60,
      TK::ShopItemOceanName, Color{40, 160, 220, 255}, Color{160, 235, 255, 255},
      0, 0, CannonEffectStyle::None, NameEffectStyle::OceanWave },
    { "name_sakura", ShopCategory::NameEffect, 70,
      TK::ShopItemSakuraName, Color{255, 130, 170, 255}, Color{255, 210, 225, 255},
      0, 0, CannonEffectStyle::None, NameEffectStyle::Sakura },
    { "name_cunt", ShopCategory::NameEffect, 85,
      TK::ShopItemCuntName, Color{255, 60, 120, 255}, Color{80, 160, 255, 255},
      0, 0, CannonEffectStyle::None, NameEffectStyle::Cunt },
};

inline constexpr int kShopCatalogCount = sizeof(kShopCatalog) / sizeof(kShopCatalog[0]);

inline const ShopItem* FindShopItem(const std::string& id) {
    const std::string resolved = (id == "name_radiant") ? "name_cunt" : id;
    for (const auto& item : kShopCatalog) {
        if (resolved == item.id) return &item;
    }
    return nullptr;
}

inline const char* EquippedColumnForCategory(ShopCategory cat) {
    switch (cat) {
        case ShopCategory::CannonColor: return "equipped_cannon_color";
        case ShopCategory::CannonSkin: return "equipped_cannon_skin";
        case ShopCategory::CannonEffect: return "equipped_cannon_effect";
        case ShopCategory::NameEffect: return "equipped_name_effect";
    }
    return nullptr;
}
