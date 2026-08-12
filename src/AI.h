#pragma once
#include "Cannon.h"

// IA básica: estima ângulo/potência por tentativa, com um pouco de "erro
// humano" proporcional à dificuldade, e reage ao vento atual.
class AI {
public:
    void SetDifficulty(float skill01) { skill = skill01; } // 0 = fraca, 1 = precisa

    // Calcula mira (grava direto em 'me') mirando no alvo, considerando o vento.
    void ComputeShot(Cannon& me, const Cannon& target, float windForce);

private:
    float skill = 0.6f;
};
