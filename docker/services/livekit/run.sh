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

MODE_ARGS=()
if [ "$RUNTIME_MODE" = "prod" ]; then
  MODE_ARGS=(--prod)
fi

if ! livekit_compose_matches_mode "$RUNTIME_MODE"; then
  "$SCRIPT_DIR/render.sh" "${MODE_ARGS[@]}" >/dev/null
fi

source "$DOCKER_DIR/lib/compose.sh"
mapfile -t LIVEKIT_SERVICES < <(livekit_service_names)
if [ "${#LIVEKIT_SERVICES[@]}" -eq 0 ]; then
  echo "❌ No generated LiveKit services found in $GENERATED_COMPOSE_FILE"
  exit 1
fi

ensure_compose_network
echo "[+] Starting LiveKit services: ${LIVEKIT_SERVICES[*]}"
docker compose "${COMPOSE_ARGS[@]}" up -d "${LIVEKIT_SERVICES[@]}"
