#pragma once

#include <raylib.h>
#include <vector>

// Label flutuante "+X" mostrado acima do canhão que ganhou moedas (acerto,
// power-up). Sobe devagar a maior parte da vida e, perto do fim, acelera a
// subida e o fade — "some rápido da tela".
class CoinPopupSystem {
public:
    void Spawn(Vector2 pos, int amount);
    void Update(float dt);
    void Draw() const;

private:
    struct Popup {
        Vector2 pos;
        int amount;
        float age = 0.0f;
    };

    static constexpr float kLifetime = 1.1f;
    static constexpr float kFastPhaseStart = 0.75f; // fração da vida onde acelera/some

    std::vector<Popup> popups_;
};
