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

exec docker exec "${DOCKER_EXEC_FLAGS[@]}" \
  --env WEB_DOMAIN \
  --env NUXT_PUBLIC_SUPABASE_URL \
  --env LIVEKIT_NODES \
  --env LIVEKIT_PRODUCTION_MODE \
  server-node bash -lc \
  "cd /root/workspace && bazel run --config=$BAZEL_CONFIG //server:fake_discord"
