#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"

load_runtime_env_for_stop
source "$DOCKER_DIR/lib/compose.sh"

echo "[+] Stopping Caddy..."
docker compose "${COMPOSE_ARGS[@]}" stop caddy-node || true
docker compose "${COMPOSE_ARGS[@]}" rm -f caddy-node || true
docker rm -f caddy-node livekit-caddy >/dev/null 2>&1 || true
