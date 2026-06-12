BEGIN;

CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE TABLE IF NOT EXISTS public.hub_roles (
  id uuid NOT NULL DEFAULT gen_random_uuid(),
  name text NOT NULL,
  description text,
  priority integer NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now(),
  CONSTRAINT hub_roles_pkey PRIMARY KEY (id),
  CONSTRAINT hub_roles_name_key UNIQUE (name)
);

INSERT INTO public.hub_roles (name, priority)
VALUES
  ('owner', 100),
  ('admin', 70),
  ('member', 10)
ON CONFLICT (name) DO NOTHING;

ALTER TABLE public.hub_members
  ADD COLUMN IF NOT EXISTS role_id uuid;

DO $$
DECLARE
  v_invalid_roles text;
BEGIN
  IF EXISTS (
    SELECT 1
    FROM information_schema.columns
    WHERE table_schema = 'public'
      AND table_name = 'hub_members'
      AND column_name = 'role'
  ) THEN
    SELECT string_agg(DISTINCT hm.role, ', ' ORDER BY hm.role)
      INTO v_invalid_roles
    FROM public.hub_members hm
    WHERE hm.role NOT IN ('owner', 'admin', 'member');

    IF v_invalid_roles IS NOT NULL THEN
      RAISE EXCEPTION 'hub_members.role contains unsupported value(s): %', v_invalid_roles;
    END IF;

    UPDATE public.hub_members hm
    SET role_id = hr.id
    FROM public.hub_roles hr
    WHERE hm.role = hr.name
      AND hm.role_id IS DISTINCT FROM hr.id;
  END IF;
END
$$;

DO $$
BEGIN
  IF EXISTS (
    SELECT 1
    FROM public.hub_members hm
    WHERE hm.role_id IS NULL
  ) THEN
    RAISE EXCEPTION 'hub_members.role_id contains NULL values; migration cannot proceed';
  END IF;
END
$$;

DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1
    FROM pg_constraint
    WHERE conname = 'hub_members_role_id_fkey'
      AND conrelid = 'public.hub_members'::regclass
  ) THEN
    ALTER TABLE public.hub_members
      ADD CONSTRAINT hub_members_role_id_fkey
      FOREIGN KEY (role_id)
      REFERENCES public.hub_roles(id)
      ON DELETE RESTRICT;
  END IF;
END
$$;

CREATE INDEX IF NOT EXISTS idx_hub_members_role_id
  ON public.hub_members(role_id);

ALTER TABLE public.hub_members
  ALTER COLUMN role_id SET NOT NULL;

CREATE OR REPLACE FUNCTION public.ensure_owner_membership()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
DECLARE
  v_owner_role_id uuid;
BEGIN
  IF NEW.owner_id IS NOT NULL THEN
    SELECT hr.id
      INTO v_owner_role_id
    FROM public.hub_roles hr
    WHERE hr.name = 'owner'
    LIMIT 1;

    IF v_owner_role_id IS NULL THEN
      RAISE EXCEPTION 'public.hub_roles is missing required role: owner';
    END IF;

    INSERT INTO public.hub_members (hub_id, user_id, role_id)
    VALUES (NEW.id, NEW.owner_id, v_owner_role_id)
    ON CONFLICT (hub_id, user_id) DO NOTHING;
  END IF;

  RETURN NEW;
END
$$;

CREATE OR REPLACE FUNCTION public._hub_role_name(p_role_id uuid)
RETURNS text
LANGUAGE sql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
  SELECT hr.name
  FROM public.hub_roles hr
  WHERE hr.id = p_role_id
  LIMIT 1
$$;

CREATE OR REPLACE FUNCTION public._role_in_hub(p_hub_id uuid)
RETURNS text
LANGUAGE sql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
  SELECT public._hub_role_name(hm.role_id)
  FROM public.hub_members hm
  WHERE hm.hub_id = p_hub_id
    AND hm.user_id = auth.uid()
  LIMIT 1
$$;

GRANT EXECUTE ON FUNCTION public._hub_role_name(uuid)       TO authenticated;
GRANT EXECUTE ON FUNCTION public._role_in_hub(uuid)         TO authenticated;
GRANT EXECUTE ON FUNCTION public._is_member(uuid)           TO authenticated;
GRANT EXECUTE ON FUNCTION public._is_owner(uuid)            TO authenticated;
GRANT EXECUTE ON FUNCTION public._is_admin_or_owner(uuid)   TO authenticated;

DROP POLICY IF EXISTS hub_members_insert_owner_admin_only    ON public.hub_members;
DROP POLICY IF EXISTS hub_members_update_promote_demote      ON public.hub_members;
DROP POLICY IF EXISTS hub_members_delete_admin_owner_or_self ON public.hub_members;
DROP POLICY IF EXISTS messages_update_edit_rules             ON public.messages;
DROP POLICY IF EXISTS messages_delete_owner_admin_only       ON public.messages;
DROP POLICY IF EXISTS voice_state_delete_self_or_owner_admin ON public.voice_state;

CREATE POLICY hub_members_insert_owner_admin_only
  ON public.hub_members
  FOR INSERT
  TO authenticated
  WITH CHECK (
    public._hub_role_name(hub_members.role_id) IN ('admin', 'member')
    AND public._is_admin_or_owner(hub_members.hub_id)
  );

CREATE POLICY hub_members_update_promote_demote
  ON public.hub_members
  FOR UPDATE
  TO authenticated
  USING (
    public._is_owner(hub_members.hub_id)
    OR (
      public._role_in_hub(hub_members.hub_id) = 'admin'
      AND public._hub_role_name(hub_members.role_id) <> 'owner'
    )
  )
  WITH CHECK (
    public._hub_role_name(hub_members.role_id) IN ('admin', 'member')
  );

CREATE POLICY hub_members_delete_admin_owner_or_self
  ON public.hub_members
  FOR DELETE
  TO authenticated
  USING (
    (
      user_id = auth.uid()
      AND NOT public._is_owner(hub_members.hub_id)
    )
    OR public._is_owner(hub_members.hub_id)
    OR (
      public._role_in_hub(hub_members.hub_id) = 'admin'
      AND public._hub_role_name(hub_members.role_id) <> 'owner'
    )
  );

CREATE POLICY messages_update_edit_rules
  ON public.messages
  FOR UPDATE
  TO authenticated
  USING (
    (sender_id = auth.uid() AND now() - created_at <= INTERVAL '10 minutes')
    OR EXISTS (
      SELECT 1
      FROM public.channels c
      WHERE c.id = messages.channel_id
        AND public._is_admin_or_owner(c.hub_id)
    )
  )
  WITH CHECK (true);

CREATE POLICY messages_delete_owner_admin_only
  ON public.messages
  FOR DELETE
  TO authenticated
  USING (
    EXISTS (
      SELECT 1
      FROM public.channels c
      WHERE c.id = messages.channel_id
        AND public._is_admin_or_owner(c.hub_id)
    )
  );

CREATE POLICY voice_state_delete_self_or_owner_admin
  ON public.voice_state
  FOR DELETE
  TO authenticated
  USING (
    user_id = auth.uid()
    OR EXISTS (
      SELECT 1
      FROM public.channels c
      WHERE c.id = voice_state.channel_id
        AND public._is_admin_or_owner(c.hub_id)
    )
  );

ALTER TABLE public.hub_members
  DROP CONSTRAINT IF EXISTS hub_members_role_check;

ALTER TABLE public.hub_members
  DROP COLUMN IF EXISTS role;

COMMIT;
