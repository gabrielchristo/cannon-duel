#include "AI.h"
#include "Config.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

static float RandRange(float lo, float hi) {
    return lo + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

// Aproximação: usa a equação de alcance balístico (sem arrasto) para estimar
// ângulo/potência que atingiriam a distância horizontal até o alvo, depois
// aplica um pequeno offset dependendo da dificuldade e compensa o vento
// "chutando" a potência um pouco mais forte contra o vento contrário.
void AI::ComputeShot(Cannon& me, float targetX, float targetY, float windForce) {
    float dx = std::fabs(targetX - me.x);
    float dy = (targetY - me.groundY); // diferença de altura entre a base do canhão e o alvo

    float distM = cfg::PxToM(dx);
    float g = cfg::GRAVITY_MPS2;

    // Ângulo fixo de 45 graus é ótimo para alcance sem arrasto; ajusta um pouco
    // pela diferença de altura (mais alto se o alvo estiver mais alto).
    float baseAngle = 45.0f - (dy / std::max(1.0f, dx)) * 8.0f;
    baseAngle = std::clamp(baseAngle, 25.0f, 65.0f);

    float rad = baseAngle * DEG2RAD;
    // v^2 = (g * d) / sin(2*theta)  -> resolve para v
    float sin2 = std::sin(2.0f * rad);
    float v = std::sqrt(std::max(0.01f, (g * distM) / std::max(0.15f, sin2)));

    // Compensa vento: quanto mais vento contrário, mais potência.
    float side = (targetX > me.x) ? 1.0f : -1.0f;
    float windAgainst = -windForce * side; // >0 significa vento contra
    v += windAgainst * 0.6f;

    float power01 = std::clamp((v - cfg::MIN_POWER) / (cfg::MAX_POWER - cfg::MIN_POWER), 0.0f, 1.0f);

    // Erro humano: quanto menor 'skill', maior o desvio aleatório.
    float errorSpread = (1.0f - skill) * 12.0f; // graus
    float powerErrorSpread = (1.0f - skill) * 0.12f;

    float finalAngle = baseAngle + RandRange(-errorSpread, errorSpread);
    float finalPower = power01 + RandRange(-powerErrorSpread, powerErrorSpread);

    me.SetAim(finalAngle, std::clamp(finalPower, 0.0f, 1.0f));
}
