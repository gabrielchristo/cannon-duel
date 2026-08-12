#pragma once

// O NDK do Android já define __ANDROID__ automaticamente para qualquer
// código compilado com o toolchain dele — não precisamos configurar nada
// extra no CMake para isso funcionar. Usamos essa macro para isolar, em
// tempo de compilação, funcionalidades que só fazem sentido em desktop
// (painel de desenvolvedor, atalhos de teclado para o 2º jogador) das que
// valem para ambas as plataformas. O multiplayer online (src/net/) NÃO é
// mais restrito por essa macro — compila e funciona nas duas plataformas.
#if defined(__ANDROID__)
    #define CANNON_DUEL_ANDROID_BUILD 1
#else
    #define CANNON_DUEL_ANDROID_BUILD 0
#endif
