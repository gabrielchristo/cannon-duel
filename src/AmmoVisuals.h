#pragma once
#include <raylib.h>
#include "ShopCatalog.h"

class ParticleSystem;

void EmitAmmoTrail(ParticleSystem& particles, Vector2 pos, Vector2 vel,
                   AmmoStyle ammo, bool powerupDouble, bool powerupGuided);
void EmitAmmoImpact(ParticleSystem& particles, Vector2 pos, AmmoStyle ammo);
void DrawAmmoProjectile(Vector2 pos, Vector2 vel, AmmoStyle ammo,
                        const Texture2D* sprite, bool powerupDouble, bool powerupGuided,
                        float visualScale = 1.2f);
void DrawAmmoShopPreview(Vector2 center, AmmoStyle ammo, const Texture2D* sprite);
