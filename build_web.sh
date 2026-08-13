#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build do Cannon Duel pra Web (WebAssembly via Emscripten). Sem servidor
# próprio: gera arquivos estáticos que rodam em qualquer host HTTP simples,
# inclusive GitHub Pages.
#
# Pré-requisito: emsdk instalado e ativado (ver web/README.md). O script
# tenta localizar automaticamente em $EMSDK ou ~/emsdk; senão, rode antes:
#   source /caminho/pro/emsdk/emsdk_env.sh
#
# Uso:
#   ./build_web.sh [debug|release]
#
# Saída:
#   web/dist/CannonDuel.html (+ .js, .wasm, .data)
# ---------------------------------------------------------------------------
set -euo pipefail

BUILD_TYPE="${1:-release}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/web/build"
DIST_DIR="$PROJECT_ROOT/web/dist"

if ! command -v emcmake >/dev/null 2>&1; then
    for candidate in "${EMSDK:-}" "$HOME/Git/emsdk" "$HOME/emsdk"; do
        if [ -n "$candidate" ] && [ -f "$candidate/emsdk_env.sh" ]; then
            # shellcheck disable=SC1091
            source "$candidate/emsdk_env.sh"
            break
        fi
    done
fi
if ! command -v emcmake >/dev/null 2>&1; then
    echo "ERRO: emcmake não encontrado no PATH." >&2
    echo "       Instale o emsdk (https://emscripten.org/docs/getting_started/downloads.html)" >&2
    echo "       e rode: source <caminho-do-emsdk>/emsdk_env.sh" >&2
    exit 1
fi

CMAKE_BUILD_TYPE="Release"
if [ "$BUILD_TYPE" = "debug" ]; then
    CMAKE_BUILD_TYPE="Debug"
fi

echo "==> Configurando (emcmake cmake, CMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE)"
emcmake cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCANNON_DUEL_DEBUG_MODE=$([ "$BUILD_TYPE" = "debug" ] && echo ON || echo OFF)

echo "==> Compilando (emmake)"
emmake cmake --build "$BUILD_DIR" -j

mkdir -p "$DIST_DIR"
for ext in html js wasm data; do
    f="$BUILD_DIR/CannonDuel.$ext"
    [ -f "$f" ] && cp "$f" "$DIST_DIR/"
done

echo ""
echo "Build pronta em: $DIST_DIR"
echo "Pra testar localmente (fetch() exige http:// ou https://, não file://):"
echo "  cd \"$DIST_DIR\" && python3 -m http.server 8080"
echo "  depois abra http://localhost:8080/CannonDuel.html"
