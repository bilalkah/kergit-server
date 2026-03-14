#!/usr/bin/env bash
set -euo pipefail

MULTI_NODE=0
PROD=0
SERVER_ONLY=0

for arg in "$@"; do
  case "$arg" in
    --multi)
      MULTI_NODE=1
      ;;
    --prod)
      PROD=1
      ;;
    --server-only)
      SERVER_ONLY=1
      ;;
    *)
      echo "❌ Unknown argument: $arg"
      echo "Usage: $0 [--multi] [--prod] [--server-only]"
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.yml"
PROJECT_NAME="${COMPOSE_PROJECT_NAME:-sercom}"
DEV_CONTAINER="kergit-ubuntu-server"
BAZEL_CONFIG="vanilla"
CADDY_CONF_DIR="conf.dev"
LIVEKIT_CONF_SUBDIR="dev"

if [ "$PROD" -eq 1 ]; then
  BAZEL_CONFIG="vanilla_opt"
  CADDY_CONF_DIR="conf.prod"
  LIVEKIT_CONF_SUBDIR="prod"
fi

export CADDY_CONF_DIR
export LIVEKIT_CONF_SUBDIR

LIVEKIT_CONF_PATH="$REPO_ROOT/livekit/conf/$LIVEKIT_CONF_SUBDIR"
if [ ! -d "$LIVEKIT_CONF_PATH" ]; then
  echo "❌ LiveKit config directory not found: $LIVEKIT_CONF_PATH"
  exit 1
fi

if [ "$SERVER_ONLY" -eq 0 ]; then
  echo "▶ Using Caddy config dir: docker/caddy/$CADDY_CONF_DIR (Caddyfile)"
  echo "▶ Using LiveKit config dir: livekit/conf/$LIVEKIT_CONF_SUBDIR"
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
fi

echo "▶ Starting socket server..."
docker compose -p "$PROJECT_NAME" -f "$COMPOSE_FILE" up -d --force-recreate ubuntu-dev

echo "▶ Preparing cache permissions..."
docker exec -u root "$DEV_CONTAINER" bash -lc \
  "mkdir -p /root/.cache/bazel /root/.cache/bazelisk && chmod -R 777 /root/.cache/bazel /root/.cache/bazelisk"

echo "▶ Ensuring server log directory is writable..."
mkdir -p "$REPO_ROOT/logs"
chmod 0777 "$REPO_ROOT/logs"

if [ "$SERVER_ONLY" -eq 0 ]; then
  echo "▶ Starting admin client..."
  "$REPO_ROOT/clients/admin/docker/run-app.sh" --detached

  echo "▶ Starting web client..."
  if [ "$PROD" -eq 1 ]; then
    "$REPO_ROOT/clients/web/docker/run-app.sh" --detached --prod
  else
    "$REPO_ROOT/clients/web/docker/run-app.sh" --detached
  fi
else
  echo "▶ Server-only mode: skipping Redis, LiveKit, Caddy, admin client, and web client."
fi

DOCKER_EXEC_FLAGS=(-i)
if [ -t 0 ] && [ -t 1 ]; then
  DOCKER_EXEC_FLAGS=(-it)
fi

exec docker exec "${DOCKER_EXEC_FLAGS[@]}" "$DEV_CONTAINER" bash -lc \
  "cd /root/workspace && bazel run --config=$BAZEL_CONFIG //server:fake_discord"
