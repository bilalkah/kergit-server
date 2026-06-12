#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"

load_runtime_env_for_stop
source "$DOCKER_DIR/lib/compose.sh"

echo "[+] Stopping backend container..."
docker compose "${COMPOSE_ARGS[@]}" stop server-node || true
docker compose "${COMPOSE_ARGS[@]}" rm -f server-node || true
# Remove containers created by current and previous backend service names.
docker rm -f server-node app-node app-server kergit-dev-macos >/dev/null 2>&1 || true
