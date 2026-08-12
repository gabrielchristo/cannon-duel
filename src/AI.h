#pragma once
#include "Cannon.h"

// IA básica: estima ângulo/potência por tentativa, com um pouco de "erro
// humano" proporcional à dificuldade, e reage ao vento atual.
class AI {
public:
    void SetDifficulty(float skill01) { skill = skill01; } // 0 = fraca, 1 = precisa

    // Calcula mira (grava direto em 'me') mirando num alvo (targetX, targetY)
    // — pode ser o canhão adversário ou, na versão Plus, um power-up no mapa
    // — considerando o vento atual.
    void ComputeShot(Cannon& me, float targetX, float targetY, float windForce);

private:
    float skill = 0.6f;
};
