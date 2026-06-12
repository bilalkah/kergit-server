#!/usr/bin/env bash
set -euo pipefail

FORCE=0

for arg in "$@"; do
  case "$arg" in
    --force)
      FORCE=1
      ;;
    *)
      echo "❌ Unknown argument: $arg"
      echo "Usage: $0 [--force]"
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CERTS_DIR="$REPO_ROOT/certs"
DEV_CERTS_DIR="$CERTS_DIR/dev"
DEV_CERT_PATH="$DEV_CERTS_DIR/localhost.pem"
DEV_KEY_PATH="$DEV_CERTS_DIR/localhost-key.pem"
COMPAT_CERT_PATH="$CERTS_DIR/cert.pem"
COMPAT_KEY_PATH="$CERTS_DIR/key.pem"

mkdir -p "$DEV_CERTS_DIR"

if ! command -v mkcert >/dev/null 2>&1; then
  if [ -f "$DEV_CERT_PATH" ] && [ -f "$DEV_KEY_PATH" ]; then
    echo "⚠️ mkcert is not installed. Reusing existing dev cert files:"
    echo "   $DEV_CERT_PATH"
    echo "   $DEV_KEY_PATH"
  elif [ -f "$COMPAT_CERT_PATH" ] && [ -f "$COMPAT_KEY_PATH" ]; then
    echo "⚠️ mkcert is not installed. Falling back to existing compatibility certs."
    cp "$COMPAT_CERT_PATH" "$DEV_CERT_PATH"
    cp "$COMPAT_KEY_PATH" "$DEV_KEY_PATH"
    chmod 0644 "$DEV_CERT_PATH"
    chmod 0600 "$DEV_KEY_PATH"
  else
    echo "❌ mkcert is not installed and no fallback certs are available."
    echo "Install mkcert first, then run:"
    echo "  mkcert -install"
    echo "  $0"
    exit 1
  fi
fi

if command -v mkcert >/dev/null 2>&1 && \
   { [ "$FORCE" -eq 1 ] || [ ! -f "$DEV_CERT_PATH" ] || [ ! -f "$DEV_KEY_PATH" ]; }; then
  echo "▶ Generating localhost development certificate via mkcert..."
  mkcert -cert-file "$DEV_CERT_PATH" -key-file "$DEV_KEY_PATH" localhost 127.0.0.1 ::1
elif [ -f "$DEV_CERT_PATH" ] && [ -f "$DEV_KEY_PATH" ]; then
  echo "▶ Reusing existing development certificate: $DEV_CERT_PATH"
fi

# Keep legacy cert aliases in place for components that still use certs/cert.pem + certs/key.pem.
# Avoid rewriting tracked files on every run; only materialize aliases when absent.
if [ ! -f "$COMPAT_CERT_PATH" ] || [ ! -f "$COMPAT_KEY_PATH" ]; then
  cp "$DEV_CERT_PATH" "$COMPAT_CERT_PATH"
  cp "$DEV_KEY_PATH" "$COMPAT_KEY_PATH"
  chmod 0644 "$COMPAT_CERT_PATH"
  chmod 0600 "$COMPAT_KEY_PATH"
else
  echo "▶ Reusing compatibility cert aliases:"
  echo "   $COMPAT_CERT_PATH"
  echo "   $COMPAT_KEY_PATH"
fi

echo "✅ Dev TLS cert ready:"
echo "   cert: $DEV_CERT_PATH"
echo "   key:  $DEV_KEY_PATH"
echo "   compat aliases: $COMPAT_CERT_PATH, $COMPAT_KEY_PATH"
