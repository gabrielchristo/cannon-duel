#!/usr/bin/env bash
# Uso: ./run_cmake.sh debug|release
#
# Debug e release compartilham build/ — ao trocar o modo é preciso rodar de
# novo porque Make/Ninja (Linux) gravam CMAKE_BUILD_TYPE na configuração, não
# por invocação. Só geradores multi-config (Visual Studio) mantêm Debug e
# Release lado a lado na mesma pasta.
set -euo pipefail

usage() {
    echo "Uso: $0 debug|release" >&2
    exit 1
}

case "${1:-}" in
    debug|release) BUILD_TYPE="$1" ;;
    *) usage ;;
esac

BUILD_DIR="build"
CMAKE_BUILD_TYPE="Debug"
DEBUG_MODE="ON"
if [ "$BUILD_TYPE" = "release" ]; then
    CMAKE_BUILD_TYPE="Release"
    DEBUG_MODE="OFF"
fi

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCANNON_DUEL_DEBUG_MODE="$DEBUG_MODE" \
    -DCMAKE_C_FLAGS="-Wno-error=maybe-uninitialized"

echo "$BUILD_TYPE" > "$BUILD_DIR/.build-type"
