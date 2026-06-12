-- hubs.sql
-- Kergit hub, role and membership tables.
--
-- Depends on:
-- - profiles.sql
-- - kergit_app.profiles
-- - kergit_app.set_updated_at()
--
-- Assumptions:
-- - Client does not access PostgreSQL directly.
-- - All writes go through server-side application code.
-- - No RLS policy is defined here.
-- - profiles uses app-level soft delete/anonymization.
-- - hub_members stores current membership only.
-- - Join/leave/kick/history events are stored by server-side application code in audit_events.
-- - Hub deletion is a destructive owner action and hard-deletes the hub.
-- - Account deletion is blocked while the user owns hubs.
-- - Account deletion removes current hub memberships explicitly.
-- - Owner transfer is not implemented yet.

BEGIN;

-- ============================================================
-- Hub roles
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.hub_roles (
  id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  name        text NOT NULL UNIQUE,
  description text,
  priority    integer NOT NULL,

  is_system   boolean NOT NULL DEFAULT false,

  created_at  timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT hub_roles_name_length
    CHECK (
      char_length(btrim(name)) >= 1
      AND char_length(name) <= 40
    ),

  CONSTRAINT hub_roles_priority_check
    CHECK (priority >= 0)
);

INSERT INTO kergit_app.hub_roles (
  name,
  description,
  priority,
  is_system
)
VALUES
  ('owner',  'Hub owner. Full control.', 100, true),
  ('admin',  'Hub administrator.',       70,  true),
  ('member', 'Regular hub member.',      10,  true)
ON CONFLICT (name) DO UPDATE
SET
  description = EXCLUDED.description,
  priority = EXCLUDED.priority,
  is_system = EXCLUDED.is_system;

CREATE OR REPLACE FUNCTION kergit_app.get_hub_role_id(p_role_name text)
RETURNS uuid
LANGUAGE sql
STABLE
AS $$
  SELECT id
  FROM kergit_app.hub_roles
  WHERE name = p_role_name
  LIMIT 1
$$;

-- ============================================================
-- Hubs
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.hubs (
  id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  name        text NOT NULL,
  avatar_seed text NOT NULL DEFAULT 'Felix',

  owner_id    uuid NOT NULL REFERENCES kergit_app.profiles(user_id) ON DELETE RESTRICT,
  created_by  uuid REFERENCES kergit_app.profiles(user_id) ON DELETE SET NULL,

  created_at  timestamptz NOT NULL DEFAULT now(),
  updated_at  timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT hubs_name_length
    CHECK (
      char_length(btrim(name)) >= 1
      AND char_length(name) <= 80
    ),

  CONSTRAINT hubs_avatar_seed_length
    CHECK (
      avatar_seed IS NULL
      OR char_length(avatar_seed) <= 128
    )
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_hubs_owner_name_unique
  ON kergit_app.hubs(owner_id, lower(name));

CREATE INDEX IF NOT EXISTS idx_hubs_owner_id
  ON kergit_app.hubs(owner_id);

CREATE INDEX IF NOT EXISTS idx_hubs_created_at
  ON kergit_app.hubs(created_at DESC);

CREATE TRIGGER trg_hubs_updated_at
  BEFORE UPDATE ON kergit_app.hubs
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.set_updated_at();

-- Deleted/anonymized users cannot create or own hubs.
CREATE OR REPLACE FUNCTION kergit_app.ensure_hub_owner_active()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_deleted_at timestamptz;
BEGIN
  SELECT p.deleted_at
  INTO v_deleted_at
  FROM kergit_app.profiles p
  WHERE p.user_id = NEW.owner_id;

  IF v_deleted_at IS NOT NULL THEN
    RAISE EXCEPTION 'Deleted users cannot own hubs';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_hubs_owner_active
  BEFORE INSERT OR UPDATE OF owner_id ON kergit_app.hubs
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_hub_owner_active();

-- Owner transfer is not supported yet.
-- If this feature is added later, transfer must be explicit app logic:
-- - update hubs.owner_id
-- - update old/new hub_members roles
-- - write audit event
CREATE OR REPLACE FUNCTION kergit_app.prevent_hub_owner_change()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
  IF NEW.owner_id <> OLD.owner_id THEN
    RAISE EXCEPTION
      'Hub ownership transfer is currently disabled';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_hubs_prevent_owner_change
  BEFORE UPDATE OF owner_id ON kergit_app.hubs
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.prevent_hub_owner_change();

-- Alpha limit: each owner can create at most 2 hubs.
CREATE OR REPLACE FUNCTION kergit_app.enforce_max_hubs_per_owner()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_count integer;
BEGIN
  SELECT count(*)
  INTO v_count
  FROM kergit_app.hubs h
  WHERE h.owner_id = NEW.owner_id;

  IF v_count >= 2 THEN
    RAISE EXCEPTION 'Hub ownership limit reached: max 2 hubs per owner';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_hubs_limit
  BEFORE INSERT ON kergit_app.hubs
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.enforce_max_hubs_per_owner();

-- ============================================================
-- Hub members
-- ============================================================
-- Current membership only.
--
-- User joins hub:
-- - server inserts hub_members row
-- - server writes audit_events: member.joined
--
-- User leaves hub:
-- - server deletes hub_members row
-- - server writes audit_events: member.left
--
-- Admin kicks user:
-- - server deletes hub_members row
-- - server writes audit_events: member.kicked
--
-- Account deletion:
-- - if user owns hubs, deletion is blocked
-- - otherwise server deletes hub_members rows for that user
-- - profile is anonymized/soft-deleted
--
-- Hub is deleted:
-- - hub_members rows are deleted by cascade
-- - server writes audit_events: hub.deleted
--
-- No membership history is stored here.

CREATE TABLE IF NOT EXISTS kergit_app.hub_members (
  hub_id     uuid NOT NULL REFERENCES kergit_app.hubs(id) ON DELETE CASCADE,
  user_id    uuid NOT NULL REFERENCES kergit_app.profiles(user_id) ON DELETE CASCADE,
  role_id    uuid NOT NULL REFERENCES kergit_app.hub_roles(id) ON DELETE RESTRICT,

  invited_by uuid REFERENCES kergit_app.profiles(user_id) ON DELETE SET NULL,

  joined_at  timestamptz NOT NULL DEFAULT now(),
  updated_at timestamptz NOT NULL DEFAULT now(),

  PRIMARY KEY (hub_id, user_id)
);

CREATE INDEX IF NOT EXISTS idx_hub_members_user_id
  ON kergit_app.hub_members(user_id);

CREATE INDEX IF NOT EXISTS idx_hub_members_role_id
  ON kergit_app.hub_members(role_id);

CREATE INDEX IF NOT EXISTS idx_hub_members_joined_at
  ON kergit_app.hub_members(joined_at DESC);

CREATE TRIGGER trg_hub_members_updated_at
  BEFORE UPDATE ON kergit_app.hub_members
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.set_updated_at();

-- Deleted/anonymized users cannot be current hub members.
CREATE OR REPLACE FUNCTION kergit_app.ensure_hub_member_active()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_deleted_at timestamptz;
BEGIN
  SELECT p.deleted_at
  INTO v_deleted_at
  FROM kergit_app.profiles p
  WHERE p.user_id = NEW.user_id;

  IF v_deleted_at IS NOT NULL THEN
    RAISE EXCEPTION 'Deleted users cannot be hub members';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_hub_members_user_active
  BEFORE INSERT OR UPDATE OF user_id ON kergit_app.hub_members
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_hub_member_active();

-- Owner automatically becomes hub member with owner role.
CREATE OR REPLACE FUNCTION kergit_app.ensure_owner_membership()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_owner_role_id uuid;
BEGIN
  v_owner_role_id := kergit_app.get_hub_role_id('owner');

  IF v_owner_role_id IS NULL THEN
    RAISE EXCEPTION 'Missing required hub role: owner';
  END IF;

  INSERT INTO kergit_app.hub_members (
    hub_id,
    user_id,
    role_id,
    invited_by
  )
  VALUES (
    NEW.id,
    NEW.owner_id,
    v_owner_role_id,
    NULL
  )
  ON CONFLICT (hub_id, user_id) DO UPDATE
  SET
    role_id = EXCLUDED.role_id,
    updated_at = now();

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_hubs_owner_membership
  AFTER INSERT ON kergit_app.hubs
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_owner_membership();

-- Only hubs.owner_id can have owner role.
CREATE OR REPLACE FUNCTION kergit_app.prevent_invalid_owner_role()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_owner_role_id uuid;
  v_hub_owner_id uuid;
BEGIN
  v_owner_role_id := kergit_app.get_hub_role_id('owner');

  IF NEW.role_id = v_owner_role_id THEN
    SELECT h.owner_id
    INTO v_hub_owner_id
    FROM kergit_app.hubs h
    WHERE h.id = NEW.hub_id;

    IF v_hub_owner_id IS NULL THEN
      RAISE EXCEPTION 'Hub does not exist';
    END IF;

    IF NEW.user_id <> v_hub_owner_id THEN
      RAISE EXCEPTION 'Only hub owner can have owner role';
    END IF;
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_hub_members_prevent_invalid_owner_role
  BEFORE INSERT OR UPDATE OF role_id, user_id, hub_id ON kergit_app.hub_members
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.prevent_invalid_owner_role();


-- ============================================================
-- Profile soft-delete membership cleanup
-- ============================================================
-- Soft-deleted users must not remain active hub members.
--
-- Important:
-- - ON DELETE CASCADE does not run for soft delete.
-- - Account deletion sets profiles.deleted_at.
-- - Therefore membership cleanup must happen explicitly.
--
-- Behavior:
-- - if the user owns any hub, soft delete is blocked
-- - otherwise all hub_members rows for that user are removed
--
-- The Nuxt server should update account_deletion_requests.memberships_removed_at
-- after the profile anonymization update succeeds.

CREATE OR REPLACE FUNCTION kergit_app.handle_profile_soft_delete_for_hubs()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
  IF OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL THEN
    IF EXISTS (
      SELECT 1
      FROM kergit_app.hubs h
      WHERE h.owner_id = NEW.user_id
    ) THEN
      RAISE EXCEPTION 'Account deletion blocked: user owns hubs';
    END IF;

    DELETE FROM kergit_app.hub_members hm
    WHERE hm.user_id = NEW.user_id;
  END IF;

  RETURN NEW;
END
$$;

DROP TRIGGER IF EXISTS trg_profiles_soft_delete_hub_cleanup
  ON kergit_app.profiles;

CREATE TRIGGER trg_profiles_soft_delete_hub_cleanup
  BEFORE UPDATE OF deleted_at ON kergit_app.profiles
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.handle_profile_soft_delete_for_hubs();

-- No direct client DB access.
REVOKE ALL ON TABLE kergit_app.hub_roles   FROM anon, authenticated;
REVOKE ALL ON TABLE kergit_app.hubs        FROM anon, authenticated;
REVOKE ALL ON TABLE kergit_app.hub_members FROM anon, authenticated;
REVOKE ALL ON FUNCTION kergit_app.handle_profile_soft_delete_for_hubs()
FROM anon, authenticated;

COMMIT;
