#!/usr/bin/env bash

COMPOSE_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$COMPOSE_LIB_DIR/../.." && pwd)"
PROJECT_NAME="${COMPOSE_PROJECT_NAME:-kergit}"
COMPOSE_FILE="$REPO_ROOT/docker/docker-compose.yml"
GENERATED_COMPOSE_FILE="$REPO_ROOT/docker/generated/livekit-compose.yml"
ENV_FILE="${ENV_FILE:-$REPO_ROOT/.env}"
COMPOSE_NETWORK_NAME="kergit_default"
source "$COMPOSE_LIB_DIR/livekit-runtime.sh"

build_compose_args() {
  COMPOSE_ARGS=(-p "$PROJECT_NAME" -f "$COMPOSE_FILE")

  if [ -f "$ENV_FILE" ]; then
    COMPOSE_ARGS=(--env-file "$ENV_FILE" "${COMPOSE_ARGS[@]}")
  fi
  if livekit_compose_is_current; then
    COMPOSE_ARGS+=(-f "$GENERATED_COMPOSE_FILE")
  fi
}

ensure_compose_network() {
  docker network inspect "$COMPOSE_NETWORK_NAME" >/dev/null 2>&1 ||
    docker network create "$COMPOSE_NETWORK_NAME" >/dev/null 2>&1 ||
    true
}

build_compose_args
