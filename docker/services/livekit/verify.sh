#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
source "$DOCKER_DIR/lib/runtime-env.sh"

parse_runtime_mode "$@" || {
  echo "Usage: $0 [--prod]"
  exit 1
}
load_runtime_env_strict "$RUNTIME_MODE"

mapfile -t LIVEKIT_ENDPOINTS < <(
  python3 - "$LIVEKIT_NODES" <<'PY'
import json
import sys

for node in json.loads(sys.argv[1]):
    print(f"{node['id']} {node['signal_port']}")
PY
)

echo "[+] Verifying backend access to LiveKit nodes..."
for endpoint in "${LIVEKIT_ENDPOINTS[@]}"; do
  read -r host port <<<"$endpoint"
  if ! docker exec server-node bash -lc \
    "getent hosts '$host' >/dev/null && timeout 3 bash -lc '</dev/tcp/$host/$port'" \
    >/dev/null 2>&1; then
    echo "❌ Backend cannot connect to LiveKit node '$host:$port'."
    echo "Ensure server-node and $host share the kergit_default network."
    exit 1
  fi
done

echo "[✓] Backend can connect to all LiveKit nodes."
