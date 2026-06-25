#!/usr/bin/env bash

RUNTIME_ENV_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_ENV_REPO_ROOT="$(cd "$RUNTIME_ENV_LIB_DIR/../.." && pwd)"
ENV_FILE="${ENV_FILE:-$RUNTIME_ENV_REPO_ROOT/.env}"

RETIRED_CONFIG_VARIABLES=(
  CADDY_SITE_HOST
  CADDY_ALLOWED_ORIGIN_1
  CADDY_ALLOWED_ORIGIN_2
  CADDY_WSS_ORIGIN_1
  CADDY_WSS_ORIGIN_2
  LIVEKIT_NODE1_PUBLIC_URL
  LIVEKIT_NODE2_PUBLIC_URL
  INVITE_BASE_URL
  SUPABASE_PROJECT_ORIGIN
  SUPABASE_EXPECTED_ISS
  ACTUAL_DOMAIN
  LIVEKIT_NODE1_PRIVATE_URL
  LIVEKIT_NODE2_PRIVATE_URL
  LIVEKIT_PROMETHEUS_PORT
  LIVEKIT_PROMETHEUS_NODE1_PORT
  LIVEKIT_PROMETHEUS_NODE2_PORT
  CADDY_LIVEKIT_NODE1_TARGET
  CADDY_LIVEKIT_NODE2_TARGET
  ADMIN_LIVEKIT_NODE1_METRICS_TARGET
  ADMIN_LIVEKIT_NODE2_METRICS_TARGET
  LIVEKIT_NODE1_CONFIG_PATH
  LIVEKIT_NODE2_CONFIG_PATH
  LIVEKIT_PORT
)

parse_runtime_mode() {
  RUNTIME_MODE="dev"

  case "$#" in
  0)
    ;;
  1)
    if [ "$1" != "--prod" ]; then
      return 1
    fi
    RUNTIME_MODE="prod"
    ;;
  *)
    return 1
    ;;
  esac
}

read_env_value() {
  local key="$1"
  local line

  if [ ! -f "$ENV_FILE" ]; then
    return 1
  fi

  line="$(grep -E "^${key}=" "$ENV_FILE" | tail -n 1 || true)"
  if [ -z "$line" ]; then
    return 1
  fi

  line="${line#*=}"
  if [[ "$line" == \"*\" && "$line" == *\" ]]; then
    line="${line:1:${#line}-2}"
  elif [[ "$line" == \'*\' && "$line" == *\' ]]; then
    line="${line:1:${#line}-2}"
  fi

  printf '%s' "$line"
}

runtime_env_resolve_value() {
  local key="$1"
  local fallback="$2"
  local value="${!key:-}"

  if [ -z "$value" ]; then
    value="$(read_env_value "$key" || true)"
  fi

  printf -v "$key" '%s' "${value:-$fallback}"
}

runtime_env_check_retired_variables() {
  local retired_key

  for retired_key in "${RETIRED_CONFIG_VARIABLES[@]}"; do
    if printenv "$retired_key" >/dev/null 2>&1 ||
      { [ -f "$ENV_FILE" ] && grep -q "^${retired_key}=" "$ENV_FILE"; }; then
      echo "❌ Retired configuration variable '$retired_key' is set."
      echo "Configure public origins and LiveKit nodes using the canonical variables."
      return 1
    fi
  done
}

validate_https_origin() {
  local key="$1"
  local origin="$2"

  python3 - "$key" "$origin" <<'PY'
import sys
import ipaddress
import re
from urllib.parse import urlsplit

key, origin = sys.argv[1:3]
message = f"{key} must be a canonical HTTPS origin without a path, credentials, or :443."

try:
    parsed = urlsplit(origin)
    port = parsed.port
except ValueError:
    print(f"❌ {message}")
    raise SystemExit(1)

host = parsed.hostname or ""
authority = parsed.netloc
valid_host = False
try:
    ipaddress.ip_address(host)
    valid_host = True
except ValueError:
    labels = host.split(".")
    valid_host = bool(labels) and all(
        label
        and not label.startswith("-")
        and not label.endswith("-")
        and re.fullmatch(r"[a-z0-9-]+", label)
        for label in labels
    )

if (
    parsed.scheme != "https"
    or not valid_host
    or authority != authority.lower()
    or parsed.username is not None
    or parsed.password is not None
    or parsed.path
    or parsed.query
    or parsed.fragment
    or port == 443
    or (port is not None and port < 1)
    or any(ch.isspace() for ch in origin)
):
    print(f"❌ {message}")
    raise SystemExit(1)
PY
}

validate_livekit_webhook_url() {
  local webhook_url="$1"

  case "$webhook_url" in
  http://app-server | http://app-server/* | http://app-server:* | \
    https://app-server | https://app-server/* | https://app-server:* | \
    http://app-node | http://app-node/* | http://app-node:* | \
    https://app-node | https://app-node/* | https://app-node:*)
    echo "❌ LIVEKIT_WEBHOOK_URL targets retired backend host '${webhook_url#*://}'."
    echo "Use LIVEKIT_WEBHOOK_URL=http://server-node:8080/webhook for this Compose stack."
    return 1
    ;;
  esac
}

validate_admin_allowed_cidr() {
  local mode="$1"
  local allowed_cidrs="$2"

  if [ "$mode" = "prod" ] && [ "$allowed_cidrs" = "private_ranges" ]; then
    echo "❌ Production ADMIN_ALLOWED_CIDR cannot be 'private_ranges'."
    echo "Docker and reverse proxies can make public requests appear to come from private addresses."
    echo "Use an explicit trusted CIDR, or 127.0.0.1/32 to disable public admin access."
    return 1
  fi

  python3 - "$allowed_cidrs" <<'PY'
import ipaddress
import sys

value = sys.argv[1].strip()
if value == "private_ranges":
    raise SystemExit(0)

cidrs = value.split()
if not cidrs:
    print("❌ ADMIN_ALLOWED_CIDR must contain at least one CIDR.")
    raise SystemExit(1)

for cidr in cidrs:
    try:
        ipaddress.ip_network(cidr, strict=False)
    except ValueError:
        print(f"❌ ADMIN_ALLOWED_CIDR contains an invalid CIDR: {cidr}")
        raise SystemExit(1)
PY
}

runtime_env_resolve_compose_defaults() {
  runtime_env_resolve_value CONTROL_HOST "0.0.0.0"
  runtime_env_resolve_value CONTROL_PORT "8081"
  runtime_env_resolve_value CADDY_TLS_CERT_FILE "/certs/dev/localhost.pem"
  runtime_env_resolve_value CADDY_TLS_KEY_FILE "/certs/dev/localhost-key.pem"
  runtime_env_resolve_value ADMIN_ALLOWED_CIDR "127.0.0.1/32"

  export CONTROL_HOST CONTROL_PORT
  export CADDY_TLS_CERT_FILE CADDY_TLS_KEY_FILE
  export ADMIN_ALLOWED_CIDR
}

runtime_env_export_canonical_values() {
  export WEB_DOMAIN
  export WEB_WWW_DOMAIN
  export NUXT_PUBLIC_SUPABASE_URL
  export WEB_SOCKET_ORIGIN
  export LIVEKIT_NODES
  export LIVEKIT_PRODUCTION_MODE
  export CADDY_CONF_DIR
}

load_runtime_env_strict() {
  local mode="${1:-dev}"
  local public_authority

  if [ "$mode" != "dev" ] && [ "$mode" != "prod" ]; then
    echo "❌ Runtime mode must be 'dev' or 'prod'."
    return 1
  fi
  if [ ! -f "$ENV_FILE" ]; then
    echo "❌ Missing .env file at $ENV_FILE"
    echo "Copy .env.example to .env and fill in your local values first."
    return 1
  fi

  runtime_env_check_retired_variables || return 1
  runtime_env_resolve_value WEB_DOMAIN ""
  runtime_env_resolve_value WEB_WWW_DOMAIN ""
  runtime_env_resolve_value NUXT_PUBLIC_SUPABASE_URL ""
  runtime_env_resolve_value LIVEKIT_NODES ""
  runtime_env_resolve_value LIVEKIT_WEBHOOK_URL "http://server-node:8080/webhook"
  validate_https_origin WEB_DOMAIN "$WEB_DOMAIN" || return 1
  validate_https_origin NUXT_PUBLIC_SUPABASE_URL "$NUXT_PUBLIC_SUPABASE_URL" || return 1
  validate_livekit_webhook_url "$LIVEKIT_WEBHOOK_URL" || return 1

  if [ -z "$LIVEKIT_NODES" ]; then
    echo "❌ Missing LIVEKIT_NODES in $ENV_FILE"
    return 1
  fi

  WEB_SOCKET_ORIGIN="wss://${WEB_DOMAIN#https://}"
  if [ "$mode" = "prod" ]; then
    CADDY_CONF_DIR="conf.prod"
    LIVEKIT_PRODUCTION_MODE=1
    public_authority="${WEB_DOMAIN#https://}"
    case "$public_authority" in
    localhost | localhost:* | 127.0.0.1 | 127.0.0.1:* | \
      0.0.0.0 | 0.0.0.0:* | "[::1]" | "[::1]:"*)
      echo "❌ Production mode requires WEB_DOMAIN to be a non-local HTTPS origin."
      return 1
      ;;
    esac
    # conf.prod/Caddyfile references {$WEB_WWW_DOMAIN} as a site label, so prod must set it.
    if [ -z "$WEB_WWW_DOMAIN" ]; then
      echo "❌ Production mode requires WEB_WWW_DOMAIN (the www host that 301-redirects to WEB_DOMAIN)."
      return 1
    fi
    validate_https_origin WEB_WWW_DOMAIN "$WEB_WWW_DOMAIN" || return 1
  else
    CADDY_CONF_DIR="conf.dev"
    LIVEKIT_PRODUCTION_MODE=0
  fi

  runtime_env_resolve_compose_defaults
  validate_admin_allowed_cidr "$mode" "$ADMIN_ALLOWED_CIDR" || return 1
  runtime_env_export_canonical_values
  export LIVEKIT_WEBHOOK_URL
}

load_runtime_env_for_stop() {
  runtime_env_resolve_value WEB_DOMAIN "https://localhost"
  runtime_env_resolve_value WEB_WWW_DOMAIN ""
  runtime_env_resolve_value NUXT_PUBLIC_SUPABASE_URL "https://localhost"
  runtime_env_resolve_value LIVEKIT_NODES "[]"
  runtime_env_resolve_value LIVEKIT_PRODUCTION_MODE "0"
  runtime_env_resolve_value CADDY_CONF_DIR "conf.dev"

  WEB_SOCKET_ORIGIN="wss://${WEB_DOMAIN#https://}"

  runtime_env_resolve_compose_defaults
  runtime_env_export_canonical_values
}
