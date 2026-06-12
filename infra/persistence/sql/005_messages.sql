-- 005_messages.sql
-- Kergit messages, message attachments and link preview metadata.
--
-- Depends on:
-- - 001_profiles.sql
-- - 002_hubs.sql
-- - 003_channels.sql
-- - 004_storage.sql
-- - kergit_app.profiles
-- - kergit_app.channels
--
-- Assumptions:
-- - Client does not access PostgreSQL directly.
-- - All writes go through server-side application code.
-- - No RLS policy is defined here.
-- - profiles uses app-level soft delete/anonymization.
-- - Message create is supported.
-- - Message edit is NOT supported yet.
-- - Message delete is NOT supported yet.
-- - Message history/audit is written by server-side application code to audit_events.
-- - Message content is stored once, in messages.content.
-- - Attachment file bytes live in Storage.
-- - Attachment metadata is stored in message_attachments, not in messages JSON.
-- - Link preview metadata may be stored on the message.
-- - Account deletion preserves message rows and attachment metadata.
-- - Deleted users are represented through anonymized profiles.
-- - Audit logs must not copy message content, file content, tokens, or full request bodies.

BEGIN;

-- ============================================================
-- Per-channel message sequence
-- ============================================================
-- Stable ordering inside a channel.
--
-- Store messages row-by-row.
-- Do not store a channel's whole message list as one JSON blob.
-- The server can aggregate rows into JSON/protobuf for the client.

CREATE TABLE IF NOT EXISTS kergit_app.channel_message_counters (
  channel_id uuid PRIMARY KEY REFERENCES kergit_app.channels(id) ON DELETE CASCADE,
  next_seq   bigint NOT NULL DEFAULT 1,

  CONSTRAINT channel_message_counters_next_seq_check
    CHECK (next_seq >= 1)
);

-- ============================================================
-- Messages
-- ============================================================
-- sender_id is nullable only as a defensive fallback if a profile is ever
-- physically deleted.
--
-- Normal account deletion should NOT physically delete the profile row.
-- It should anonymize the profile and set profiles.deleted_at.
--
-- New messages still require an active sender. This is enforced by trigger.

CREATE TABLE IF NOT EXISTS kergit_app.messages (
  id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  channel_id  uuid NOT NULL REFERENCES kergit_app.channels(id) ON DELETE CASCADE,
  sender_id   uuid REFERENCES kergit_app.profiles(user_id) ON DELETE SET NULL,

  message_seq bigint NOT NULL,

  content     text NOT NULL,

  -- Optional server-side generated link preview metadata.
  -- Keep this compact. Do not store fetch logs, full request traces,
  -- or sensitive target response data here.
  link_preview_json jsonb,

  created_at  timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT messages_content_length
    CHECK (
      char_length(content) <= 4000
    ),

  CONSTRAINT messages_message_seq_check
    CHECK (message_seq >= 1),

  CONSTRAINT messages_channel_seq_unique
    UNIQUE (channel_id, message_seq),

  CONSTRAINT messages_link_preview_object
    CHECK (
      link_preview_json IS NULL
      OR jsonb_typeof(link_preview_json) = 'object'
    )
);

CREATE INDEX IF NOT EXISTS idx_messages_channel_seq_desc
  ON kergit_app.messages(channel_id, message_seq DESC);

CREATE INDEX IF NOT EXISTS idx_messages_sender_created_at
  ON kergit_app.messages(sender_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_messages_created_at
  ON kergit_app.messages(created_at DESC);

-- Messages can only be inserted into text channels.
CREATE OR REPLACE FUNCTION kergit_app.ensure_message_text_channel()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_channel_type text;
BEGIN
  SELECT c.type
  INTO v_channel_type
  FROM kergit_app.channels c
  WHERE c.id = NEW.channel_id;

  IF v_channel_type IS NULL THEN
    RAISE EXCEPTION 'Channel does not exist';
  END IF;

  IF v_channel_type <> 'text' THEN
    RAISE EXCEPTION 'Messages can only be created in text channels';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_messages_text_channel
  BEFORE INSERT ON kergit_app.messages
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_message_text_channel();

-- New messages require an active, non-deleted sender.
CREATE OR REPLACE FUNCTION kergit_app.ensure_message_sender_active()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_deleted_at timestamptz;
BEGIN
  IF NEW.sender_id IS NULL THEN
    RAISE EXCEPTION 'Message sender is required';
  END IF;

  SELECT p.deleted_at
  INTO v_deleted_at
  FROM kergit_app.profiles p
  WHERE p.user_id = NEW.sender_id;

  IF NOT FOUND THEN
    RAISE EXCEPTION 'Message sender profile does not exist';
  END IF;

  IF v_deleted_at IS NOT NULL THEN
    RAISE EXCEPTION 'Deleted users cannot create messages';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_messages_sender_active
  BEFORE INSERT ON kergit_app.messages
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_message_sender_active();

-- Assign a stable per-channel sequence number.
--
-- PostgreSQL row locking on channel_message_counters serializes concurrent
-- inserts for the same channel.
CREATE OR REPLACE FUNCTION kergit_app.assign_channel_message_seq()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_next_seq bigint;
BEGIN
  IF NEW.message_seq IS NOT NULL THEN
    RETURN NEW;
  END IF;

  WITH upserted AS (
    INSERT INTO kergit_app.channel_message_counters (
      channel_id,
      next_seq
    )
    VALUES (
      NEW.channel_id,
      2
    )
    ON CONFLICT (channel_id) DO UPDATE
    SET next_seq = kergit_app.channel_message_counters.next_seq + 1
    RETURNING next_seq
  )
  SELECT next_seq - 1
  INTO v_next_seq
  FROM upserted;

  NEW.message_seq := v_next_seq;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_messages_assign_seq
  BEFORE INSERT ON kergit_app.messages
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.assign_channel_message_seq();

-- ============================================================
-- Message attachments
-- ============================================================
-- Metadata only.
-- File bytes live in Storage.
--
-- Product model:
-- - upload creates Storage object and message_attachments metadata
-- - message fetch joins attachments by message_id
-- - signed URL generation checks hub/channel membership in server-side code
-- - hub/channel/message deletion must delete Storage objects by
--   storage_bucket + storage_key through app-level cleanup
-- - account deletion does not delete preserved message attachments by default
--
-- No attachment file content is stored in PostgreSQL.

CREATE TABLE IF NOT EXISTS kergit_app.message_attachments (
  id             uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  message_id     uuid NOT NULL REFERENCES kergit_app.messages(id) ON DELETE CASCADE,
  uploaded_by    uuid REFERENCES kergit_app.profiles(user_id) ON DELETE SET NULL,

  storage_bucket text NOT NULL DEFAULT 'chat-attachments',
  storage_key    text NOT NULL UNIQUE,

  file_name      text NOT NULL,
  mime_type      text,
  size_bytes     bigint NOT NULL,

  created_at     timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT message_attachments_bucket_length
    CHECK (
      char_length(btrim(storage_bucket)) >= 1
      AND char_length(storage_bucket) <= 80
    ),

  CONSTRAINT message_attachments_storage_key_length
    CHECK (
      char_length(btrim(storage_key)) >= 1
      AND char_length(storage_key) <= 1024
    ),

  CONSTRAINT message_attachments_file_name_length
    CHECK (
      char_length(btrim(file_name)) >= 1
      AND char_length(file_name) <= 255
    ),

  CONSTRAINT message_attachments_mime_type_length
    CHECK (
      mime_type IS NULL
      OR char_length(mime_type) <= 120
    ),

  CONSTRAINT message_attachments_size_check
    CHECK (
      size_bytes >= 0
      AND size_bytes <= 15728640 -- 15 MB
    )
);

CREATE INDEX IF NOT EXISTS idx_message_attachments_message_id
  ON kergit_app.message_attachments(message_id);

CREATE INDEX IF NOT EXISTS idx_message_attachments_uploaded_by
  ON kergit_app.message_attachments(uploaded_by);

CREATE INDEX IF NOT EXISTS idx_message_attachments_created_at
  ON kergit_app.message_attachments(created_at DESC);

-- New attachments require an active, non-deleted uploader.
CREATE OR REPLACE FUNCTION kergit_app.ensure_attachment_uploader_active()
RETURNS trigger
LANGUAGE plpgsql
AS $$
DECLARE
  v_deleted_at timestamptz;
BEGIN
  IF NEW.uploaded_by IS NULL THEN
    RAISE EXCEPTION 'Attachment uploader is required';
  END IF;

  SELECT p.deleted_at
  INTO v_deleted_at
  FROM kergit_app.profiles p
  WHERE p.user_id = NEW.uploaded_by;

  IF NOT FOUND THEN
    RAISE EXCEPTION 'Attachment uploader profile does not exist';
  END IF;

  IF v_deleted_at IS NOT NULL THEN
    RAISE EXCEPTION 'Deleted users cannot upload attachments';
  END IF;

  RETURN NEW;
END
$$;

CREATE TRIGGER trg_message_attachments_uploader_active
  BEFORE INSERT ON kergit_app.message_attachments
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.ensure_attachment_uploader_active();

-- No direct client DB access.
REVOKE ALL ON TABLE kergit_app.channel_message_counters FROM anon, authenticated;
REVOKE ALL ON TABLE kergit_app.messages                 FROM anon, authenticated;
REVOKE ALL ON TABLE kergit_app.message_attachments      FROM anon, authenticated;

COMMIT;
