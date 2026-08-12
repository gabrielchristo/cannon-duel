#include "Terrain.h"
#include <cmath>
#include <random>
#include <algorithm>

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
    // Preenche o terreno com triângulos coluna a coluna (rápido e simples).
    Color topColor  = Color{101, 67, 33, 255};
    Color baseColor = Color{60, 40, 20, 255};

    for (int x = 0; x < static_cast<int>(heights.size()) - 1; x += 2) {
        float y0 = heights[x];
        DrawRectangle(x, static_cast<int>(y0), 2,
                      cfg::SCREEN_HEIGHT - static_cast<int>(y0), baseColor);
        DrawRectangle(x, static_cast<int>(y0), 2, 4, topColor); // "grama"
    }
}
