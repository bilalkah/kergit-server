#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_ROOT="$(cd "$DOCKER_DIR/.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"
source "$DOCKER_DIR/lib/livekit-runtime.sh"

parse_runtime_mode "$@" || {
  echo "Usage: $0 [--prod]"
  exit 1
}
load_runtime_env_strict "$RUNTIME_MODE"
configure_livekit_runtime "$RUNTIME_MODE"

LIVEKIT_API_KEY_VALUE="${LIVEKIT_API_KEY:-$(read_env_value LIVEKIT_API_KEY || true)}"
LIVEKIT_API_SECRET_VALUE="${LIVEKIT_API_SECRET:-$(read_env_value LIVEKIT_API_SECRET || true)}"
LIVEKIT_WEBHOOK_URL_VALUE="${LIVEKIT_WEBHOOK_URL:-$(read_env_value LIVEKIT_WEBHOOK_URL || true)}"

if [ -z "$LIVEKIT_API_KEY_VALUE" ]; then
  echo "❌ Missing LIVEKIT_API_KEY in $ENV_FILE"
  exit 1
fi
if [ -z "$LIVEKIT_API_SECRET_VALUE" ]; then
  echo "❌ Missing LIVEKIT_API_SECRET in $ENV_FILE"
  exit 1
fi
if [ -z "$LIVEKIT_WEBHOOK_URL_VALUE" ]; then
  LIVEKIT_WEBHOOK_URL_VALUE="http://server-node:8080/webhook"
fi

LIVEKIT_API_KEY="$LIVEKIT_API_KEY_VALUE" \
LIVEKIT_API_SECRET="$LIVEKIT_API_SECRET_VALUE" \
LIVEKIT_WEBHOOK_URL="$LIVEKIT_WEBHOOK_URL_VALUE" \
LIVEKIT_NODES="$LIVEKIT_NODES" \
  python3 "$REPO_ROOT/scripts/render_livekit_runtime.py" \
    --mode "$LIVEKIT_CONF_SUBDIR" \
    --template "$LIVEKIT_TEMPLATE" \
    --config-dir "$RENDERED_LIVEKIT_CONF_DIR" \
    --compose-output "$GENERATED_COMPOSE_FILE" \
    --caddy-output "$GENERATED_CADDY_ROUTES"
