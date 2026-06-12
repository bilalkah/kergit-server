#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"

load_runtime_env_for_stop
source "$DOCKER_DIR/lib/compose.sh"

echo "[+] Stopping Redis..."
docker compose "${COMPOSE_ARGS[@]}" stop redis-node || true
docker compose "${COMPOSE_ARGS[@]}" rm -f redis-node || true
docker rm -f redis-node livekit-redis >/dev/null 2>&1 || true
