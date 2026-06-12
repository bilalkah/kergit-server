-- profiles.sql
-- Kergit profile table.
--
-- Assumptions:
-- - Supabase Auth remains the identity source via auth.users.
-- - Client does not access PostgreSQL directly.
-- - All reads/writes go through server-side application code.
-- - No RLS policy is defined here.
-- - This table stores app-visible user profile data, not auth credentials.
-- - Account deletion uses app-level soft delete/anonymization.
-- - Deleted profiles are anonymized instead of being physically deleted.
-- - Messages may continue to reference deleted/anonymized profiles.

BEGIN;

-- ============================================================
-- Helpers
-- ============================================================

CREATE OR REPLACE FUNCTION kergit_app.set_updated_at()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
  NEW.updated_at := now();
  RETURN NEW;
END
$$;

-- ============================================================
-- Profiles
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.profiles (
  user_id       uuid PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,

  user_name     text NOT NULL,
  display_name  text NOT NULL,

  avatar_seed   text NOT NULL DEFAULT 'Caleb',

  last_seen_at  timestamptz,
  deleted_at    timestamptz,

  created_at    timestamptz NOT NULL DEFAULT now(),
  updated_at    timestamptz NOT NULL DEFAULT now(),

  -- Active usernames are limited to 32 chars.
  -- Deleted/anonymized usernames may be longer, for example:
  -- deleted_<uuid_without_dashes>
  CONSTRAINT profiles_user_name_length
    CHECK (
      (
        deleted_at IS NULL
        AND char_length(btrim(user_name)) >= 3
        AND char_length(user_name) <= 32
      )
      OR
      (
        deleted_at IS NOT NULL
        AND char_length(btrim(user_name)) >= 3
        AND char_length(user_name) <= 64
      )
    ),

  CONSTRAINT profiles_user_name_format
    CHECK (
      user_name = lower(user_name)
      AND user_name ~ '^[a-z0-9_][a-z0-9_.-]*[a-z0-9_]$'
    ),

  CONSTRAINT profiles_display_name_length
    CHECK (
      char_length(btrim(display_name)) >= 1
      AND char_length(display_name) <= 40
    ),

  CONSTRAINT profiles_avatar_seed_length
    CHECK (
      avatar_seed IS NULL
      OR char_length(avatar_seed) <= 128
    )
);

-- Case-insensitive uniqueness for usernames.
-- During account deletion, the server should first anonymize user_name,
-- so the old username becomes reusable.
CREATE UNIQUE INDEX IF NOT EXISTS idx_profiles_user_name_unique
  ON kergit_app.profiles(lower(user_name));

CREATE INDEX IF NOT EXISTS idx_profiles_last_seen_at
  ON kergit_app.profiles(last_seen_at DESC);

CREATE INDEX IF NOT EXISTS idx_profiles_deleted_at
  ON kergit_app.profiles(deleted_at)
  WHERE deleted_at IS NOT NULL;

CREATE TRIGGER trg_profiles_updated_at
  BEFORE UPDATE ON kergit_app.profiles
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.set_updated_at();

-- No direct client DB access.
REVOKE ALL ON TABLE kergit_app.profiles FROM anon, authenticated;

COMMIT;
