#!/usr/bin/env bash
# Uso: ./build_pc.sh
# Compila build/ — o modo (debug/release) vem do último ./run_cmake.sh.
set -euo pipefail

cmake --build build -j
