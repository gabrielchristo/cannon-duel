#!/usr/bin/env bash
# Uso: ./build_android.sh
# Compila android/build/ — o modo (debug/release) vem do último ./run_cmake_android.sh.
#
# Saída: android/build/CannonDuel.apk (assinado, pronto pra instalar via adb)
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
ANDROID_DIR="$PROJECT_ROOT/android"
BUILD_DIR="$ANDROID_DIR/build"

ABI="arm64-v8a"
RAYLIB_ARCH="arm64"
API_LEVEL=21
RAYLIB_TAG="5.5"
BUILD_TOOLS_VERSION="30.0.3"
PLATFORM_VERSION="android-30"

require_android_env() {
    : "${ANDROID_HOME:?defina ANDROID_HOME antes de rodar este script}"
    : "${ANDROID_NDK_HOME:?defina ANDROID_NDK_HOME antes de rodar este script}"

    case "$ANDROID_NDK_HOME" in
        "$ANDROID_HOME"/*) ;;
        *)
            echo "ERRO: ANDROID_NDK_HOME (\"$ANDROID_NDK_HOME\") não está dentro de ANDROID_HOME (\"$ANDROID_HOME\")." >&2
            echo "       Rode de novo:" >&2
            echo "         export ANDROID_HOME=$ANDROID_HOME" >&2
            echo "         export ANDROID_NDK_HOME=\$ANDROID_HOME/ndk/<versão>" >&2
            exit 1
            ;;
    esac

    CLANG_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API_LEVEL}-clang"
    if [ ! -x "$CLANG_BIN" ]; then
        echo "ERRO: não encontrei o compilador do NDK em:" >&2
        echo "  $CLANG_BIN" >&2
        exit 1
    fi

    BUILD_TOOLS="$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION"
    ANDROID_JAR="$ANDROID_HOME/platforms/$PLATFORM_VERSION/android.jar"

    for tool in "$BUILD_TOOLS/aapt" "$BUILD_TOOLS/zipalign" "$BUILD_TOOLS/apksigner"; do
        if [ ! -x "$tool" ]; then
            echo "ERRO: não encontrei $tool — confira BUILD_TOOLS_VERSION em build_android.sh." >&2
            exit 1
        fi
    done
    if [ ! -f "$ANDROID_JAR" ]; then
        echo "ERRO: não encontrei $ANDROID_JAR — confira PLATFORM_VERSION em build_android.sh." >&2
        exit 1
    fi
}

ensure_cacert() {
    local cacert="$PROJECT_ROOT/assets/certs/cacert.pem"
    if [ ! -s "$cacert" ]; then
        echo "==> assets/certs/cacert.pem ausente — baixando de curl.se..."
        mkdir -p "$(dirname "$cacert")"
        curl -fsSL -o "$cacert" https://curl.se/ca/cacert.pem
    fi
    if [ ! -s "$cacert" ]; then
        echo "ERRO: assets/certs/cacert.pem não existe ou está vazio." >&2
        exit 1
    fi
}

build_raylib_if_needed() {
    local raylib_src="$BUILD_DIR/raylib-src"
    if [ ! -d "$raylib_src" ]; then
        echo "==> Baixando código-fonte do raylib ($RAYLIB_TAG)"
        git clone --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib.git "$raylib_src"
    fi

    RAYLIB_A="$raylib_src/src/libraylib.a"
    if [ -f "$RAYLIB_A" ]; then
        return 0
    fi

    echo "==> Compilando libraylib.a (Makefile oficial, ANDROID_ARCH=$RAYLIB_ARCH, API=$API_LEVEL)"
    make -C "$raylib_src/src" clean >/dev/null 2>&1 || true
    make -C "$raylib_src/src" \
        PLATFORM=PLATFORM_ANDROID \
        ANDROID_NDK="$ANDROID_NDK_HOME" \
        ANDROID_ARCH="$RAYLIB_ARCH" \
        ANDROID_API_VERSION="$API_LEVEL"

    if [ ! -f "$RAYLIB_A" ]; then
        echo "ERRO: $RAYLIB_A não foi gerado — confira o log de build do raylib acima." >&2
        exit 1
    fi
}

read_build_type() {
    BUILD_TYPE=""
    if [ -f "$BUILD_DIR/.build-type" ]; then
        BUILD_TYPE="$(tr -d '[:space:]' < "$BUILD_DIR/.build-type")"
    fi
}

require_build_type() {
    read_build_type
    if [ "$BUILD_TYPE" != "debug" ] && [ "$BUILD_TYPE" != "release" ]; then
        echo "Build Android não configurada — rode: ./run_cmake_android.sh debug|release" >&2
        exit 1
    fi
}

android_configure_cmake() {
    local build_type="$1"
    local cmake_build_type="Debug"
    local debug_mode="ON"
    if [ "$build_type" = "release" ]; then
        cmake_build_type="Release"
        debug_mode="OFF"
    fi

    require_android_env
    mkdir -p "$BUILD_DIR"
    ensure_cacert
    build_raylib_if_needed

    local cmake_build_dir="$BUILD_DIR/cmake-$ABI"
    echo "==> Configurando CMake (ANDROID_ABI=$ABI, CMAKE_BUILD_TYPE=$cmake_build_type, CANNON_DUEL_DEBUG_MODE=$debug_mode)"
    cmake -S "$ANDROID_DIR" -B "$cmake_build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$API_LEVEL" \
        -DANDROID_STL=c++_static \
        -DRAYLIB_A="$RAYLIB_A" \
        -DRAYLIB_INCLUDE_DIR="$BUILD_DIR/raylib-src/src" \
        -DCANNON_DUEL_DEBUG_MODE="$debug_mode" \
        -DCMAKE_BUILD_TYPE="$cmake_build_type"

    echo "$build_type" > "$BUILD_DIR/.build-type"
    echo "Configuração gravada em $BUILD_DIR/.build-type ($build_type)"
}

android_build_apk() {
    require_android_env
    require_build_type
    mkdir -p "$BUILD_DIR"
    ensure_cacert
    build_raylib_if_needed

    local cmake_build_dir="$BUILD_DIR/cmake-$ABI"
    if [ ! -f "$cmake_build_dir/CMakeCache.txt" ]; then
        echo "CMake não configurado — rode: ./run_cmake_android.sh $BUILD_TYPE" >&2
        exit 1
    fi

    echo "==> 1/4: Compilando Box2D + jogo (CMake + NDK, ABI=$ABI, modo=$BUILD_TYPE)"
    cmake --build "$cmake_build_dir" -j

    local so_file="$cmake_build_dir/libCannonDuel.so"
    if [ ! -f "$so_file" ]; then
        echo "ERRO: $so_file não foi gerado — confira o log de build acima." >&2
        exit 1
    fi

    echo "==> 2/4: Montando a árvore do APK"
    local apk_root="$BUILD_DIR/apk_root"
    rm -rf "$apk_root"
    mkdir -p "$apk_root/lib/$ABI"
    cp "$so_file" "$apk_root/lib/$ABI/"
    cp -r "$PROJECT_ROOT/assets" "$apk_root/assets"

    echo "==> 3/4: Empacotando com aapt (não assinado, não alinhado)"
    local unaligned_apk="$BUILD_DIR/CannonDuel.unaligned.apk"
    "$BUILD_TOOLS/aapt" package -f -F "$unaligned_apk" \
        -M "$ANDROID_DIR/AndroidManifest.xml" \
        -S "$ANDROID_DIR/res" \
        -I "$ANDROID_JAR" \
        -A "$apk_root/assets"
    (cd "$apk_root" && "$BUILD_TOOLS/aapt" add "$unaligned_apk" "lib/$ABI/libCannonDuel.so")

    echo "==> 4/4: Alinhando e assinando (modo $BUILD_TYPE)"
    local aligned_apk="$BUILD_DIR/CannonDuel.aligned.apk"
    "$BUILD_TOOLS/zipalign" -f 4 "$unaligned_apk" "$aligned_apk"

    local keystore="$ANDROID_DIR/cannon-duel.keystore"
    local keystore_props="$ANDROID_DIR/keystore.properties"
    if [ ! -f "$keystore" ] || [ ! -f "$keystore_props" ]; then
        echo "ERRO: falta a chave de assinatura (android/cannon-duel.keystore + android/keystore.properties)." >&2
        echo "      Debug e release usam a mesma chave; sem ela o Android exige desinstalar o app." >&2
        echo "      Restaure o backup da chave — não gere outra se o app já estiver instalado." >&2
        exit 1
    fi

    local store_password="" key_alias="" key_password=""
    local k v
    while IFS='=' read -r k v; do
        case "$k" in
            storePassword) store_password="$v" ;;
            keyAlias)      key_alias="$v" ;;
            keyPassword)   key_password="$v" ;;
        esac
    done < "$keystore_props"
    if [ -z "$store_password" ] || [ -z "$key_alias" ] || [ -z "$key_password" ]; then
        echo "ERRO: android/keystore.properties incompleto (storePassword, keyAlias, keyPassword)." >&2
        exit 1
    fi

    local final_apk="$BUILD_DIR/CannonDuel.apk"
    "$BUILD_TOOLS/apksigner" sign \
        --ks "$keystore" \
        --ks-key-alias "$key_alias" \
        --ks-pass "pass:$store_password" \
        --key-pass "pass:$key_password" \
        --out "$final_apk" "$aligned_apk"

    echo ""
    echo "APK pronto: $final_apk ($BUILD_TYPE)"
    echo "Pra instalar num celular/emulador conectado:"
    echo "  \$ANDROID_HOME/platform-tools/adb install -r \"$final_apk\""
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    android_build_apk
fi
