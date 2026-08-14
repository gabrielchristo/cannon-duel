#!/usr/bin/env bash
# Uso: ./run_cmake_android.sh debug|release
#
# Debug e release compartilham android/build/ — ao trocar o modo é preciso
# rodar de novo (mesmo princípio do ./run_cmake.sh no desktop).
set -euo pipefail

usage() {
    echo "Uso: $0 debug|release" >&2
    exit 1
}

case "${1:-}" in
    debug|release) BUILD_TYPE="$1" ;;
    *) usage ;;
esac

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=build_android.sh
source "$PROJECT_ROOT/build_android.sh"

android_configure_cmake "$BUILD_TYPE"
