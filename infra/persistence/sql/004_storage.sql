-- 004_storage.sql
-- Kergit Storage bucket bootstrap.
--
-- Depends on:
-- - Supabase Storage extension/schema when running on Supabase.
--
-- Assumptions:
-- - Attachment file bytes live in Supabase Storage or another object storage.
-- - Attachment metadata lives in kergit_app.message_attachments.
-- - Storage bucket is private.
-- - Client must not get broad public object access.
-- - Upload/download authorization is handled by server-side application code.
-- - Server may issue signed upload/download data after checking membership.
--
-- Do NOT store:
-- - message content
-- - file content in PostgreSQL
-- - access tokens
-- - refresh tokens
-- - full request bodies

BEGIN;

-- ============================================================
-- Storage bucket bootstrap
-- ============================================================
-- Supabase-only helper.
-- Safe no-op when storage schema is unavailable, for local/non-Supabase DBs.

DO $$
BEGIN
  IF to_regclass('storage.buckets') IS NULL THEN
    RAISE NOTICE 'storage.buckets is unavailable; skipping chat-attachments bucket bootstrap';
    RETURN;
  END IF;

  INSERT INTO storage.buckets (
    id,
    name,
    public,
    file_size_limit,
    allowed_mime_types
  )
  VALUES (
    'chat-attachments',
    'chat-attachments',
    false,
    15728640, -- 15 MB
    ARRAY[
      'image/png',
      'image/jpeg',
      'image/webp',
      'image/gif',
      'image/avif',
      'application/pdf',
      'text/plain',
      'application/zip',
      'application/x-zip-compressed'
    ]
  )
  ON CONFLICT (id) DO UPDATE
  SET
    public = EXCLUDED.public,
    file_size_limit = EXCLUDED.file_size_limit,
    allowed_mime_types = EXCLUDED.allowed_mime_types;
END
$$;

COMMIT;
