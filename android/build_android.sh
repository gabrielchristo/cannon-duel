#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build + empacotamento do Cannon Duel para Android, 100% via linha de
# comando. Sem Gradle, sem Android Studio.
#
# Duas etapas de compilação, deliberadamente separadas:
#   1) libraylib.a — compilado com o Makefile OFICIAL do próprio raylib
#      (src/Makefile, PLATFORM=PLATFORM_ANDROID), que é o caminho testado e
#      documentado pra Android. O CMakeLists.txt genérico do raylib tem
#      suporte a Android historicamente instável (foi o que quebrou na
#      primeira tentativa) — o Makefile é o caminho confiável.
#   2) Box2D + nosso código → libCannonDuel.so — via CMake com o toolchain
#      do NDK, linkando o libraylib.a do passo 1 como lib pré-compilada.
#
# Pré-requisitos (variáveis de ambiente já devem estar setadas):
#   ANDROID_HOME       -> raiz do SDK (ex: /home/gabriel/.android_SDK)
#   ANDROID_NDK_HOME    -> raiz do NDK (ex: $ANDROID_HOME/ndk/27.0.12077973)
#
# Uso:
#   ./android/build_android.sh [debug|release]
#
# Saída:
#   android/build/CannonDuel.apk  (assinado, pronto pra instalar via adb)
# ---------------------------------------------------------------------------
set -euo pipefail

BUILD_TYPE="${1:-debug}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_DIR="$PROJECT_ROOT/android"
BUILD_DIR="$ANDROID_DIR/build"

ABI="arm64-v8a"                 # cobre praticamente todo celular de 2018+
RAYLIB_ARCH="arm64"             # nome que o Makefile do raylib usa pra essa ABI
API_LEVEL=21
RAYLIB_TAG="5.5"
BUILD_TOOLS_VERSION="30.0.3"    # ajuste se você tiver outra versão instalada
PLATFORM_VERSION="android-30"   # deve bater com o que está em $ANDROID_HOME/platforms

: "${ANDROID_HOME:?defina ANDROID_HOME antes de rodar este script}"
: "${ANDROID_NDK_HOME:?defina ANDROID_NDK_HOME antes de rodar este script}"

# Checagem extra: garante que ANDROID_NDK_HOME de fato aponta pra dentro de
# ANDROID_HOME e que o compilador existe nesse caminho — pega cedo o caso
# clássico de ANDROID_NDK_HOME ter sido exportado com um valor "congelado"
# de uma sessão de terminal antiga, antes de ANDROID_HOME estar definido
# (o valor de um export não se recalcula sozinho depois).
case "$ANDROID_NDK_HOME" in
    "$ANDROID_HOME"/*) ;;
    *)
        echo "ERRO: ANDROID_NDK_HOME (\"$ANDROID_NDK_HOME\") não está dentro de ANDROID_HOME (\"$ANDROID_HOME\")." >&2
        echo "       Isso costuma acontecer quando ANDROID_NDK_HOME foi exportado numa sessão" >&2
        echo "       de terminal antiga, antes de ANDROID_HOME estar definido — o valor fica" >&2
        echo "       'congelado' errado. Rode de novo:" >&2
        echo "         export ANDROID_HOME=$ANDROID_HOME" >&2
        echo "         export ANDROID_NDK_HOME=\$ANDROID_HOME/ndk/<versão>" >&2
        exit 1
        ;;
esac

CLANG_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API_LEVEL}-clang"
if [ ! -x "$CLANG_BIN" ]; then
    echo "ERRO: não encontrei o compilador do NDK em:" >&2
    echo "  $CLANG_BIN" >&2
    echo "Confira se ANDROID_NDK_HOME (\"$ANDROID_NDK_HOME\") está correto." >&2
    exit 1
fi

BUILD_TOOLS="$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION"
ANDROID_JAR="$ANDROID_HOME/platforms/$PLATFORM_VERSION/android.jar"

for tool in "$BUILD_TOOLS/aapt" "$BUILD_TOOLS/zipalign" "$BUILD_TOOLS/apksigner"; do
    if [ ! -x "$tool" ]; then
        echo "ERRO: não encontrei $tool — confira BUILD_TOOLS_VERSION no script." >&2
        exit 1
    fi
done
if [ ! -f "$ANDROID_JAR" ]; then
    echo "ERRO: não encontrei $ANDROID_JAR — confira PLATFORM_VERSION no script." >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"

CACERT="$PROJECT_ROOT/assets/certs/cacert.pem"
if [ ! -s "$CACERT" ]; then
    echo "==> assets/certs/cacert.pem ausente — baixando de curl.se..."
    mkdir -p "$(dirname "$CACERT")"
    curl -fsSL -o "$CACERT" https://curl.se/ca/cacert.pem
fi
if [ ! -s "$CACERT" ]; then
    echo "ERRO: assets/certs/cacert.pem não existe ou está vazio." >&2
    echo "       Rode: curl -o assets/certs/cacert.pem https://curl.se/ca/cacert.pem" >&2
    exit 1
fi
echo "==> cacert.pem OK ($(wc -c < "$CACERT") bytes)"

# ---------------------------------------------------------------------------
# 1/6: raylib via Makefile oficial
# ---------------------------------------------------------------------------
RAYLIB_SRC="$BUILD_DIR/raylib-src"
if [ ! -d "$RAYLIB_SRC" ]; then
    echo "==> 1/6: Baixando código-fonte do raylib ($RAYLIB_TAG)"
    git clone --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib.git "$RAYLIB_SRC"
fi

RAYLIB_A="$RAYLIB_SRC/src/libraylib.a"
echo "==> 1/6: Compilando libraylib.a (Makefile oficial, ANDROID_ARCH=$RAYLIB_ARCH, API=$API_LEVEL)"
make -C "$RAYLIB_SRC/src" clean >/dev/null 2>&1 || true
make -C "$RAYLIB_SRC/src" \
    PLATFORM=PLATFORM_ANDROID \
    ANDROID_NDK="$ANDROID_NDK_HOME" \
    ANDROID_ARCH="$RAYLIB_ARCH" \
    ANDROID_API_VERSION="$API_LEVEL"

if [ ! -f "$RAYLIB_A" ]; then
    echo "ERRO: $RAYLIB_A não foi gerado — confira o log de build do raylib acima." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 2/6: Box2D + nosso código via CMake, linkando o libraylib.a acima
# ---------------------------------------------------------------------------
echo "==> 2/6: Compilando Box2D + jogo (CMake + NDK, ABI=$ABI)"
CMAKE_BUILD_DIR="$BUILD_DIR/cmake-$ABI"
cmake -S "$ANDROID_DIR" -B "$CMAKE_BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API_LEVEL" \
    -DANDROID_STL=c++_static \
    -DRAYLIB_A="$RAYLIB_A" \
    -DRAYLIB_INCLUDE_DIR="$RAYLIB_SRC/src" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$CMAKE_BUILD_DIR" -j

SO_FILE="$CMAKE_BUILD_DIR/libCannonDuel.so"
if [ ! -f "$SO_FILE" ]; then
    echo "ERRO: $SO_FILE não foi gerado — confira o log de build acima." >&2
    exit 1
fi

echo "==> 3/6: Montando a árvore do APK"
APK_ROOT="$BUILD_DIR/apk_root"
rm -rf "$APK_ROOT"
mkdir -p "$APK_ROOT/lib/$ABI"
cp "$SO_FILE" "$APK_ROOT/lib/$ABI/"
cp -r "$PROJECT_ROOT/assets" "$APK_ROOT/assets"

echo "==> 4/6: Empacotando com aapt (não assinado, não alinhado)"
UNALIGNED_APK="$BUILD_DIR/CannonDuel.unaligned.apk"
"$BUILD_TOOLS/aapt" package -f -F "$UNALIGNED_APK" \
    -M "$ANDROID_DIR/AndroidManifest.xml" \
    -S "$ANDROID_DIR/res" \
    -I "$ANDROID_JAR" \
    -A "$APK_ROOT/assets"
(cd "$APK_ROOT" && "$BUILD_TOOLS/aapt" add "$UNALIGNED_APK" "lib/$ABI/libCannonDuel.so")

echo "==> 5/6: Alinhando (zipalign)"
ALIGNED_APK="$BUILD_DIR/CannonDuel.aligned.apk"
"$BUILD_TOOLS/zipalign" -f 4 "$UNALIGNED_APK" "$ALIGNED_APK"

echo "==> 6/6: Assinando (apksigner, chave de $BUILD_TYPE)"
KEYSTORE="$BUILD_DIR/${BUILD_TYPE}.keystore"
if [ ! -f "$KEYSTORE" ]; then
    echo "    (gerando keystore de $BUILD_TYPE pela primeira vez)"
    keytool -genkeypair -v \
        -keystore "$KEYSTORE" -storepass android -keypass android \
        -alias "${BUILD_TYPE}key" -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=CannonDuel $BUILD_TYPE, OU=Dev, O=GabrielChristo, C=BR"
fi

FINAL_APK="$BUILD_DIR/CannonDuel.apk"
"$BUILD_TOOLS/apksigner" sign \
    --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
    --out "$FINAL_APK" "$ALIGNED_APK"

echo ""
echo "APK pronto: $FINAL_APK"
echo "Pra instalar num celular/emulador conectado:"
echo "  \$ANDROID_HOME/platform-tools/adb install -r \"$FINAL_APK\""
