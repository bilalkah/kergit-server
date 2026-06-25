#!/usr/bin/env bash
#
# Restart one or more individual stack services without touching the rest.
#
#   ./docker/restart-service.sh [--prod] <service> [<service> ...]
#
# Services: caddy | server | livekit | web | admin | redis
#
# Each service is brought down and back up via its own service scripts, so a restart only
# affects the named service(s). `server` runs the C++ binary in the FOREGROUND (like
# run-server.sh), so if it is requested it is always restarted LAST and the script blocks
# on it (Ctrl-C to stop). Example:
#
#   ./docker/restart-service.sh --prod caddy            # just caddy, prod config
#   ./docker/restart-service.sh web admin               # both clients, dev
#   ./docker/restart-service.sh --prod livekit server   # livekit, then exec server (blocks)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

KNOWN_SERVICES="caddy server livekit web admin redis"

usage() {
  echo "Usage: $0 [--prod] <service> [<service> ...]"
  echo "Services: ${KNOWN_SERVICES// /, }"
}

PROD=0
SERVICES=()
for arg in "$@"; do
  case "$arg" in
  --prod)
    PROD=1
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  caddy | server | livekit | web | admin | redis)
    SERVICES+=("$arg")
    ;;
  *)
    echo "❌ Unknown argument: $arg"
    usage
    exit 1
    ;;
  esac
done

if [ "${#SERVICES[@]}" -eq 0 ]; then
  echo "❌ No service specified."
  usage
  exit 1
fi

MODE_ARGS=()
WEB_RUN_ARGS=(--detached)
if [ "$PROD" -eq 1 ]; then
  MODE_ARGS=(--prod)
  WEB_RUN_ARGS=(--detached --prod)
fi

# Restart a single non-server service (stop then run). Mode flags are applied per service.
restart_one() {
  local svc="$1"
  echo "▶ Restarting ${svc}..."
  case "$svc" in
  caddy)
    "$SCRIPT_DIR/services/caddy/stop.sh" || true
    "$SCRIPT_DIR/services/caddy/run.sh" "${MODE_ARGS[@]}"
    ;;
  redis)
    "$SCRIPT_DIR/services/redis/stop.sh" || true
    "$SCRIPT_DIR/services/redis/run.sh" "${MODE_ARGS[@]}"
    ;;
  livekit)
    "$SCRIPT_DIR/services/livekit/stop.sh" || true
    "$SCRIPT_DIR/services/livekit/render.sh" "${MODE_ARGS[@]}" >/dev/null
    "$SCRIPT_DIR/services/livekit/run.sh" "${MODE_ARGS[@]}"
    ;;
  web)
    "$REPO_ROOT/clients/web/docker/stop-app.sh" || true
    "$REPO_ROOT/clients/web/docker/run-app.sh" "${WEB_RUN_ARGS[@]}"
    ;;
  admin)
    # The admin client is mode-agnostic (always detached, no --prod).
    "$REPO_ROOT/clients/admin/docker/stop-app.sh" || true
    "$REPO_ROOT/clients/admin/docker/run-app.sh" --detached
    ;;
  esac
}

# Restart everything except server first (these return immediately). Defer server to the
# end because it runs in the foreground and blocks.
RESTART_SERVER=0
for svc in "${SERVICES[@]}"; do
  if [ "$svc" = "server" ]; then
    RESTART_SERVER=1
    continue
  fi
  restart_one "$svc"
done

if [ "$RESTART_SERVER" -eq 1 ]; then
  echo "▶ Restarting server (foreground build+run; Ctrl-C to stop)..."
  "$SCRIPT_DIR/services/backend/stop.sh" || true
  "$SCRIPT_DIR/services/backend/run-container.sh" "${MODE_ARGS[@]}"
  exec "$SCRIPT_DIR/services/backend/exec-server.sh" "${MODE_ARGS[@]}"
fi

echo "[✓] Restarted: ${SERVICES[*]}"
