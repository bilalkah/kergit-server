#!/usr/bin/env bash
set -euo pipefail

PROD=0

for arg in "$@"; do
  case "$arg" in
  --prod)
    PROD=1
    ;;
  *)
    echo "❌ Unknown argument: $arg"
    echo "Usage: $0 [--prod]"
    exit 1
    ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/lib/runtime-env.sh"

RUNTIME_MODE="dev"
MODE_ARGS=()
if [ "$PROD" -eq 1 ]; then
  RUNTIME_MODE="prod"
  MODE_ARGS=(--prod)
fi

load_runtime_env_strict "$RUNTIME_MODE"
source "$SCRIPT_DIR/lib/compose.sh"

PROD_CERT_PATH="$REPO_ROOT/certs/prod/origin.pem"
PROD_KEY_PATH="$REPO_ROOT/certs/prod/origin-key.pem"

if [ "$PROD" -eq 0 ]; then
  echo "▶ Ensuring localhost TLS certificate (mkcert)..."
  "$REPO_ROOT/scripts/certs/generate-dev-cert.sh"
elif [ ! -f "$PROD_CERT_PATH" ] || [ ! -f "$PROD_KEY_PATH" ]; then
  echo "❌ Missing prod TLS certificate files."
  echo "Expected:"
  echo "  $PROD_CERT_PATH"
  echo "  $PROD_KEY_PATH"
  echo "Place your Cloudflare Origin cert/key there and retry."
  exit 1
else
  echo "▶ Prod mode: using the configured origin certificate for your public domain."
fi

"$SCRIPT_DIR/services/redis/run.sh" "${MODE_ARGS[@]}"

LIVEKIT_SERVICES="$("$SCRIPT_DIR/services/livekit/render.sh" "${MODE_ARGS[@]}")"
echo "▶ Rendered LiveKit services: $LIVEKIT_SERVICES"
"$SCRIPT_DIR/services/livekit/run.sh" "${MODE_ARGS[@]}"

"$SCRIPT_DIR/services/backend/run-container.sh" "${MODE_ARGS[@]}"
"$SCRIPT_DIR/services/livekit/verify.sh" "${MODE_ARGS[@]}"

echo "▶ Starting admin client..."
"$REPO_ROOT/clients/admin/docker/run-app.sh" --detached

echo "▶ Starting web client..."
if [ "$PROD" -eq 1 ]; then
  "$REPO_ROOT/clients/web/docker/run-app.sh" --detached --prod
else
  "$REPO_ROOT/clients/web/docker/run-app.sh" --detached
fi

echo "▶ Using Caddy config dir: docker/caddy/$CADDY_CONF_DIR (Caddyfile)"
echo "▶ Using web domain: $WEB_DOMAIN"
"$SCRIPT_DIR/services/caddy/run.sh" "${MODE_ARGS[@]}"
"$SCRIPT_DIR/services/caddy/verify-upstreams.sh"

exec "$SCRIPT_DIR/services/backend/exec-server.sh" "${MODE_ARGS[@]}"
