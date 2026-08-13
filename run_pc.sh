#!/usr/bin/env bash
# Uso: ./run_pc.sh [debug|release]
set -euo pipefail

BUILD_TYPE="${1:-debug}"
BUILD_DIR="build"
if [ "$BUILD_TYPE" = "release" ]; then
    BUILD_DIR="build_release"
fi

"./$BUILD_DIR/CannonDuel"
