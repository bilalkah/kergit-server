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
echo "[+] Starting backend container..."
docker compose "${COMPOSE_ARGS[@]}" up -d --force-recreate server-node

echo "[+] Preparing Bazel cache permissions..."
docker exec -u root server-node bash -lc \
  "mkdir -p /root/.cache/bazel /root/.cache/bazelisk && chmod -R 777 /root/.cache/bazel /root/.cache/bazelisk"

echo "[+] Ensuring server log directory is writable..."
mkdir -p "$REPO_ROOT/logs"
chmod 0777 "$REPO_ROOT/logs"
