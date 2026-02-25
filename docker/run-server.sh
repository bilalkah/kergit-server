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
    *)
      echo "❌ Unknown argument: $arg"
      echo "Usage: $0 [--multi] [--prod]"
      exit 1
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
docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" up -d --force-recreate ubuntu-dev

echo "▶ Preparing cache permissions..."
docker exec -u root "$DEV_CONTAINER" bash -lc \
  "mkdir -p /home/sercom/.cache/bazel /home/sercom/.cache/bazelisk && chown -R sercom:sercom /home/sercom/.cache /home/sercom/.ccache"

echo "▶ Starting admin client..."
"$REPO_ROOT/clients/admin/docker/run-app.sh" --detached

echo "▶ Starting web client..."
if [ "$PROD" -eq 1 ]; then
  "$REPO_ROOT/clients/web/docker/run-app.sh" --detached --prod
else
  "$REPO_ROOT/clients/web/docker/run-app.sh" --detached
fi

DOCKER_EXEC_FLAGS=(-i)
if [ -t 0 ] && [ -t 1 ]; then
  DOCKER_EXEC_FLAGS=(-it)
fi

exec docker exec "${DOCKER_EXEC_FLAGS[@]}" -u sercom "$DEV_CONTAINER" bash -lc \
  "cd /home/sercom/workspace && bazel run --config=$BAZEL_CONFIG //server:fake_discord"
