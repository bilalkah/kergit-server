ALTER TABLE public.messages
  ADD COLUMN IF NOT EXISTS attachments_json jsonb NOT NULL DEFAULT '[]'::jsonb;

ALTER TABLE public.messages
  ADD COLUMN IF NOT EXISTS link_preview_json jsonb NULL;

