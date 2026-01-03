#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: depgraph <bazel-target> <out.(svg|png|pdf)> [--scope=local|all]"
  exit 2
fi

TARGET="$1"
OUT="$2"
SCOPE="${3:-"--scope=local"}"

# Bazel run executes from a bazel output tree; jump back to the real workspace.
if [[ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
  cd "$BUILD_WORKSPACE_DIRECTORY"
fi

# Pick output format from extension
EXT="${OUT##*.}"
case "$EXT" in
  svg|png|pdf) : ;;
  *)
    echo "unknown output extension: .$EXT (use svg|png|pdf)"
    exit 2
    ;;
esac

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing tool: $1"; exit 1; }; }
need bazel
need dot
need gvpr

# Query: local-only by default (filters out @repo//... labels)
# local: keep only workspace labels, drop externals
# all:   keep everything
if [[ "$SCOPE" == "--scope=all" ]]; then
  Q="deps(${TARGET})"
else
  # Option A (strict local): only labels under //...
  Q="deps(${TARGET}) intersect //..."
  # Option B (also local but more explicit about externals):
  # Q=\"let d = deps(${TARGET}) in (d except filter('^@', d))\"
fi

DOT_TMP="$(mktemp /tmp/depgraph.XXXXXX.dot)"
DOT_COLORED="$(mktemp /tmp/depgraph.colored.XXXXXX.dot)"

echo "[depgraph] Querying: $Q"
bazel query --noimplicit_deps --output=graph "$Q" > "$DOT_TMP"

# Color ONLY node boxes (no background clusters)
# Match prefixes and apply node attrs.
awk '
# Only standalone node declarations (no edges)
/^  "/ && !/->/ {

  # default
  style = "[style=\"rounded,filled\", fillcolor=\"#F5F5F5\", color=\"#616161\"]"

  if ($0 ~ /^  "\/\/app/) {
    style = "[style=\"rounded,filled\", fillcolor=\"#CFF5D6\", color=\"#1B5E20\"]"
  }
  else if ($0 ~ /^  "\/\/net/) {
    style = "[style=\"rounded,filled\", fillcolor=\"#E3F2FD\", color=\"#0D47A1\"]"
  }
  else if ($0 ~ /^  "\/\/core/) {
    style = "[style=\"rounded,filled\", fillcolor=\"#FFE0B2\", color=\"#E65100\"]"
  }
  else if ($0 ~ /^  "\/\/domains/) {
    style = "[style=\"rounded,filled\", fillcolor=\"#E0F2F1\", color=\"#004D40\"]"
  }
  else if ($0 ~ /^  "\/\/utils/) {
    style = "[style=\"rounded,filled\", fillcolor=\"#ECEFF1\", color=\"#37474F\"]"
  }
  else if ($0 ~ /^  "\/\/server/) {
    style = "[style=\"rounded,filled\", fillcolor=\"#E6D6FF\", color=\"#4A148C\"]"
  }
  else if ($0 ~ /^  "\/\/infra/) {
  style = "[style=\"rounded,filled\", fillcolor=\"#F3E5F5\", color=\"#6A1B9A\"]"
  }

  print $0 " " style
  next
}

# Everything else (edges, graph header)
{ print }
' "$DOT_TMP" > "$DOT_COLORED"

sed -i '0,/digraph/{s/digraph [^{]*{/digraph mygraph {\n  rankdir=LR;/}' "$DOT_COLORED"

echo "[depgraph] Rendering: $OUT"
dot "-T${EXT}" "$DOT_COLORED" > "$OUT"

echo "[depgraph] Done: $OUT"
