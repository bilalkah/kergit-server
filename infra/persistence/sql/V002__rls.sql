-- V002__rls.sql  (FULL REPLACEMENT)
-- Row Level Security + policies for hubs, hub_members, channels, messages, voice_state
-- Fixes: no OLD/NEW in policies, no self-recursive RLS on hub_members.

BEGIN;

----------------------------------------------------------------------
-- 0) Enable RLS on all tables (idempotent)
----------------------------------------------------------------------
ALTER TABLE public.hubs        ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.hub_members ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.channels    ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.messages    ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.voice_state ENABLE ROW LEVEL SECURITY;

----------------------------------------------------------------------
-- 1) Helper functions to avoid hub_members self-recursion in policies
--    SECURITY DEFINER ensures they evaluate without re-entering RLS
--    on public.hub_members. search_path is locked down.
----------------------------------------------------------------------

CREATE OR REPLACE FUNCTION public._role_in_hub(p_hub_id uuid)
RETURNS text
LANGUAGE sql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
  SELECT hm.role
  FROM public.hub_members hm
  WHERE hm.hub_id = p_hub_id
    AND hm.user_id = auth.uid()
  LIMIT 1
$$;

CREATE OR REPLACE FUNCTION public._is_member(p_hub_id uuid)
RETURNS boolean
LANGUAGE sql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
  SELECT public._role_in_hub(p_hub_id) IS NOT NULL
$$;

CREATE OR REPLACE FUNCTION public._is_owner(p_hub_id uuid)
RETURNS boolean
LANGUAGE sql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
  SELECT public._role_in_hub(p_hub_id) = 'owner'
$$;

CREATE OR REPLACE FUNCTION public._is_admin_or_owner(p_hub_id uuid)
RETURNS boolean
LANGUAGE sql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
  SELECT public._role_in_hub(p_hub_id) IN ('owner','admin')
$$;

-- allow authenticated users to call the helpers
GRANT EXECUTE ON FUNCTION public._role_in_hub(uuid)       TO authenticated;
GRANT EXECUTE ON FUNCTION public._is_member(uuid)         TO authenticated;
GRANT EXECUTE ON FUNCTION public._is_owner(uuid)          TO authenticated;
GRANT EXECUTE ON FUNCTION public._is_admin_or_owner(uuid) TO authenticated;

----------------------------------------------------------------------
-- 2) H U B S  (owner-only delete; members can read; create ≤2 via trigger)
----------------------------------------------------------------------

-- drop existing policies (safe re-run)
DROP POLICY IF EXISTS hubs_select_if_member     ON public.hubs;
DROP POLICY IF EXISTS hubs_insert_owner_is_self ON public.hubs;
DROP POLICY IF EXISTS hubs_update_owner_only    ON public.hubs;
DROP POLICY IF EXISTS hubs_delete_owner_only    ON public.hubs;

-- members can view hubs they belong to
CREATE POLICY hubs_select_if_member
  ON public.hubs
  FOR SELECT
  TO authenticated
  USING (
    EXISTS (
      SELECT 1
      FROM public.hub_members me
      WHERE me.hub_id = hubs.id
        AND me.user_id = auth.uid()
    )
  );

-- create hub: must set yourself as owner (≤2 enforced by V001 trigger)
CREATE POLICY hubs_insert_owner_is_self
  ON public.hubs
  FOR INSERT
  TO authenticated
  WITH CHECK (owner_id = auth.uid());

-- update / delete hub: owner only
CREATE POLICY hubs_update_owner_only
  ON public.hubs
  FOR UPDATE
  TO authenticated
  USING (owner_id = auth.uid())
  WITH CHECK (owner_id = auth.uid());

CREATE POLICY hubs_delete_owner_only
  ON public.hubs
  FOR DELETE
  TO authenticated
  USING (owner_id = auth.uid());

----------------------------------------------------------------------
-- 3) H U B   M E M B E R S  (use helper functions to avoid recursion)
----------------------------------------------------------------------

DROP POLICY IF EXISTS hub_members_select_if_member            ON public.hub_members;
DROP POLICY IF EXISTS hub_members_insert_owner_admin_only     ON public.hub_members;
DROP POLICY IF EXISTS hub_members_update_promote_demote       ON public.hub_members;
DROP POLICY IF EXISTS hub_members_delete_admin_owner_or_self  ON public.hub_members;

-- read membership only if you are a member of that hub
CREATE POLICY hub_members_select_if_member
  ON public.hub_members
  FOR SELECT
  TO authenticated
  USING ( public._is_member(hub_members.hub_id) );

-- insert (invite): only owner/admin of the hub; role must be admin|member
CREATE POLICY hub_members_insert_owner_admin_only
  ON public.hub_members
  FOR INSERT
  TO authenticated
  WITH CHECK (
    role IN ('admin','member')
    AND public._is_admin_or_owner(hub_members.hub_id)
  );

-- update (promote/demote):
--   allowed if requester is owner
--   OR requester is admin AND target row is not an owner row
--   new role must be admin|member (no setting 'owner' via UPDATE)
CREATE POLICY hub_members_update_promote_demote
  ON public.hub_members
  FOR UPDATE
  TO authenticated
  USING (
    public._is_owner(hub_members.hub_id)
    OR (
      public._role_in_hub(hub_members.hub_id) = 'admin'
      AND hub_members.role <> 'owner'
    )
  )
  WITH CHECK ( role IN ('admin','member') );

-- delete:
--   self can leave (but not the owner row)
--   owner can delete any non-owner row
--   admin can delete non-owner rows
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
      AND hub_members.role <> 'owner'
    )
  );

----------------------------------------------------------------------
-- 4) C H A N N E L S  (owner/admin manage; members can view)
----------------------------------------------------------------------

DROP POLICY IF EXISTS channels_select_if_member       ON public.channels;
DROP POLICY IF EXISTS channels_cud_owner_admin_only   ON public.channels;

-- visible to hub members
CREATE POLICY channels_select_if_member
  ON public.channels
  FOR SELECT
  TO authenticated
  USING (
    EXISTS (
      SELECT 1
      FROM public.hub_members me
      WHERE me.hub_id = channels.hub_id
        AND me.user_id = auth.uid()
    )
  );

-- create/update/delete channels: owner or admin
CREATE POLICY channels_cud_owner_admin_only
  ON public.channels
  FOR ALL
  TO authenticated
  USING (
    public._is_admin_or_owner(channels.hub_id)
  )
  WITH CHECK (
    public._is_admin_or_owner(channels.hub_id)
  );

----------------------------------------------------------------------
-- 5) M E S S A G E S  (member visibility; self-sender insert; edits window)
----------------------------------------------------------------------

DROP POLICY IF EXISTS messages_select_if_member             ON public.messages;
DROP POLICY IF EXISTS messages_insert_if_member_self_sender ON public.messages;
DROP POLICY IF EXISTS messages_update_edit_rules            ON public.messages;
DROP POLICY IF EXISTS messages_delete_owner_admin_only      ON public.messages;

-- members of the channel's hub can read
CREATE POLICY messages_select_if_member
  ON public.messages
  FOR SELECT
  TO authenticated
  USING (
    EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = messages.channel_id
        AND me.user_id = auth.uid()
    )
  );

-- insert: must be member of hub AND sender_id = auth.uid()
CREATE POLICY messages_insert_if_member_self_sender
  ON public.messages
  FOR INSERT
  TO authenticated
  WITH CHECK (
    sender_id = auth.uid()
    AND EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = messages.channel_id
        AND me.user_id = auth.uid()
    )
  );

-- update (edit): author within 10 minutes OR hub owner/admin
CREATE POLICY messages_update_edit_rules
  ON public.messages
  FOR UPDATE
  TO authenticated
  USING (
    (sender_id = auth.uid() AND now() - created_at <= INTERVAL '10 minutes')
    OR EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = messages.channel_id
        AND me.user_id = auth.uid()
        AND me.role IN ('owner','admin')
    )
  )
  WITH CHECK (true);

-- delete: hub owner/admin can delete anytime (authors should soft-delete via UPDATE)
CREATE POLICY messages_delete_owner_admin_only
  ON public.messages
  FOR DELETE
  TO authenticated
  USING (
    EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = messages.channel_id
        AND me.user_id = auth.uid()
        AND me.role IN ('owner','admin')
    )
  );

----------------------------------------------------------------------
-- 6) V O I C E   S T A T E  (ephemeral presence; members only)
----------------------------------------------------------------------

DROP POLICY IF EXISTS voice_state_select_if_member           ON public.voice_state;
DROP POLICY IF EXISTS voice_state_insert_if_member           ON public.voice_state;
DROP POLICY IF EXISTS voice_state_update_self_only           ON public.voice_state;
DROP POLICY IF EXISTS voice_state_delete_self_or_owner_admin ON public.voice_state;

-- visible to members of the channel's hub
CREATE POLICY voice_state_select_if_member
  ON public.voice_state
  FOR SELECT
  TO authenticated
  USING (
    EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = voice_state.channel_id
        AND me.user_id = auth.uid()
    )
  );

-- insert: only hub members
CREATE POLICY voice_state_insert_if_member
  ON public.voice_state
  FOR INSERT
  TO authenticated
  WITH CHECK (
    EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = voice_state.channel_id
        AND me.user_id = auth.uid()
    )
  );

-- update: only the user can update their own row
CREATE POLICY voice_state_update_self_only
  ON public.voice_state
  FOR UPDATE
  TO authenticated
  USING (user_id = auth.uid())
  WITH CHECK (user_id = auth.uid());

-- delete: self OR hub owner/admin
CREATE POLICY voice_state_delete_self_or_owner_admin
  ON public.voice_state
  FOR DELETE
  TO authenticated
  USING (
    user_id = auth.uid()
    OR EXISTS (
      SELECT 1
      FROM public.channels c
      JOIN public.hub_members me ON me.hub_id = c.hub_id
      WHERE c.id = voice_state.channel_id
        AND me.user_id = auth.uid()
        AND me.role IN ('owner','admin')
    )
  );

COMMIT;
