cmake_minimum_required(VERSION 3.16)
project(CannonDuel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

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

FetchContent_MakeAvailable(raylib box2d)

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

add_executable(CannonDuel ${CANNON_DUEL_SOURCES})

target_include_directories(CannonDuel PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(CannonDuel PRIVATE raylib box2d)

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
