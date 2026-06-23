#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"

parse_runtime_mode "$@" || {
  echo "Usage: $0 [--prod]"
  exit 1
}
load_runtime_env_strict "$RUNTIME_MODE"
source "$DOCKER_DIR/lib/compose.sh"

ensure_compose_network
echo "[+] Starting Redis (app coordination + dedicated LiveKit registry)..."
docker compose "${COMPOSE_ARGS[@]}" up -d redis-node livekit-redis
