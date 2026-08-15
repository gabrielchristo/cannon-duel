#pragma once
#include "../Platform.h"
#include "PlayerIdentity.h"
#include "SupabaseClient.h"

#include <set>
#include <string>

class PlayerWallet {
public:
    void Init(PlayerIdentity* identity);

    int Coins() const { return coins_; }
    bool Owns(const std::string& itemId) const;
    const std::string& EquippedCannonColor() const { return equippedCannonColor_; }
    const std::string& EquippedCannonSkin() const { return equippedCannonSkin_; }
    const std::string& EquippedCannonEffect() const { return equippedCannonEffect_; }
    const std::string& EquippedNameEffect() const { return equippedNameEffect_; }

    void RefreshFromServer();
    void AwardCoins(int amount);
    bool TryPurchase(const std::string& itemId);
    bool Equip(const std::string& itemId);

private:
    PlayerIdentity* identity_ = nullptr;
    SupabaseClient client_;

    int coins_ = 0;
    std::set<std::string> ownedItems_;
    std::string equippedCannonColor_;
    std::string equippedCannonSkin_;
    std::string equippedCannonEffect_;
    std::string equippedNameEffect_;

    static std::string CachePath();
    void LoadDisplayCache();
    void SaveDisplayCache() const;

    void EnsurePlayerRow();
    int FetchServerCoins();
};
