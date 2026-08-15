#include "Terrain.h"
#include <cmath>
#include <random>
#include <algorithm>

void Terrain::SetScenario(Scenario scenario) {
    scenario_ = scenario;
}

void Terrain::Generate(unsigned int seed, Scenario scenario) {
    scenario_ = scenario;
    GenerateRandom(seed);
    if (scenario_ == Scenario::ValleyOfTheEnd) {
        ApplyValleyEnvelope();
    }
}

void Terrain::ApplyValleyEnvelope() {
    const int n = static_cast<int>(heights.size());
    if (n < 2) return;

    // Laterais (zonas de spawn) ficam no relevo original. O miolo afunda
    // num U suave — vale jogável sem nascer canhão no fundo.
    for (int x = 0; x < n; ++x) {
        const float u = static_cast<float>(x) / static_cast<float>(n - 1);
        const float dist = std::fabs(u - 0.5f);
        const float t = std::clamp((0.28f - dist) / 0.16f, 0.0f, 1.0f);
        const float dip = t * t * (3.0f - 2.0f * t);
        float elev = static_cast<float>(cfg::SCREEN_HEIGHT) - heights[static_cast<size_t>(x)];
        elev = std::max(cfg::TERRAIN_MIN_HEIGHT * 0.45f, elev - dip * 168.0f);
        heights[static_cast<size_t>(x)] = static_cast<float>(cfg::SCREEN_HEIGHT) - elev;
    }
}

// Midpoint displacement (1D) para gerar um relevo suave e aleatório,
// depois normalizado para os limites min/max configurados.
void Terrain::GenerateRandom(unsigned int seed) {
    int n = cfg::TERRAIN_COLUMNS;
    heights.assign(n, 0.0f);

    // Trabalha em uma potência de 2 +1 pontos e depois reamostra para 'n' colunas.
    int pow2 = 1;
    while (pow2 + 1 < n) pow2 <<= 1;
    std::vector<float> buf(pow2 + 1, 0.0f);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unif(-1.0f, 1.0f);

    buf[0]     = unif(rng);
    buf[pow2]  = unif(rng);

    float roughness = 1.0f;
    int step = pow2;
    while (step > 1) {
        int half = step / 2;
        for (int i = half; i < pow2; i += step) {
            float avg = (buf[i - half] + buf[i + half]) * 0.5f;
            buf[i] = avg + unif(rng) * roughness;
        }
        step = half;
        roughness *= 0.55f; // suaviza a cada nível de detalhe
    }

    // Normaliza para [TERRAIN_MIN_HEIGHT, TERRAIN_MAX_HEIGHT] (altura a partir do chão da tela)
    float minV = *std::min_element(buf.begin(), buf.end());
    float maxV = *std::max_element(buf.begin(), buf.end());
    float range = std::max(0.0001f, maxV - minV);

    for (int x = 0; x < n; ++x) {
        float t = static_cast<float>(x) * pow2 / static_cast<float>(n - 1);
        int i0 = static_cast<int>(t);
        int i1 = std::min(i0 + 1, pow2);
        float frac = t - i0;
        float v = buf[i0] * (1.0f - frac) + buf[i1] * frac;
        float norm = (v - minV) / range; // 0..1

        float elevation = cfg::TERRAIN_MIN_HEIGHT +
                           norm * (cfg::TERRAIN_MAX_HEIGHT - cfg::TERRAIN_MIN_HEIGHT);

        heights[x] = static_cast<float>(cfg::SCREEN_HEIGHT) - elevation;
    }
}

int Terrain::ColumnOf(float worldX) const {
    int c = static_cast<int>(worldX);
    return std::clamp(c, 0, static_cast<int>(heights.size()) - 1);
}

float Terrain::HeightAt(float worldX) const {
    return heights[ColumnOf(worldX)];
}

bool Terrain::IsPointInside(float worldX, float worldY) const {
    return worldY >= HeightAt(worldX);
}

bool Terrain::IsFullyGone(float worldX) const {
    return HeightAt(worldX) >= static_cast<float>(cfg::SCREEN_HEIGHT) - 1.0f;
}

void Terrain::Explode(float worldX, float worldY, float radiusPx) {
    int c0 = ColumnOf(worldX - radiusPx);
    int c1 = ColumnOf(worldX + radiusPx);

    for (int x = c0; x <= c1; ++x) {
        float dx = static_cast<float>(x) - worldX;
        float remaining = radiusPx * radiusPx - dx * dx;
        if (remaining <= 0.0f) continue;

        float depth = std::sqrt(remaining); // formato de "meia-lua" na cratera
        float craterFloor = worldY + depth;
        // nunca deixa o terreno "sumir" para além do fundo da tela — isso é o
        // que permite detectar de forma confiável quando uma coluna foi
        // completamente destruída (ver IsFullyGone).
        craterFloor = std::min(craterFloor, static_cast<float>(cfg::SCREEN_HEIGHT));

        if (craterFloor > heights[x]) {
            heights[x] = craterFloor;
        }
    }

    RebuildPhysicsBody(physWorld);
}

void Terrain::RebuildPhysicsBody(b2WorldId worldId) {
    // Intencionalmente não cria nenhuma shape física de terreno no Box2D.
    // Veja a explicação detalhada no header (Terrain.h). Mantemos o método
    // e o parâmetro só para não quebrar as chamadas existentes (ex: depois
    // de cada explosão), caso no futuro algo realmente precise de um corpo
    // físico de terreno (ex: destroços físicos, veículos rolando etc.).
    (void)worldId;
}

void Terrain::Draw() const {
    // Paleta fixa do terrain_tile.png — mesma em todos os cenários.
    // O relevo continua o midpoint displacement original (só o Vale aplica
    // o envelope no miolo). Não carimbar o tile: isso mudava o visual.
    const Color topColor{ 76, 153, 60, 255 };
    const Color baseColor{ 101, 67, 33, 255 };

    for (int x = 0; x < static_cast<int>(heights.size()) - 1; x += 2) {
        const float y0 = heights[static_cast<size_t>(x)];
        DrawRectangle(x, static_cast<int>(y0), 2,
                      cfg::SCREEN_HEIGHT - static_cast<int>(y0), baseColor);
        DrawRectangle(x, static_cast<int>(y0), 2, 4, topColor);
    }
}
