#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"
source "$DOCKER_DIR/lib/livekit-runtime.sh"

parse_runtime_mode "$@" || {
  echo "Usage: $0 [--prod]"
  exit 1
}
load_runtime_env_strict "$RUNTIME_MODE"
source "$DOCKER_DIR/lib/compose.sh"

ensure_compose_network
ensure_livekit_routes_file
echo "[+] Starting Caddy..."
docker compose "${COMPOSE_ARGS[@]}" up -d --remove-orphans --force-recreate caddy-node
