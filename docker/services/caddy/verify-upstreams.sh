#!/usr/bin/env bash
set -euo pipefail

UPSTREAM_HOSTS=(
  server-node
  web-node
  admin-node
)

echo "[+] Verifying Caddy Docker DNS upstreams..."
for host in "${UPSTREAM_HOSTS[@]}"; do
  if ! docker exec caddy-node getent hosts "$host" >/dev/null 2>&1; then
    echo "❌ Caddy cannot resolve Docker upstream '$host'."
    echo "Ensure caddy-node and $host share the kergit_default network."
    exit 1
  fi
done

echo "[✓] Caddy Docker DNS upstreams resolved."
