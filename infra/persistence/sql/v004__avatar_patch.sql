begin;
CREATE TABLE public.profiles (
  user_id UUID PRIMARY KEY REFERENCES auth.users(id) ON DELETE CASCADE,
  avatar_seed TEXT NOT NULL DEFAULT 'Caleb',
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

ALTER TABLE public.hubs
ADD COLUMN avatar_seed TEXT NOT NULL DEFAULT 'Felix';/*  */
commit;