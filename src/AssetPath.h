#pragma once
#include "Platform.h"
#include <string>

// Caminhos relativos aos assets do jogo (raylib / bundle).
//
// Desktop: pasta "assets/" ao lado do executável.
// Mobile (Android/iOS): raiz do bundle dentro do app — sem prefixo "assets/".
inline std::string AssetPath(const char* relative) {
#if CANNON_DUEL_MOBILE_BUILD
    return relative;
#else
    return std::string("assets/") + relative;
#endif
}
