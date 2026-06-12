CREATE SCHEMA IF NOT EXISTS kergit_app;

CREATE EXTENSION IF NOT EXISTS pgcrypto;

REVOKE ALL ON SCHEMA kergit_app FROM anon, authenticated;
GRANT USAGE ON SCHEMA kergit_app TO service_role;