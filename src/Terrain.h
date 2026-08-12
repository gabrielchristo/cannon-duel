#pragma once
#include <vector>
#include <box2d/box2d.h>
#include <raylib.h>
#include "Config.h"

// Terreno representado como heightmap 1D (uma altura por coluna de pixel).
// A colisão é resolvida por comparação direta com o heightmap (raycast 1D),
// muito mais barata e simples de deformar em tempo real do que manter um
// b2Body poligonal complexo sendo reconstruído a cada explosão.
class Terrain {
public:
    void GenerateRandom(unsigned int seed);

    // Retorna a altura do terreno (em px, medida a partir do topo da tela)
    // na coluna X informada.
    float HeightAt(float worldX) const;

    // true se o ponto (x,y) em px está abaixo da superfície do terreno.
    bool IsPointInside(float worldX, float worldY) const;

    // true se o terreno na coluna X já foi completamente destruído (chegou
    // ao fundo da tela) — usado para checar se um canhão "sumiu" de vez.
    bool IsFullyGone(float worldX) const;

    // Cava uma cratera circular centrada em (worldX, worldY) com o raio dado.
    void Explode(float worldX, float worldY, float radiusPx);

    void Draw() const;

    // Corpo estático "vazio", mantido apenas por compatibilidade de assinatura
    // (rebuildado a cada explosão). O terreno NÃO tem colisão física real no
    // Box2D — de propósito: os canhões não são corpos físicos (posição
    // deles já vem direto do heightmap) e o projétil também usa o heightmap
    // diretamente para detectar impacto (ver Terrain::IsPointInside). Se o
    // Box2D tivesse uma chain shape física aqui, ele pararia o projétil
    // fisicamente ANTES do nosso check de heightmap disparar, deixando o
    // projétil "grudado" no terreno sem nunca contar como impacto — travando
    // o turno indefinidamente. Por isso não criamos nenhuma shape no corpo.
    void RebuildPhysicsBody(b2WorldId worldId);

private:
    std::vector<float> heights; // heights[col] = altura da superfície (px do topo)
    b2BodyId groundBody = b2_nullBodyId;
    b2WorldId physWorld = b2_nullWorldId;

    int ColumnOf(float worldX) const;
};
