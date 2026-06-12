-- channels.sql
-- Kergit channel table.
--
-- Depends on:
-- - profiles.sql
-- - hubs.sql
-- - kergit_app.profiles
-- - kergit_app.hubs
-- - kergit_app.set_updated_at()
--
-- Assumptions:
-- - Client does not access PostgreSQL directly.
-- - All writes go through server-side application code.
-- - No RLS policy is defined here.
-- - profiles uses app-level soft delete/anonymization.
-- - channels stores current channel state only.
-- - Channel create/update/delete history is stored by server-side application code in audit_events.
-- - Channel deletion is hard-delete.
-- - Message edit/delete is not supported yet.
-- - Voice live state is not stored in PostgreSQL.

BEGIN;

-- ============================================================
-- Channels
-- ============================================================
-- Current state only.
--
-- Channel is created:
-- - server inserts channels row
-- - server writes audit_events: channel.created
--
-- Channel is renamed/reordered:
-- - server updates channels row
-- - server writes audit_events: channel.updated
--
-- Channel is deleted:
-- - server deletes channels row
-- - dependent messages/attachments will later cascade through FK rules
-- - server writes audit_events: channel.deleted
--
-- Account deletion:
-- - channels are not deleted just because a user account is deleted
-- - created_by may remain pointing to an anonymized/deleted profile
--
-- No channel history is stored here.

CREATE TABLE IF NOT EXISTS kergit_app.channels (
  id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  hub_id      uuid NOT NULL REFERENCES kergit_app.hubs(id) ON DELETE CASCADE,

  name        text NOT NULL,
  type        text NOT NULL,
  position    integer NOT NULL DEFAULT 0,

  created_by  uuid REFERENCES kergit_app.profiles(user_id) ON DELETE SET NULL,

  created_at  timestamptz NOT NULL DEFAULT now(),
  updated_at  timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT channels_type_check
    CHECK (type IN ('text', 'voice')),

  CONSTRAINT channels_name_length
    CHECK (
      char_length(btrim(name)) >= 1
      AND char_length(name) <= 80
    ),

  CONSTRAINT channels_position_check
    CHECK (position >= 0)
);

-- Channel names are unique inside one hub.
CREATE UNIQUE INDEX IF NOT EXISTS idx_channels_hub_name_unique
  ON kergit_app.channels(hub_id, lower(name));

CREATE INDEX IF NOT EXISTS idx_channels_hub_id
  ON kergit_app.channels(hub_id);

CREATE INDEX IF NOT EXISTS idx_channels_hub_type
  ON kergit_app.channels(hub_id, type);

CREATE INDEX IF NOT EXISTS idx_channels_hub_position
  ON kergit_app.channels(hub_id, position, created_at);

CREATE INDEX IF NOT EXISTS idx_channels_created_by
  ON kergit_app.channels(created_by);

CREATE TRIGGER trg_channels_updated_at
  BEFORE UPDATE ON kergit_app.channels
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.set_updated_at();

-- Deleted/anonymized users cannot create channels.
CREATE OR REPLACE FUNCTION kergit_app.ensure_channel_creator_active()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_deleted_at timestamptz;
BEGIN
  IF NEW.created_by IS NULL THEN
    RETURN NEW;
  END IF;

  SELECT p.deleted_at
  INTO v_deleted_at
  FROM kergit_app.profiles p
  WHERE p.user_id = NEW.created_by;

  IF v_deleted_at IS NOT NULL THEN
    RAISE EXCEPTION 'Deleted users cannot create channels';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_channels_creator_active
  BEFORE INSERT OR UPDATE OF created_by ON kergit_app.channels
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_channel_creator_active();

-- Channel type is immutable.
--
-- Reason:
-- - changing text -> voice would make existing messages invalid
-- - changing voice -> text changes product behavior silently
-- - if conversion is needed later, it should be explicit app logic
CREATE OR REPLACE FUNCTION kergit_app.prevent_channel_type_change()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
  IF NEW.type <> OLD.type THEN
    RAISE EXCEPTION 'Channel type cannot be changed';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_channels_prevent_type_change
  BEFORE UPDATE OF type ON kergit_app.channels
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.prevent_channel_type_change();

-- Alpha limit: each hub can have at most 3 channels.
CREATE OR REPLACE FUNCTION kergit_app.enforce_max_channels_per_hub()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_count integer;
BEGIN
  SELECT count(*)
  INTO v_count
  FROM kergit_app.channels c
  WHERE c.hub_id = NEW.hub_id;

  IF v_count >= 3 THEN
    RAISE EXCEPTION 'Channel limit reached: max 3 channels per hub';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_channels_limit
  BEFORE INSERT ON kergit_app.channels
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.enforce_max_channels_per_hub();

-- No direct client DB access.
REVOKE ALL ON TABLE kergit_app.channels FROM anon, authenticated;

COMMIT;
