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

# The backend now runs as the server-node container's MAIN process (see docker-compose.yml
# `command:`), supervised by Docker's `restart: unless-stopped` policy — a crash exits the
# container and Docker restarts it, with no dependence on this foreground session. So this
# script no longer launches the server; it just follows the container's output so
# `run-server.sh` still blocks showing backend logs. Ctrl-C here detaches without stopping
# the server (the old behaviour killed it).
exec docker logs -f --tail 200 server-node
