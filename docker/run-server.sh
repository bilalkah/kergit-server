#!/usr/bin/env bash
set -euo pipefail

MULTI_NODE=0
PROD=0

for arg in "$@"; do
  case "$arg" in
    --multi)
      MULTI_NODE=1
      ;;
    --prod)
      PROD=1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.yml"
PROJECT_NAME="${COMPOSE_PROJECT_NAME:-sercom}"
DEV_CONTAINER="sercom-dev-ubuntu"
BAZEL_CONFIG="vanilla_dbg"

echo "▶ Starting Redis..."
docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" up -d redis

if [ "$MULTI_NODE" -eq 1 ]; then
  echo "▶ Starting LiveKit multi-node..."
  docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" --profile multi up -d livekit-node1 livekit-node2
else
  echo "▶ Starting LiveKit single node..."
  docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" up -d livekit-node1
  docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" stop livekit-node2 >/dev/null 2>&1 || true
fi

echo "▶ Starting Caddy load balancer..."
docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" up -d caddy

echo "▶ Starting socket server..."
docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" up -d ubuntu-dev

echo "▶ Starting admin client..."
"$REPO_ROOT/clients/admin/docker/docker-run-nuxt.sh" --detached

echo "▶ Starting web client..."
if [ "$PROD" -eq 1 ]; then
  "$REPO_ROOT/clients/web/docker/docker-run-nuxt.sh" --detached --prod
else
  "$REPO_ROOT/clients/web/docker/docker-run-nuxt.sh" --detached
fi

exec docker exec -it "$DEV_CONTAINER" bash -lc \
  "cd /home/sercom/workspace && bazel run --config=$BAZEL_CONFIG //server:fake_discord"
