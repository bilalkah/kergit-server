#!/usr/bin/env bash
set -euo pipefail

CHAT_USER=${CHAT_USER:-chat_user}
CHAT_PW=${CHAT_PW:-12345678}
CHAT_DB=${CHAT_DB:-chat_db}
CHAT_HOST=${CHAT_HOST:-localhost}
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../../.. && pwd)"
SCHEMA_PATH="$REPO_ROOT/core/database/sql/database_schema.sql"

export PGPASSWORD="$CHAT_PW"

echo "Terminating active connections to $CHAT_DB..."
psql -h "$CHAT_HOST" -U "$CHAT_USER" -d postgres -v ON_ERROR_STOP=1 -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname='${CHAT_DB}' AND pid <> pg_backend_pid();" || true

echo "Dropping database $CHAT_DB (if exists)..."
dropdb -h "$CHAT_HOST" -U "$CHAT_USER" --if-exists "$CHAT_DB" || true

echo "Creating database $CHAT_DB..."
createdb -h "$CHAT_HOST" -U "$CHAT_USER" "$CHAT_DB" || true

echo "Applying schema..."
"$REPO_ROOT/bazel-bin/core/database/chatdb_init" --conninfo="postgresql://$CHAT_USER:$CHAT_PW@$CHAT_HOST/$CHAT_DB" --sql="$SCHEMA_PATH"

echo "Done." 