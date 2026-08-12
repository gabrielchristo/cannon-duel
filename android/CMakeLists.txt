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
# nlohmann/json (header-only) — cliente Supabase (src/net/).
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# curl + mbedTLS — o Android não tem libcurl de sistema como o Linux
# desktop, então compilamos os dois do zero com o toolchain do NDK. Usamos
# mbedTLS como backend de TLS (em vez de OpenSSL/BoringSSL) porque o build
# dele via CMake cross-compila de primeira com o NDK — o do OpenSSL usa um
# sistema de build próprio (Perl/Configure), e o do BoringSSL usa
# Bazel/GN — nenhum dos dois se dá bem com toolchain files do CMake sem
# ferramental extra. Trocar de backend de TLS não evitaria o problema
# abaixo, então ficamos com mbedTLS mesmo.
#
# AVISO: esta é a parte menos testada de todo o pipeline Android — não
# tive como validar isso rodando de verdade neste ambiente. Se der erro de
# build aqui, é o primeiro lugar a investigar.
# ---------------------------------------------------------------------------
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(mbedtls)

set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE STRING "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
# O curl, por padrão, tenta gerar um export() de CURL::libcurl (pra permitir
# find_package(CURL) direto na árvore de build, sem instalar nada) — isso
# falha porque exige que TODAS as dependências linkadas (inclusive o
# mbedTLS que linkamos manualmente mais abaixo) também estejam num "export
# set", o que não é o nosso caso. Essa opção desliga exatamente esse
# export(), feita pelo próprio curl para cenários como o nosso (consumo via
# FetchContent, sem instalação).
set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)

# Desliga tudo que a gente não usa (só faz GET/POST/PATCH/DELETE simples de
# JSON pro Supabase) — reduz tempo de build e evita que o CMake do curl
# tente auto-detectar bibliotecas opcionais que não existem no sysroot do
# NDK (foi exatamente isso que quebrou com libidn2: o curl encontrou
# alguma pista de que ela existia — provavelmente cache antigo de uma
# tentativa anterior — e tentou usar sem o header estar realmente lá).
# CMAKE_DISABLE_FIND_PACKAGE_<Nome> força qualquer find_package(<Nome>)
# chamado internamente pelo curl a sempre reportar "não encontrado",
# independente de cache ou de qualquer coisa parecida existir no sistema.
set(CURL_DISABLE_IDN ON CACHE BOOL "" FORCE)
# ATENÇÃO: "CURL_DISABLE_IDN" não é a variável que realmente controla isso
# no CMake do curl (deixamos ela por precaução, mas sozinha não bastou).
# O nome real é USE_LIBIDN2 — e pior, o curl detecta o libidn2 via
# pkg-config (pkg_check_modules), não via find_package() do CMake, então
# CMAKE_DISABLE_FIND_PACKAGE_Libidn2 (abaixo) não pega esse caso sozinho.
# Foi por isso que a primeira tentativa não resolveu.
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_Libidn2 ON CACHE BOOL "" FORCE)

set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)          # HTTP/2 — não precisamos
set(CMAKE_DISABLE_FIND_PACKAGE_NGHTTP2 ON CACHE BOOL "" FORCE)

set(CURL_BROTLI OFF CACHE BOOL "" FORCE)          # compressão Brotli
set(CMAKE_DISABLE_FIND_PACKAGE_Brotli ON CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)            # compressão Zstd
set(CMAKE_DISABLE_FIND_PACKAGE_Zstd ON CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB ON CACHE BOOL "" FORCE)

set(ENABLE_ARES OFF CACHE BOOL "" FORCE)          # DNS assíncrono (c-ares)
set(CMAKE_DISABLE_FIND_PACKAGE_Cares ON CACHE BOOL "" FORCE)

set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)      # public suffix list
set(CMAKE_DISABLE_FIND_PACKAGE_Libpsl ON CACHE BOOL "" FORCE)

set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)     # SCP/SFTP — já desligado via CMAKE_USE_LIBSSH2, redundante mas garante
set(CMAKE_DISABLE_FIND_PACKAGE_LibSSH2 ON CACHE BOOL "" FORCE)

set(CURL_USE_GSASL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBUV OFF CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL ON CACHE BOOL "" FORCE) # garante que não tenta OpenSSL por engano (usamos mbedTLS)
set(ENABLE_WEBSOCKETS ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_WEBSOCKETS OFF CACHE BOOL "" FORCE)

# O CMakeLists.txt do próprio curl chama find_package(MbedTLS REQUIRED)
# internamente pra decidir se o mbedTLS está disponível — esse é um "Find
# module" clássico baseado em variável, que procura bibliotecas JÁ
# INSTALADAS no sistema, não os alvos que o FetchContent acima criou em
# memória. Pré-preencher as variáveis nem sempre é suficiente (o próprio
# find_package pode ignorá-las e procurar de novo), então a forma confiável
# é neutralizar essa chamada diretamente no código-fonte baixado do curl
# (via patch automático) e linkar os alvos reais do mbedTLS manualmente
# logo depois — assim nunca dependemos do find_package funcionar.
FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_9_1
    GIT_SHALLOW    TRUE
    PATCH_COMMAND sed -i "s/find_package(MbedTLS REQUIRED.*/# &/" CMakeLists.txt
)
FetchContent_MakeAvailable(curl)

if(TARGET libcurl)
    # Versões recentes do curl fazem de "libcurl" um alvo ALIAS apontando
    # pro alvo real (ex: "libcurl_static"), dependendo de BUILD_SHARED_LIBS.
    # Não dá pra chamar target_link_libraries/target_include_directories
    # diretamente num ALIAS — resolvemos pro alvo real primeiro, de forma
    # genérica (sem fixar um nome específico que pode mudar entre versões
    # do curl).
    get_target_property(_libcurl_aliased libcurl ALIASED_TARGET)
    if(_libcurl_aliased)
        set(_libcurl_real_target ${_libcurl_aliased})
    else()
        set(_libcurl_real_target libcurl)
    endif()
    target_link_libraries(${_libcurl_real_target} PRIVATE mbedtls mbedx509 mbedcrypto)
    target_include_directories(${_libcurl_real_target} PRIVATE ${mbedtls_SOURCE_DIR}/include)
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

set(CANNON_DUEL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
include(${CANNON_DUEL_ROOT}/cmake/EmbedCaCert.cmake)

set(CANNON_DUEL_TARGET CannonDuel)
include(${CANNON_DUEL_ROOT}/cmake/DebugMode.cmake)

add_library(CannonDuel SHARED
    ${CANNON_DUEL_SOURCES}
    ${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
)

cannon_duel_apply_debug_mode(CannonDuel)

cannon_duel_embed_cacert(CannonDuel "${CANNON_DUEL_ROOT}")

target_include_directories(CannonDuel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${NATIVE_APP_GLUE_DIR}
    ${RAYLIB_INCLUDE_DIR}
)

# libraylib.a foi compilado separadamente pelo Makefile oficial do raylib
# (ver build_android.sh) — aqui só apontamos pra ele como uma lib
# estática "importada" (pré-compilada), sem o CMake tentar reconstruí-la.
add_library(raylib_prebuilt STATIC IMPORTED)
set_target_properties(raylib_prebuilt PROPERTIES IMPORTED_LOCATION ${RAYLIB_A})

target_link_libraries(CannonDuel PRIVATE
    raylib_prebuilt
    box2d
    libcurl
    nlohmann_json::nlohmann_json
    log android EGL GLESv2 OpenSLES
)

# Garante que o símbolo de entrada da NativeActivity não seja removido pelo
# linker por "não ser referenciado" (ele é chamado via JNI, não por código
# C/C++ visível estaticamente).
target_link_options(CannonDuel PRIVATE "-u" "ANativeActivity_onCreate")

# Intercepta logcat/printf e manda pro overlay (DebugLog) em vez do terminal.
if(CANNON_DUEL_DEBUG_MODE)
    target_link_options(CannonDuel PRIVATE
        "-Wl,--wrap,__android_log_print"
        "-Wl,--wrap,__android_log_vprint"
        "-Wl,--wrap,printf"
        "-Wl,--wrap,vprintf"
        "-Wl,--wrap,fprintf"
    )
endif()
