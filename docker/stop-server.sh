#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_NAME="${COMPOSE_PROJECT_NAME:-sercom}"

echo "[+] Stopping client containers..."
"$REPO_ROOT/clients/admin/docker/docker-stop-nuxt.sh" || true
"$REPO_ROOT/clients/web/docker/docker-stop-nuxt.sh" || true

echo "[+] Stopping and removing server containers..."
docker compose -p "$PROJECT_NAME" -f "$REPO_ROOT/docker/docker-compose.yml" down

echo "[+] Ensuring stray LiveKit containers are stopped..."
for name in livekit-node1 livekit-node2 livekit-caddy livekit-redis sercom-dev-ubuntu sercom-dev-macos; do
  if docker ps -a --format '{{.Names}}' | grep -q "^${name}$"; then
    docker rm -f "$name" >/dev/null 2>&1 || true
  fi
done

echo "[✓] Containers stopped and removed."
