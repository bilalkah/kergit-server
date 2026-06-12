-- Supabase storage bootstrap for chat attachments.
-- Safe no-op when the storage schema is unavailable (non-Supabase DB environments).
DO $$
BEGIN
  IF to_regclass('storage.buckets') IS NULL THEN
    RAISE NOTICE 'storage.buckets is unavailable; skipping chat-attachments bucket bootstrap';
    RETURN;
  END IF;

  INSERT INTO storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
  VALUES (
    'chat-attachments',
    'chat-attachments',
    false,
    15728640, -- 15 MB
    ARRAY[
      'image/png','image/jpeg','image/webp','image/gif','image/avif',
      'application/pdf','text/plain',
      'application/zip','application/x-zip-compressed'
    ]
  )
  ON CONFLICT (id) DO UPDATE
  SET public = EXCLUDED.public,
      file_size_limit = EXCLUDED.file_size_limit,
      allowed_mime_types = EXCLUDED.allowed_mime_types;
END
$$;
