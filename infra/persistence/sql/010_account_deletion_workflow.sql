-- 010_account_deletion_workflow.sql
-- Server-driven account deletion workflow for the kergit_app schema.
--
-- Depends on:
-- - 001_profiles.sql              (kergit_app.profiles)
-- - 002_hubs.sql                  (kergit_app.hubs, kergit_app.hub_members)
-- - 006_audit_events.sql          (kergit_app.audit_events)
-- - 007_account_deletion_requests.sql
--
-- Design:
-- - Account deletion is an auth/account-management flow owned by the Nuxt
--   server endpoint (clients/web/server/api/auth/delete-account.post.ts).
-- - The C++ runtime server is NOT involved and is not ready for kergit_app yet.
-- - The DB-side workflow runs as SECURITY DEFINER functions so the destructive
--   steps (reserve email, anonymize profile, remove memberships, audit) commit
--   atomically inside one PostgreSQL transaction.
-- - The Supabase Auth Admin soft-delete cannot run inside a DB transaction, so
--   the endpoint calls request_account_deletion() first, then the Admin API,
--   then complete_account_deletion() / fail_account_deletion().
--
-- Privacy:
-- - The deleted-email reservation stores only a one-way HMAC/SHA-256 hex digest,
--   never the plain email address.
-- - Audit metadata stays compact and non-sensitive (no email, tokens, cookies,
--   request bodies, message content, or file content).
--
-- Message history:
-- - Historical messages are intentionally preserved. The sender profile is
--   anonymized so old messages render as a deleted/anonymized user instead of
--   being rewritten or removed.

BEGIN;

-- ============================================================
-- Reconcile deleted-email reservation table name
-- ============================================================
-- Earlier schema (007) created kergit_app.account_email_reservations.
-- This workflow uses the deletion-specific name kergit_app.account_deleted_email_reservations.
-- Rename in place when needed; non-destructive and idempotent.

DO $$
BEGIN
  IF to_regclass('kergit_app.account_email_reservations') IS NOT NULL
     AND to_regclass('kergit_app.account_deleted_email_reservations') IS NULL THEN
    ALTER TABLE kergit_app.account_email_reservations
      RENAME TO account_deleted_email_reservations;

    DROP INDEX IF EXISTS kergit_app.idx_account_email_reservations_email_hash_unique;
    DROP INDEX IF EXISTS kergit_app.idx_account_email_reservations_first_user_id;
  END IF;

  IF to_regclass('kergit_app.account_deleted_email_reservations') IS NOT NULL
     AND EXISTS (
       SELECT 1
       FROM information_schema.columns
       WHERE table_schema = 'kergit_app'
         AND table_name = 'account_deleted_email_reservations'
         AND column_name = 'first_user_id'
     ) THEN
    ALTER TABLE kergit_app.account_deleted_email_reservations
      RENAME COLUMN first_user_id TO subject_user_id;
  END IF;
END
$$;

-- Fresh-install safety: create the table if neither name existed.
CREATE TABLE IF NOT EXISTS kergit_app.account_deleted_email_reservations (
  id              uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  email_hash      text NOT NULL,
  subject_user_id uuid,
  created_at      timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT account_deleted_email_reservations_email_hash_hex
    CHECK (email_hash ~ '^[0-9a-f]{64}$')
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_account_deleted_email_reservations_email_hash_unique
  ON kergit_app.account_deleted_email_reservations(email_hash);

CREATE INDEX IF NOT EXISTS idx_account_deleted_email_reservations_subject_user_id
  ON kergit_app.account_deleted_email_reservations(subject_user_id);

REVOKE ALL ON TABLE kergit_app.account_deleted_email_reservations FROM anon, authenticated;

-- ============================================================
-- request_account_deletion
-- ============================================================
-- Atomic begin step.
-- - creates an account_deletion_requests row
-- - blocks deletion when the user still owns hubs
-- - otherwise reserves the deleted email, removes hub memberships,
--   anonymizes the profile, and records audit events
--
-- Returns jsonb: { request_id, status, owned_hub_count }
--   status = 'blocked'    -> caller must return 409, no auth delete
--   status = 'anonymized' -> caller proceeds to Supabase Auth soft delete

CREATE OR REPLACE FUNCTION kergit_app.request_account_deletion(
  p_user_id        uuid,
  p_email_hash     text,
  p_request_id     text DEFAULT NULL,
  p_session_id     text DEFAULT NULL,
  p_connection_id  text DEFAULT NULL,
  p_server_node_id text DEFAULT NULL
)
RETURNS jsonb
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = ''
AS $$
DECLARE
  v_deletion_id     uuid;
  v_owned_hub_count integer;
  v_deleted_name    text;
BEGIN
  IF p_user_id IS NULL THEN
    RAISE EXCEPTION 'Subject user id is required';
  END IF;

  IF p_email_hash IS NULL OR p_email_hash !~ '^[0-9a-f]{64}$' THEN
    RAISE EXCEPTION 'Email hash must be a 64-character hex digest';
  END IF;

  INSERT INTO kergit_app.account_deletion_requests (
    subject_user_id,
    requested_by_user_id,
    status,
    started_at
  )
  VALUES (
    p_user_id,
    p_user_id,
    'running',
    now()
  )
  RETURNING id INTO v_deletion_id;

  INSERT INTO kergit_app.audit_events (
    category, event_type, actor_type, actor_user_id, target_user_id,
    request_id, session_id, connection_id, server_node_id
  )
  VALUES (
    'account', 'account.delete_requested', 'user', p_user_id, p_user_id,
    p_request_id, p_session_id, p_connection_id, p_server_node_id
  );

  SELECT count(*)
  INTO v_owned_hub_count
  FROM kergit_app.hubs
  WHERE owner_id = p_user_id;

  IF v_owned_hub_count > 0 THEN
    UPDATE kergit_app.account_deletion_requests
    SET status = 'blocked',
        blocked_at = now(),
        metadata = jsonb_build_object('owned_hub_count', v_owned_hub_count)
    WHERE id = v_deletion_id;

    INSERT INTO kergit_app.audit_events (
      category, event_type, severity, actor_type, actor_user_id, target_user_id,
      request_id, session_id, connection_id, server_node_id, metadata
    )
    VALUES (
      'account', 'account.delete_blocked_owned_hubs', 'warning', 'user', p_user_id, p_user_id,
      p_request_id, p_session_id, p_connection_id, p_server_node_id,
      jsonb_build_object('owned_hub_count', v_owned_hub_count)
    );

    RETURN jsonb_build_object(
      'request_id', v_deletion_id,
      'status', 'blocked',
      'owned_hub_count', v_owned_hub_count
    );
  END IF;

  -- Reserve the deleted email (idempotent). Only the hash is stored.
  INSERT INTO kergit_app.account_deleted_email_reservations (email_hash, subject_user_id)
  VALUES (p_email_hash, p_user_id)
  ON CONFLICT (email_hash) DO NOTHING;

  INSERT INTO kergit_app.audit_events (
    category, event_type, actor_type, actor_user_id, target_user_id,
    request_id, session_id, connection_id, server_node_id
  )
  VALUES (
    'account', 'account.email_reserved_for_deleted_account', 'user', p_user_id, p_user_id,
    p_request_id, p_session_id, p_connection_id, p_server_node_id
  );

  -- Remove current hub memberships. Soft-deleted profiles must not stay members.
  DELETE FROM kergit_app.hub_members
  WHERE user_id = p_user_id;

  UPDATE kergit_app.account_deletion_requests
  SET memberships_removed_at = now()
  WHERE id = v_deletion_id;

  INSERT INTO kergit_app.audit_events (
    category, event_type, actor_type, actor_user_id, target_user_id,
    request_id, session_id, connection_id, server_node_id
  )
  VALUES (
    'account', 'account.memberships_removed', 'user', p_user_id, p_user_id,
    p_request_id, p_session_id, p_connection_id, p_server_node_id
  );

  -- Anonymize the profile. Historical messages are preserved and keep
  -- referencing this now-anonymized sender.
  v_deleted_name := 'deleted_' || replace(p_user_id::text, '-', '');

  UPDATE kergit_app.profiles
  SET user_name = v_deleted_name,
      display_name = 'Silinmiş Kullanıcı',
      avatar_seed = '',
      last_seen_at = NULL,
      deleted_at = now()
  WHERE user_id = p_user_id;

  UPDATE kergit_app.account_deletion_requests
  SET profile_anonymized_at = now()
  WHERE id = v_deletion_id;

  INSERT INTO kergit_app.audit_events (
    category, event_type, actor_type, actor_user_id, target_user_id,
    request_id, session_id, connection_id, server_node_id
  )
  VALUES (
    'account', 'account.profile_anonymized', 'user', p_user_id, p_user_id,
    p_request_id, p_session_id, p_connection_id, p_server_node_id
  );

  RETURN jsonb_build_object(
    'request_id', v_deletion_id,
    'status', 'anonymized',
    'owned_hub_count', 0
  );
END
$$;

-- ============================================================
-- complete_account_deletion
-- ============================================================
-- Final success step after Supabase Auth soft delete succeeds.

CREATE OR REPLACE FUNCTION kergit_app.complete_account_deletion(
  p_deletion_id    uuid,
  p_user_id        uuid,
  p_request_id     text DEFAULT NULL,
  p_server_node_id text DEFAULT NULL
)
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = ''
AS $$
BEGIN
  UPDATE kergit_app.account_deletion_requests
  SET status = 'completed',
      auth_soft_deleted_at = now(),
      finished_at = now()
  WHERE id = p_deletion_id;

  INSERT INTO kergit_app.audit_events (
    category, event_type, actor_type, actor_user_id, target_user_id,
    request_id, server_node_id
  )
  VALUES
    ('account', 'account.auth_soft_deleted', 'system', p_user_id, p_user_id,
     p_request_id, p_server_node_id),
    ('account', 'account.delete_completed', 'system', p_user_id, p_user_id,
     p_request_id, p_server_node_id);
END
$$;

-- ============================================================
-- fail_account_deletion
-- ============================================================
-- Failure step when Supabase Auth soft delete fails after anonymization.
-- failure_reason must already be a user-safe, non-sensitive string.

CREATE OR REPLACE FUNCTION kergit_app.fail_account_deletion(
  p_deletion_id    uuid,
  p_user_id        uuid,
  p_error_code     text,
  p_failure_reason text,
  p_request_id     text DEFAULT NULL,
  p_server_node_id text DEFAULT NULL
)
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = ''
AS $$
BEGIN
  UPDATE kergit_app.account_deletion_requests
  SET status = 'failed',
      error_code = p_error_code,
      failure_reason = p_failure_reason,
      finished_at = now()
  WHERE id = p_deletion_id;

  INSERT INTO kergit_app.audit_events (
    category, event_type, severity, actor_type, actor_user_id, target_user_id,
    request_id, server_node_id, error_code, metadata
  )
  VALUES (
    'account', 'account.delete_failed', 'error', 'system', p_user_id, p_user_id,
    p_request_id, p_server_node_id, p_error_code,
    jsonb_build_object('error_code', p_error_code)
  );
END
$$;

-- ============================================================
-- is_email_reserved
-- ============================================================
-- Signup precheck: returns true when a normalized-email HMAC/SHA-256 hex
-- digest already belongs to a deleted account. Active duplicate handling
-- stays Supabase Auth's responsibility.

CREATE OR REPLACE FUNCTION kergit_app.is_email_reserved(p_email_hash text)
RETURNS boolean
LANGUAGE sql
SECURITY DEFINER
SET search_path = ''
AS $$
  SELECT EXISTS (
    SELECT 1
    FROM kergit_app.account_deleted_email_reservations
    WHERE email_hash = p_email_hash
  );
$$;

-- ============================================================
-- Permissions
-- ============================================================
-- Reachable by the service-role server client only.

REVOKE ALL ON FUNCTION kergit_app.request_account_deletion(uuid, text, text, text, text, text) FROM anon, authenticated;
REVOKE ALL ON FUNCTION kergit_app.complete_account_deletion(uuid, uuid, text, text) FROM anon, authenticated;
REVOKE ALL ON FUNCTION kergit_app.fail_account_deletion(uuid, uuid, text, text, text, text) FROM anon, authenticated;
REVOKE ALL ON FUNCTION kergit_app.is_email_reserved(text) FROM anon, authenticated;

GRANT EXECUTE ON FUNCTION kergit_app.request_account_deletion(uuid, text, text, text, text, text) TO service_role;
GRANT EXECUTE ON FUNCTION kergit_app.complete_account_deletion(uuid, uuid, text, text) TO service_role;
GRANT EXECUTE ON FUNCTION kergit_app.fail_account_deletion(uuid, uuid, text, text, text, text) TO service_role;
GRANT EXECUTE ON FUNCTION kergit_app.is_email_reserved(text) TO service_role;

COMMIT;
