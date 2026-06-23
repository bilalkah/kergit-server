#!/usr/bin/env bash
#
# Supervised backend launcher — runs INSIDE the server-node container.
#
# Builds and runs the C++ server, and if it crashes/exits, rebuilds-and-reruns so a single
# fault doesn't take the whole stack down. A clean SIGINT/SIGTERM stops the loop and the
# server (so Ctrl-C during an interactive run exits cleanly).
#
# Used by docker/services/backend/exec-server.sh for the interactive run. It is also safe
# to use as the container's main command (compose `command:`) to auto-start the backend on
# container/host restart — it logs to `docker logs server-node` in that mode.
#
# Configuration (env vars, with defaults):
#   BAZEL_CONFIG            bazel --config to run (default: vanilla)
#   WORKSPACE_DIR           repo path inside the container (default: /root/workspace)
#   RESTART_DELAY_SECONDS   backoff between restarts (default: 2)
set -uo pipefail

BAZEL_CONFIG="${BAZEL_CONFIG:-vanilla}"
WORKSPACE_DIR="${WORKSPACE_DIR:-/root/workspace}"
RESTART_DELAY_SECONDS="${RESTART_DELAY_SECONDS:-2}"

cd "$WORKSPACE_DIR"

# Exit the loop cleanly on Ctrl-C / container stop instead of restarting.
trap 'echo "[serve-backend] stop requested; exiting"; exit 0' INT TERM

attempt=0
while true; do
  attempt=$((attempt + 1))
  echo "[serve-backend] starting backend (config=${BAZEL_CONFIG}, attempt=${attempt})"
  bazel run --config="$BAZEL_CONFIG" //server:fake_discord || true
  echo "[serve-backend] backend exited; restarting in ${RESTART_DELAY_SECONDS}s (Ctrl-C to stop)"
  sleep "$RESTART_DELAY_SECONDS"
done
