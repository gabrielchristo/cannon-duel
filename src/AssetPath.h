#pragma once
#include "Platform.h"
#include <string>

// No desktop, os assets ficam numa pasta "assets/" ao lado do executável,
// então os caminhos usados pelo raylib (e por qualquer leitura via
// LoadFileData) precisam do prefixo "assets/". No Android, o raylib
// carrega arquivos de dentro do APK via AAssetManager, cujos caminhos já
// são relativos à RAIZ da pasta assets/ do APK — incluir o prefixo
// "assets/" de novo faria ele procurar por uma subpasta "assets/" dentro
// de "assets/", que não existe.
//
// IMPORTANTE: isso só vale pra leituras que passam pelas funções do
// próprio raylib (LoadTexture, LoadSound, LoadFileData, etc.) — bibliotecas
// de terceiros como o curl usam fopen() puro, que NÃO enxerga dentro do
// APK no Android. Pra arquivos que o curl precisa ler diretamente (como o
// certificado CA), é preciso extrair o conteúdo pra um caminho gravável de
// verdade primeiro (ver SupabaseClient.cpp).
inline std::string AssetPath(const char* relative) {
#if CANNON_DUEL_ANDROID_BUILD
    return relative;
#else
    return std::string("assets/") + relative;
#endif
}
