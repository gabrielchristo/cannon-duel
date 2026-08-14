#!/usr/bin/env bash
# Uso: ./run_pc.sh
# Roda build/ — o modo (debug/release) vem do último ./run_cmake.sh.
set -euo pipefail

BUILD_DIR="build"

if [ -f "$BUILD_DIR/CannonDuel" ]; then
    exec "$BUILD_DIR/CannonDuel"
fi

BUILD_TYPE=""
if [ -f "$BUILD_DIR/.build-type" ]; then
    BUILD_TYPE="$(tr -d '[:space:]' < "$BUILD_DIR/.build-type")"
fi

if [ "$BUILD_TYPE" = "release" ] && [ -f "$BUILD_DIR/Release/CannonDuel.exe" ]; then
    exec "$BUILD_DIR/Release/CannonDuel.exe"
fi
if [ -f "$BUILD_DIR/Debug/CannonDuel.exe" ]; then
    exec "$BUILD_DIR/Debug/CannonDuel.exe"
fi
if [ -f "$BUILD_DIR/Release/CannonDuel.exe" ]; then
    exec "$BUILD_DIR/Release/CannonDuel.exe"
fi

echo "Executável não encontrado — rode: ./run_cmake.sh debug|release && ./build_pc.sh" >&2
exit 1
