#!/usr/bin/env bash
# Servidor HTTP local pra testar a build web (fetch/WebSocket exigem http://).
# Uso: ./run_web.sh debug|release
set -euo pipefail

usage() {
    echo "Uso: $0 debug|release" >&2
    exit 1
}

case "${1:-}" in
    debug|release) BUILD_TYPE="$1" ;;
    *) usage ;;
esac

DIST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/web/dist/$BUILD_TYPE"

if [[ ! -f "$DIST_DIR/CannonDuel.html" ]]; then
    echo "Build não encontrada em $DIST_DIR — rode: ./build_web.sh $BUILD_TYPE" >&2
    exit 1
fi

echo "Servindo $DIST_DIR em http://localhost:8080/CannonDuel.html"
cd "$DIST_DIR" && python3 -m http.server 8080
