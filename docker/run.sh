#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "[+] Starting container..."
docker compose up -d

echo "[✓] Container started."
docker compose ps
