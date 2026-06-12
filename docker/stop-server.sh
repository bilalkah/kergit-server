#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/runtime-env.sh"
source "$SCRIPT_DIR/lib/livekit-runtime.sh"

load_runtime_env_for_stop
source "$SCRIPT_DIR/lib/compose.sh"

echo "[+] Stopping client containers..."
"$REPO_ROOT/clients/admin/docker/stop-app.sh" || true
"$REPO_ROOT/clients/web/docker/stop-app.sh" || true

"$SCRIPT_DIR/services/caddy/stop.sh" || true
"$SCRIPT_DIR/services/backend/stop.sh" || true
"$SCRIPT_DIR/services/livekit/stop.sh" || true
"$SCRIPT_DIR/services/redis/stop.sh" || true

echo "[+] Running final Compose cleanup..."
docker compose "${COMPOSE_ARGS[@]}" down --remove-orphans || true

echo "[+] Ensuring known server containers are removed..."
# Include legacy names so upgrades can clean containers from older configurations.
CONTAINER_NAMES=(
  caddy-node
  redis-node
  server-node
  app-node
  admin-node
  web-node
  livekit-caddy
  livekit-redis
  app-server
  kergit-dev-macos
)
while IFS= read -r name; do
  CONTAINER_NAMES+=("$name")
done < <(livekit_container_names)

for name in "${CONTAINER_NAMES[@]}"; do
  docker rm -f "$name" >/dev/null 2>&1 || true
done

"$SCRIPT_DIR/services/livekit/clean.sh" || true
docker network rm "$COMPOSE_NETWORK_NAME" >/dev/null 2>&1 || true

echo "[✓] Containers stopped and removed."
