#!/usr/bin/env bash

LIVEKIT_RUNTIME_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIVEKIT_RUNTIME_REPO_ROOT="$(cd "$LIVEKIT_RUNTIME_LIB_DIR/../.." && pwd)"
GENERATED_COMPOSE_FILE="$LIVEKIT_RUNTIME_REPO_ROOT/docker/generated/livekit-compose.yml"
GENERATED_CADDY_ROUTES="$LIVEKIT_RUNTIME_REPO_ROOT/docker/caddy/generated/livekit_routes.caddy"
LIVEKIT_COMPOSE_SCHEMA_MARKER="# kergit-livekit-compose-version: 1"

configure_livekit_runtime() {
  local mode="${1:?configure_livekit_runtime requires dev or prod mode}"

  LIVEKIT_CONF_SUBDIR="$mode"
  RENDERED_LIVEKIT_CONF_DIR="$LIVEKIT_RUNTIME_REPO_ROOT/livekit/conf/rendered/$mode"
  LIVEKIT_TEMPLATE="$LIVEKIT_RUNTIME_REPO_ROOT/livekit/conf/$mode/node.yaml"
}

livekit_service_names() {
  if [ -f "$GENERATED_COMPOSE_FILE" ]; then
    sed -n 's/^  \([a-z0-9][a-z0-9-]*\):$/\1/p' "$GENERATED_COMPOSE_FILE"
  fi
}

livekit_container_names() {
  if [ -f "$GENERATED_COMPOSE_FILE" ]; then
    sed -n 's/^[[:space:]]*container_name:[[:space:]]*//p' "$GENERATED_COMPOSE_FILE"
  fi
}

livekit_compose_is_current() {
  [ -f "$GENERATED_COMPOSE_FILE" ] &&
    grep -Fxq "$LIVEKIT_COMPOSE_SCHEMA_MARKER" "$GENERATED_COMPOSE_FILE"
}

livekit_compose_matches_mode() {
  local mode="${1:?livekit_compose_matches_mode requires dev or prod mode}"

  livekit_compose_is_current &&
    grep -Fq "/livekit/conf/rendered/$mode/" "$GENERATED_COMPOSE_FILE"
}

ensure_livekit_routes_file() {
  mkdir -p "$(dirname "$GENERATED_CADDY_ROUTES")"
  touch "$GENERATED_CADDY_ROUTES"
}
