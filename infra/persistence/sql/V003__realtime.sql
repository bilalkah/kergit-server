-- V003__realtime.sql
BEGIN;

-- Add tables to the Realtime publication (ignore if already present)
DO $$
BEGIN
  BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.hubs;
  EXCEPTION WHEN duplicate_object THEN NULL;
  END;

  BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.hub_members;
  EXCEPTION WHEN duplicate_object THEN NULL;
  END;

  BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.channels;
  EXCEPTION WHEN duplicate_object THEN NULL;
  END;

  BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.messages;
  EXCEPTION WHEN duplicate_object THEN NULL;
  END;

  BEGIN
    ALTER PUBLICATION supabase_realtime ADD TABLE public.voice_state;
  EXCEPTION WHEN duplicate_object THEN NULL;
  END;
END $$;

COMMIT;
