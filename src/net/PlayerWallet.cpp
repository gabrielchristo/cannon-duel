#include "PlayerWallet.h"
#include "JsonHelpers.h"
#include "NetValidation.h"
#include "../DebugLog.h"
#include "../ShopCatalog.h"

#include <raylib.h>
#include <fstream>
#include <sstream>

#if CANNON_DUEL_WEB_BUILD
#include "WebOpfs.h"
#endif

using nlohmann::json;

std::string PlayerWallet::CachePath() {
#if CANNON_DUEL_ANDROID_BUILD
    return std::string(GetWorkingDirectory()) + "/wallet_cache.txt";
#elif CANNON_DUEL_WEB_BUILD
    return "/opfs/wallet_cache.txt";
#else
    return "wallet_cache.txt";
#endif
}

void PlayerWallet::Init(PlayerIdentity* identity) {
    identity_ = identity;
    equippedCannonColor_ = kDefaultCannonColorId;
    equippedCannonSkin_ = kDefaultCannonSkinId;
    equippedCannonEffect_ = kDefaultCannonEffectId;
    equippedNameEffect_ = kDefaultNameEffectId;
    ownedItems_.insert(kDefaultCannonColorId);
    ownedItems_.insert(kDefaultCannonSkinId);
    ownedItems_.insert(kDefaultCannonEffectId);
    ownedItems_.insert(kDefaultNameEffectId);
    coins_ = kStartingCoins;
    LoadDisplayCache();
    if (coins_ <= 0) coins_ = kStartingCoins;
}

void PlayerWallet::LoadDisplayCache() {
    const std::string path = CachePath();
    if (!FileExists(path.c_str())) return;
    char* raw = LoadFileText(path.c_str());
    if (!raw) return;

    std::istringstream in(raw);
    std::string line;
    if (std::getline(in, line)) {
        try { coins_ = std::stoi(line); } catch (...) { coins_ = kStartingCoins; }
    }
    if (std::getline(in, line) && !line.empty()) equippedCannonColor_ = line;
    if (std::getline(in, line) && !line.empty()) equippedCannonSkin_ = line;
    if (std::getline(in, line) && !line.empty()) equippedCannonEffect_ = line;
    if (std::getline(in, line) && !line.empty()) equippedNameEffect_ = line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) ownedItems_.insert(line);
    }
    UnloadFileText(raw);
}

void PlayerWallet::SaveDisplayCache() const {
    const std::string path = CachePath();
    std::string body = std::to_string(coins_) + "\n"
        + equippedCannonColor_ + "\n"
        + equippedCannonSkin_ + "\n"
        + equippedCannonEffect_ + "\n"
        + equippedNameEffect_ + "\n";
    for (const auto& item : ownedItems_) body += item + "\n";
    if (!SaveFileText(path.c_str(), body.data())) {
        std::ofstream out(path);
        out << body;
    }
}

bool PlayerWallet::Owns(const std::string& itemId) const {
    return ownedItems_.count(itemId) > 0;
}

void PlayerWallet::EnsurePlayerRow() {
    if (!identity_ || !net_validation::IsValidPlayerUuid(identity_->Id())) return;
    json body = {
        { "id", identity_->Id() },
        { "display_name", identity_->DisplayName() }
    };
    client_.Upsert("players", body, "id");
}

int PlayerWallet::FetchServerCoins() {
    if (!identity_) return coins_;
    json rows = client_.Select("players", "select=coins&id=eq." + identity_->Id());
    if (client_.LastRequestOk() && rows.is_array() && !rows.empty()) {
        return json_helpers::Int(rows[0], "coins", kStartingCoins);
    }
    return coins_;
}

void PlayerWallet::RefreshFromServer() {
    if (!identity_) return;
    EnsurePlayerRow();

    json rows = client_.Select("players",
        "select=coins,equipped_cannon_color,equipped_cannon_skin,equipped_cannon_effect,equipped_name_effect&id=eq."
        + identity_->Id());
    if (client_.LastRequestOk() && rows.is_array() && !rows.empty()) {
        coins_ = json_helpers::Int(rows[0], "coins", coins_);
        if (coins_ <= 0) {
            json grant = { { "coins", kStartingCoins } };
            client_.Update("players", "id=eq." + identity_->Id(), grant);
            if (client_.LastRequestOk()) coins_ = kStartingCoins;
        }
        equippedCannonColor_ = json_helpers::Str(rows[0], "equipped_cannon_color", equippedCannonColor_);
        equippedCannonSkin_ = json_helpers::Str(rows[0], "equipped_cannon_skin", equippedCannonSkin_);
        equippedCannonEffect_ = json_helpers::Str(rows[0], "equipped_cannon_effect", equippedCannonEffect_);
        equippedNameEffect_ = json_helpers::Str(rows[0], "equipped_name_effect", equippedNameEffect_);
    } else {
        DebugLogf(LOG_WARNING, "WALLET: falha ao buscar saldo no servidor — mantendo cache local");
    }

    json items = client_.Select("player_items", "select=item_id&player_id=eq." + identity_->Id());
    if (client_.LastRequestOk() && items.is_array()) {
        ownedItems_.clear();
        ownedItems_.insert(kDefaultCannonColorId);
        ownedItems_.insert(kDefaultCannonSkinId);
        ownedItems_.insert(kDefaultCannonEffectId);
        ownedItems_.insert(kDefaultNameEffectId);
        for (const auto& row : items) {
            ownedItems_.insert(json_helpers::Str(row, "item_id", ""));
        }
    }

    SaveDisplayCache();
}

void PlayerWallet::AwardCoins(int amount) {
    if (!identity_ || amount == 0) return;
    EnsurePlayerRow();
    const int current = FetchServerCoins();
    const int updated = current + amount;

    json body = { { "coins", updated } };
    client_.Update("players", "id=eq." + identity_->Id(), body);
    if (client_.LastRequestOk()) {
        coins_ = updated;
        SaveDisplayCache();
    } else {
        DebugLogf(LOG_WARNING, "WALLET: falha ao creditar %d moedas", amount);
    }
}

bool PlayerWallet::TryPurchase(const std::string& itemId) {
    if (!identity_) return false;
    const ShopItem* item = FindShopItem(itemId);
    if (!item) return false;
    if (Owns(itemId)) return true;

    EnsurePlayerRow();
    const int current = FetchServerCoins();
    if (current < item->priceCoins) return false;

    json purchase = {
        { "player_id", identity_->Id() },
        { "item_id", itemId }
    };
    client_.Insert("player_items", purchase);
    if (!client_.LastRequestOk()) {
        DebugLogf(LOG_WARNING, "WALLET: falha ao registrar compra de %s", itemId.c_str());
        return false;
    }

    const int updated = current - item->priceCoins;
    json body = { { "coins", updated } };
    client_.Update("players", "id=eq." + identity_->Id(), body);
    if (!client_.LastRequestOk()) {
        DebugLogf(LOG_WARNING, "WALLET: item %s comprado mas falha ao debitar moedas", itemId.c_str());
        return false;
    }

    coins_ = updated;
    ownedItems_.insert(itemId);
    SaveDisplayCache();
    return true;
}

bool PlayerWallet::Equip(const std::string& itemId) {
    if (!identity_) return false;
    const ShopItem* item = FindShopItem(itemId);
    if (!item || !Owns(itemId)) return false;

    const char* column = EquippedColumnForCategory(item->category);
    if (!column) return false;

    json body = { { column, itemId } };
    client_.Update("players", "id=eq." + identity_->Id(), body);
    if (!client_.LastRequestOk()) {
        DebugLogf(LOG_WARNING, "WALLET: falha ao equipar %s", itemId.c_str());
        return false;
    }

    switch (item->category) {
        case ShopCategory::CannonColor: equippedCannonColor_ = itemId; break;
        case ShopCategory::CannonSkin: equippedCannonSkin_ = itemId; break;
        case ShopCategory::CannonEffect: equippedCannonEffect_ = itemId; break;
        case ShopCategory::NameEffect: equippedNameEffect_ = itemId; break;
    }
    SaveDisplayCache();
    return true;
}
