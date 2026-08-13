#!/usr/bin/env bash
# Uso: ./run_cmake.sh [debug|release]
set -euo pipefail

BUILD_TYPE="${1:-debug}"
BUILD_DIR="build"
CMAKE_BUILD_TYPE="Debug"
if [ "$BUILD_TYPE" = "release" ]; then
    BUILD_DIR="build_release"
    CMAKE_BUILD_TYPE="Release"
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" -DCMAKE_C_FLAGS="-Wno-error=maybe-uninitialized"
