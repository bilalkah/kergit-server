#!/usr/bin/env bash
set -euo pipefail

SQL_FILE="${1:-core/database/sql/V001__init.sql}"

# ---- config (override via env) ----
DB_NAME="${CHAT_DB_NAME:-chat_db}"
DB_USER="${CHAT_DB_USER:-chat_user}"
DB_PASS="${CHAT_DB_PASSWORD:-12345678}"

# Optionally pin your custom PostgreSQL bin dir (so Bazel/sudo PATH isn’t an issue)
PG_BIN_DIR="${PG_BIN_DIR:-$HOME/repos/pg/project5_3/install/bin}"
PSQL_BIN="${PSQL_BIN:-$PG_BIN_DIR/psql}"
CREATEDB_BIN="${CREATEDB_BIN:-$PG_BIN_DIR/createdb}"

# Fallback to PATH if those don’t exist
command -v "$PSQL_BIN" >/dev/null 2>&1 || PSQL_BIN="$(command -v psql)"
command -v "$CREATEDB_BIN" >/dev/null 2>&1 || CREATEDB_BIN="$(command -v createdb)"

if [[ -z "$PSQL_BIN" || -z "$CREATEDB_BIN" ]]; then
  echo "psql/createdb not found. Set PG_BIN_DIR or PSQL_BIN/CREATEDB_BIN." >&2
  exit 1
fi

# Which DB/user to run admin commands as (must have perms to create roles/dbs)
ADMIN_DB="${ADMIN_DB:-postgres}"
ADMIN_FLAGS=()    # e.g. ADMIN_FLAGS=(-h localhost -p 5432 -U postgres)
# If you need a specific admin user: export ADMIN_FLAGS=(-U postgres)

# ---- 1) create role if missing ----
ROLE_EXISTS=$("$PSQL_BIN" "${ADMIN_FLAGS[@]}" -tAc \
  "SELECT 1 FROM pg_roles WHERE rolname = '${DB_USER}'" "$ADMIN_DB" || echo "")
if [[ "$ROLE_EXISTS" != "1" ]]; then
  # quote password safely
  PW_LIT=$("$PSQL_BIN" "${ADMIN_FLAGS[@]}" -Atc "SELECT quote_literal('${DB_PASS}')" "$ADMIN_DB")
  "$PSQL_BIN" "${ADMIN_FLAGS[@]}" -v ON_ERROR_STOP=1 -d "$ADMIN_DB" -c \
    "CREATE ROLE ${DB_USER} LOGIN INHERIT CREATEROLE CREATEDB PASSWORD ${PW_LIT};"
fi

# ---- 2) create database if missing (MUST be top-level, not in DO $$) ----
DB_EXISTS=$("$PSQL_BIN" "${ADMIN_FLAGS[@]}" -tAc \
  "SELECT 1 FROM pg_database WHERE datname = '${DB_NAME}'" "$ADMIN_DB" || echo "")
if [[ "$DB_EXISTS" != "1" ]]; then
  "$CREATEDB_BIN" "${ADMIN_FLAGS[@]}" -O "$DB_USER" "$DB_NAME"
  # Alternatively (also top-level): "$PSQL_BIN" "${ADMIN_FLAGS[@]}" -d "$ADMIN_DB" -c "CREATE DATABASE ${DB_NAME} OWNER ${DB_USER};"
fi

# ---- 3) apply baseline migration as app user ----
export PGPASSWORD="${DB_PASS}"
"$PSQL_BIN" -v ON_ERROR_STOP=1 -U "${DB_USER}" -d "${DB_NAME}" -f "${SQL_FILE}"

echo "✅ Dev DB ready: ${DB_NAME} (owner: ${DB_USER}) using ${SQL_FILE}"
