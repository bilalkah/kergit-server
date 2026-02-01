#!/usr/bin/env bash
set -euo pipefail

USE_HTTPS=0
for arg in "$@"; do
  case "$arg" in
    --https)
      USE_HTTPS=1
      ;;
    --http)
      USE_HTTPS=0
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.yml"
CERT_DIR="$REPO_ROOT/certs"
DEV_CONTAINER="sercom-dev-ubuntu"

if [ "$USE_HTTPS" -eq 1 ]; then
  if [ ! -f "$CERT_DIR/cert.pem" ] || [ ! -f "$CERT_DIR/key.pem" ]; then
    echo "❌ Missing TLS certs in $CERT_DIR (cert.pem, key.pem required)."
    exit 1
  fi

  echo "▶ Starting LiveKit (behind Caddy TLS)..."
  docker compose -f "$COMPOSE_FILE" up -d livekit caddy

  BAZEL_CONFIG="sslquic_opt"
else
  echo "▶ Starting LiveKit (plain WS)..."
  docker compose -f "$COMPOSE_FILE" up -d livekit
  docker compose -f "$COMPOSE_FILE" stop caddy >/dev/null 2>&1 || true

  BAZEL_CONFIG="vanilla_opt"
fi

echo "▶ Starting socket server (bazel --config=$BAZEL_CONFIG)..."
docker compose -f "$COMPOSE_FILE" up -d ubuntu-dev

exec docker exec -it "$DEV_CONTAINER" bash -lc \
  "cd /home/sercom/workspace && bazel run --config=$BAZEL_CONFIG //server:fake_discord"