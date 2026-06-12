#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"
source "$DOCKER_DIR/lib/livekit-runtime.sh"

load_runtime_env_for_stop
source "$DOCKER_DIR/lib/compose.sh"

mapfile -t LIVEKIT_SERVICES < <(livekit_service_names)
mapfile -t LIVEKIT_CONTAINERS < <(livekit_container_names)

echo "[+] Stopping LiveKit services..."
if [ "${#LIVEKIT_SERVICES[@]}" -gt 0 ]; then
  docker compose "${COMPOSE_ARGS[@]}" stop "${LIVEKIT_SERVICES[@]}" || true
  docker compose "${COMPOSE_ARGS[@]}" rm -f "${LIVEKIT_SERVICES[@]}" || true
fi

for name in "${LIVEKIT_CONTAINERS[@]}"; do
  docker rm -f "$name" >/dev/null 2>&1 || true
done
