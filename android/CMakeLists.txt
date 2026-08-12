cmake_minimum_required(VERSION 3.16)
project(CannonDuelAndroid CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED ANDROID_NDK)
    message(FATAL_ERROR
        "Configure com -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake")
endif()

if(NOT DEFINED RAYLIB_A)
    message(FATAL_ERROR "Passe -DRAYLIB_A=/caminho/para/libraylib.a (gerado pelo Makefile oficial do raylib)")
endif()
if(NOT DEFINED RAYLIB_INCLUDE_DIR)
    message(FATAL_ERROR "Passe -DRAYLIB_INCLUDE_DIR=/caminho/para/raylib/src (onde está raylib.h)")
endif()

# ---------------------------------------------------------------------------
# box2d (v3.x) — compilado normalmente via CMake, cross-compilando com o
# toolchain do NDK. Ao contrário do raylib, o Box2D não tem nenhuma
# dependência de janela/GLFW, então compila sem os problemas que o
# CMakeLists.txt genérico do raylib tem para Android.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(box2d)

if(TARGET box2d)
    target_compile_options(box2d PRIVATE -Wno-error)
endif()

# ---------------------------------------------------------------------------
# Nosso código + android_native_app_glue (do próprio NDK) → libCannonDuel.so
# ---------------------------------------------------------------------------
set(NATIVE_APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
if(NOT EXISTS ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c)
    message(FATAL_ERROR "android_native_app_glue.c não encontrado em ${NATIVE_APP_GLUE_DIR}")
endif()

file(GLOB_RECURSE CANNON_DUEL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp
)

add_library(CannonDuel SHARED
    ${CANNON_DUEL_SOURCES}
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
)

target_include_directories(CannonDuel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${NATIVE_APP_GLUE_DIR}
    ${RAYLIB_INCLUDE_DIR}
)

# libraylib.a foi compilado separadamente pelo Makefile oficial do raylib
# (ver android/build_android.sh) — aqui só apontamos pra ele como uma lib
# estática "importada" (pré-compilada), sem o CMake tentar reconstruí-la.
add_library(raylib_prebuilt STATIC IMPORTED)
set_target_properties(raylib_prebuilt PROPERTIES IMPORTED_LOCATION ${RAYLIB_A})

target_link_libraries(CannonDuel PRIVATE
    raylib_prebuilt
    box2d
    log android EGL GLESv2 OpenSLES
)

# Garante que o símbolo de entrada da NativeActivity não seja removido pelo
# linker por "não ser referenciado" (ele é chamado via JNI, não por código
# C/C++ visível estaticamente).
target_link_options(CannonDuel PRIVATE "-u" "ANativeActivity_onCreate")
