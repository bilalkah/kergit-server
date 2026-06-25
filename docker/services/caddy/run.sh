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
source "$DOCKER_DIR/lib/compose.sh"

# Fail fast if the Caddyfile for this mode is missing. The conf.dev/conf.prod dirs are
# git-ignored (the Caddyfile is provided locally), and the container mounts that dir at
# /etc/caddy. Without this check, `up -d` "succeeds" but Caddy crash-loops with
# "open /etc/caddy/Caddyfile: no such file or directory".
CADDYFILE="$DOCKER_DIR/caddy/${CADDY_CONF_DIR}/Caddyfile"
if [ ! -f "$CADDYFILE" ]; then
  echo "❌ Missing Caddyfile for ${RUNTIME_MODE} mode: $CADDYFILE"
  echo "   The caddy config dir is git-ignored and provided locally."
  if [ "$RUNTIME_MODE" = "dev" ]; then
    echo "   If your stack runs in production, use: $0 --prod"
  fi
  exit 1
fi

ensure_compose_network
ensure_livekit_routes_file
echo "[+] Starting Caddy (${RUNTIME_MODE}, ${CADDY_CONF_DIR}/Caddyfile)..."
# Only manage caddy-node. Do NOT pass --remove-orphans: this script runs with a partial
# compose file set (it omits the generated LiveKit compose when it isn't current), so
# --remove-orphans would treat sibling containers like livekit-* as orphans and delete
# them. Each service script must up/down only its own service. --force-recreate so an
# .env / config change is picked up.
docker compose "${COMPOSE_ARGS[@]}" up -d --force-recreate caddy-node
