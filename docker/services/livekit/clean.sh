#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/livekit-runtime.sh"

rm -rf "$LIVEKIT_RUNTIME_REPO_ROOT/livekit/conf/rendered/dev" \
  "$LIVEKIT_RUNTIME_REPO_ROOT/livekit/conf/rendered/prod"
rm -f "$GENERATED_COMPOSE_FILE" "$GENERATED_CADDY_ROUTES"
