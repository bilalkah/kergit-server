-- audit_events.sql
-- Kergit product-level audit and technical event logs.
--
-- Depends on:
-- - pgcrypto extension already available
--
-- Assumptions:
-- - Client does not access PostgreSQL directly.
-- - Audit writes are performed by server-side application code.
--   This can be the C++ server or Nuxt server auth endpoint.
-- - No RLS policy is defined here.
-- - This table stores technical/security/product events.
-- - This table intentionally avoids foreign keys.
--   Deleted/anonymized users, hubs, channels, messages or attachments
--   should not break short-lived audit records.
-- - Account deletion uses app-level soft delete/anonymization.
-- - Audit records may reference anonymized/deleted users by raw UUID.
-- - This table does NOT store IP addresses, user agents, message content,
--   file content, email addresses, passwords, access tokens, refresh tokens,
--   cookies, full request bodies, full headers, or arbitrary client payloads.
--
-- Retention:
-- - Audit events are purged by occurred_at.
-- - Default purge policy is 30 days.
-- - Legal proof should live in legal proof tables, not here.

BEGIN;

-- ============================================================
-- Audit events
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.audit_events (
  id              uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  occurred_at     timestamptz NOT NULL DEFAULT now(),

  category        text NOT NULL,
  event_type      text NOT NULL,
  severity        text NOT NULL DEFAULT 'info',

  actor_type      text NOT NULL DEFAULT 'user',
  actor_user_id   uuid,
  target_user_id  uuid,

  hub_id          uuid,
  channel_id      uuid,
  message_id      uuid,
  attachment_id   uuid,
  invite_id       uuid,

  -- Internal correlation identifiers only.
  -- Do not store auth tokens, cookies, raw session secrets, IP addresses,
  -- user agents, or request bodies in these fields.
  request_id      text,
  session_id      text,
  connection_id   text,

  server_node_id  text,

  error_code      text,
  status_code     integer,

  -- Keep metadata compact and non-sensitive.
  -- Good examples:
  --   {"reason":"rate_limit"}
  --   {"old_role":"member","new_role":"admin"}
  --   {"owned_hub_count":2}
  --   {"channel_type":"voice"}
  --
  -- Bad examples:
  --   IP address
  --   user-agent
  --   email address
  --   username/display name
  --   message content
  --   file content
  --   token
  --   cookie
  --   full request body
  --   full headers
  --   arbitrary client payload
  metadata        jsonb NOT NULL DEFAULT '{}'::jsonb,

  CONSTRAINT audit_events_category_check
    CHECK (
      category IN (
        'auth',
        'account',
        'connection',
        'hub',
        'channel',
        'member',
        'message',
        'attachment',
        'invite',
        'voice',
        'security',
        'legal',
        'system'
      )
    ),

  CONSTRAINT audit_events_severity_check
    CHECK (
      severity IN (
        'debug',
        'info',
        'warning',
        'error',
        'critical'
      )
    ),

  CONSTRAINT audit_events_actor_type_check
    CHECK (
      actor_type IN (
        'anonymous',
        'user',
        'system',
        'admin'
      )
    ),

  CONSTRAINT audit_events_event_type_length
    CHECK (
      char_length(btrim(event_type)) >= 1
      AND char_length(event_type) <= 120
    ),

  CONSTRAINT audit_events_request_id_length
    CHECK (
      request_id IS NULL
      OR char_length(request_id) <= 128
    ),

  CONSTRAINT audit_events_session_id_length
    CHECK (
      session_id IS NULL
      OR char_length(session_id) <= 128
    ),

  CONSTRAINT audit_events_connection_id_length
    CHECK (
      connection_id IS NULL
      OR char_length(connection_id) <= 128
    ),

  CONSTRAINT audit_events_server_node_id_length
    CHECK (
      server_node_id IS NULL
      OR char_length(server_node_id) <= 128
    ),

  CONSTRAINT audit_events_error_code_length
    CHECK (
      error_code IS NULL
      OR char_length(error_code) <= 120
    ),

  CONSTRAINT audit_events_status_code_check
    CHECK (
      status_code IS NULL
      OR (status_code >= 100 AND status_code <= 599)
    ),

  CONSTRAINT audit_events_metadata_object
    CHECK (jsonb_typeof(metadata) = 'object')
);

-- ============================================================
-- Indexes
-- ============================================================

CREATE INDEX IF NOT EXISTS idx_audit_events_occurred_at
  ON kergit_app.audit_events(occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_category_occurred_at
  ON kergit_app.audit_events(category, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_event_type_occurred_at
  ON kergit_app.audit_events(event_type, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_actor_user_id_occurred_at
  ON kergit_app.audit_events(actor_user_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_target_user_id_occurred_at
  ON kergit_app.audit_events(target_user_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_hub_id_occurred_at
  ON kergit_app.audit_events(hub_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_channel_id_occurred_at
  ON kergit_app.audit_events(channel_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_message_id_occurred_at
  ON kergit_app.audit_events(message_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_attachment_id_occurred_at
  ON kergit_app.audit_events(attachment_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_invite_id_occurred_at
  ON kergit_app.audit_events(invite_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_events_request_id
  ON kergit_app.audit_events(request_id);

CREATE INDEX IF NOT EXISTS idx_audit_events_session_id
  ON kergit_app.audit_events(session_id);

CREATE INDEX IF NOT EXISTS idx_audit_events_connection_id
  ON kergit_app.audit_events(connection_id);

-- ============================================================
-- Purge helper
-- ============================================================
-- Deletes old audit events in small batches.
--
-- Server-side maintenance code should call this periodically,
-- for example once per hour:
--
--   SELECT kergit_app.purge_old_audit_events(5000);
--
-- If the return value is 5000, the caller may call it again.

CREATE OR REPLACE FUNCTION kergit_app.purge_old_audit_events(p_limit integer DEFAULT 5000)
RETURNS integer
LANGUAGE plpgsql
AS $$
DECLARE
  v_deleted integer;
BEGIN
  IF p_limit <= 0 THEN
    RETURN 0;
  END IF;

  WITH old_events AS (
    SELECT id
    FROM kergit_app.audit_events
    WHERE occurred_at < now() - interval '30 days'
    ORDER BY occurred_at ASC
    LIMIT p_limit
    FOR UPDATE SKIP LOCKED
  ),
  deleted AS (
    DELETE FROM kergit_app.audit_events e
    USING old_events
    WHERE e.id = old_events.id
    RETURNING 1
  )
  SELECT count(*)
  INTO v_deleted
  FROM deleted;

  RETURN v_deleted;
END
$$;

-- ============================================================
-- Event naming guide
-- ============================================================
-- Use these event_type values from server-side application code.
--
-- auth:
--   auth.login_succeeded
--   auth.login_failed
--   auth.logout
--
-- hub:
--   hub.created
--   hub.updated
--   hub.deleted
--   hub.invite.created
--   hub.invite.used
--   hub.invite.revoked
--   hub.invite.expired
--   hub.member.joined
--   hub.member.left
--   hub.member.kicked
--   hub.member.role_changed
--
-- channel:
--   channel.created
--   channel.updated
--   channel.deleted
--
-- voice:
--   voice.joined
--   voice.left
--   voice.takeover
--   voice.kicked
--
-- ============================================================
-- Permissions
-- ============================================================

REVOKE ALL ON TABLE kergit_app.audit_events FROM anon, authenticated;
REVOKE ALL ON FUNCTION kergit_app.purge_old_audit_events(integer) FROM anon, authenticated;

COMMIT;
