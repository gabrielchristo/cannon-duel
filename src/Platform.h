#pragma once

// Detecção de plataforma em tempo de compilação.
// Usado para isolar diferenças entre desktop, Android e (futuro) iOS.
//
// Android: NDK define __ANDROID__ automaticamente.
// iOS:     __APPLE__ + TARGET_OS_IPHONE (quando existir build iOS).
// Desktop: tudo que não for mobile (Linux, macOS, Windows).

#if defined(__ANDROID__)
    #define CANNON_DUEL_ANDROID_BUILD 1
#else
    #define CANNON_DUEL_ANDROID_BUILD 0
#endif

#if defined(__EMSCRIPTEN__)
    #define CANNON_DUEL_WEB_BUILD 1
#else
    #define CANNON_DUEL_WEB_BUILD 0
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define CANNON_DUEL_IOS_BUILD 1
    #else
        #define CANNON_DUEL_IOS_BUILD 0
    #endif
#else
    #define CANNON_DUEL_IOS_BUILD 0
#endif

#define CANNON_DUEL_MOBILE_BUILD (CANNON_DUEL_ANDROID_BUILD || CANNON_DUEL_IOS_BUILD)
// AssetPath.h só distingue mobile (bundle na raiz) de tudo mais (pasta
// "assets/" ao lado do binário/preloaded no FS virtual) — web cai em
// "tudo mais" de propósito, já que os assets são pré-carregados em
// "assets/..." no MEMFS, igual ao desktop.
#define CANNON_DUEL_DESKTOP_BUILD (!CANNON_DUEL_MOBILE_BUILD && !CANNON_DUEL_WEB_BUILD)

// Definido pelo CMake (cmake/EmbedCaCert.cmake) quando cacert.pem é
// embutido no binário via xxd -i — obrigatório em mobile (curl sem CAs
// do sistema); opcional no desktop.
#if !defined(CANNON_DUEL_HAS_EMBEDDED_CA)
    #define CANNON_DUEL_HAS_EMBEDDED_CA 0
#endif

// Definido pelo CMake (cmake/DebugMode.cmake) — padrão ON por enquanto.
#if !defined(CANNON_DUEL_DEBUG_MODE)
    #define CANNON_DUEL_DEBUG_MODE 0
#endif
