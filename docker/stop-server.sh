#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "[+] Stopping and removing container..."
docker compose down

echo "[✓] Container stopped and removed."
