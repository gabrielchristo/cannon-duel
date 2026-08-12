cmake_minimum_required(VERSION 3.16)
project(CannonDuel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# NOTA: este CMakeLists.txt é usado apenas para a build DESKTOP (Linux/macOS/
# Windows). A build Android usa um pipeline separado (ver android/CMakeLists.txt
# + build_android.sh) porque o CMakeLists.txt genérico do próprio
# raylib tem suporte a Android historicamente instável — o caminho oficial e
# testado pra Android é o Makefile dedicado dele (src/Makefile), não este
# CMakeLists.txt. Ver ANDROID.md para detalhes.

include(FetchContent)

# ---------------------------------------------------------------------------
# raylib
# ---------------------------------------------------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# box2d (v3.x - API em C, usada pelo nosso PhysicsWorld wrapper)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# nlohmann/json (header-only) — usado pelo cliente Supabase (src/net/),
# que agora compila em ambas as plataformas (desktop e Android).
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(raylib box2d nlohmann_json)

# libcurl 8.x (FetchContent) — HTTP + WebSocket (Supabase Realtime).
# Evita depender do curl antigo do sistema (< 7.86 sem curl_ws_*).
include(${CMAKE_SOURCE_DIR}/cmake/CurlWithWebSocket.cmake)

# O box2d define -Werror internamente no seu CMakeLists (dependendo da
# versão/toolchain) e a 3.1.0 dispara falsos-positivos de
# "maybe-uninitialized" com certas versões do GCC em código que não é
# nosso. A forma confiável de neutralizar isso é injetar -Wno-error DEPOIS,
# diretamente no alvo já criado (isso vence qualquer -Werror herdado de
# CMAKE_C_FLAGS ou setado pelo próprio CMakeLists do box2d).
if(TARGET box2d)
    target_compile_options(box2d PRIVATE -Wno-error)
endif()

# ---------------------------------------------------------------------------
# Executável
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CANNON_DUEL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

set(CANNON_DUEL_TARGET CannonDuel)
include(${CMAKE_SOURCE_DIR}/cmake/DebugMode.cmake)

add_executable(CannonDuel ${CANNON_DUEL_SOURCES})

cannon_duel_apply_debug_mode(CannonDuel)

target_include_directories(CannonDuel PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(CannonDuel PRIVATE
    raylib box2d nlohmann_json::nlohmann_json
    libcurl OpenSSL::SSL OpenSSL::Crypto
    pthread
)

if(APPLE)
    target_link_libraries(CannonDuel PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

if(WIN32)
    target_link_libraries(CannonDuel PRIVATE winmm)
endif()

# Copia assets para a pasta de build
add_custom_command(TARGET CannonDuel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:CannonDuel>/assets
)
