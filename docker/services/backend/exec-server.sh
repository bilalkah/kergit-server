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

BAZEL_CONFIG="vanilla"
if [ "$RUNTIME_MODE" = "prod" ]; then
  BAZEL_CONFIG="vanilla_opt"
fi

DOCKER_EXEC_FLAGS=(-i)
if [ -t 0 ] && [ -t 1 ]; then
  DOCKER_EXEC_FLAGS=(-it)
fi

# Supervise the backend via serve-backend.sh: if it crashes/exits, rebuild-and-rerun
# instead of taking the whole stack down. Docker's restart policy can't recover this
# because the container's main process is `sleep infinity` and the server runs as an exec'd
# process. `bash -lc` loads the login profile (bazel on PATH); the exported env then carries
# into the script. A clean Ctrl-C / SIGTERM stops the loop and the server.
exec docker exec "${DOCKER_EXEC_FLAGS[@]}" \
  --env WEB_DOMAIN \
  --env NUXT_PUBLIC_SUPABASE_URL \
  --env LIVEKIT_NODES \
  --env LIVEKIT_PRODUCTION_MODE \
  --env BAZEL_CONFIG="$BAZEL_CONFIG" \
  server-node bash -lc 'exec bash /root/workspace/docker/services/backend/serve-backend.sh'
