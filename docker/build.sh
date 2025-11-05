#!/usr/bin/env bash
set -e

# Go to the script’s directory (docker/)
cd "$(dirname "$0")"

echo "[+] Building Docker image..."
docker compose build

echo "[✓] Build complete."

