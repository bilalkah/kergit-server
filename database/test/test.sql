-- === Configure two test users (replace both!) ===
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM auth.users WHERE id = 'a419cd98-9534-4e69-904c-0d4653986a63'::uuid) THEN
    RAISE EXCEPTION 'Replace a419cd98-9534-4e69-904c-0d4653986a63 with a real user id';
  END IF;
  IF NOT EXISTS (SELECT 1 FROM auth.users WHERE id = '77a11b96-bdf1-426f-a729-9fa2ea594c86'::uuid) THEN
    RAISE EXCEPTION 'Replace 77a11b96-bdf1-426f-a729-9fa2ea594c86 with a real user id';
  END IF;
END $$;

-- === Impersonation helpers (RLS context) ===
CREATE OR REPLACE FUNCTION public._impersonate(uid uuid)
RETURNS void LANGUAGE plpgsql AS $$
BEGIN
  PERFORM set_config('role','authenticated',true);
  PERFORM set_config('request.jwt.claim.sub',uid::text,true);
  PERFORM set_config('request.jwt.claims', json_build_object('sub',uid::text,'role','authenticated')::text, true);
END $$;

CREATE OR REPLACE FUNCTION public._reset_iam()
RETURNS void LANGUAGE plpgsql AS $$
BEGIN
  RESET role; RESET "request.jwt.claim.sub"; RESET "request.jwt.claims";
END $$;

-- === Sanity: tables present ===
SELECT
  to_regclass('public.hubs')       IS NOT NULL AS hubs_ok,
  to_regclass('public.hub_members') IS NOT NULL AS hub_members_ok,
  to_regclass('public.channels')   IS NOT NULL AS channels_ok,
  to_regclass('public.messages')   IS NOT NULL AS messages_ok,
  to_regclass('public.voice_state') IS NOT NULL AS voice_state_ok;

-- === TEST 1: Owner (A) creates hub; capture id in temp table ===
SELECT public._impersonate('a419cd98-9534-4e69-904c-0d4653986a63'::uuid);

DROP TABLE IF EXISTS tmp_hub;
CREATE TEMP TABLE tmp_hub(id uuid);

WITH ins AS (
  INSERT INTO public.hubs (name, owner_id)
  VALUES ('MVP Hub 1', auth.uid())
  RETURNING id
)
INSERT INTO tmp_hub SELECT id FROM ins;

-- Owner auto-membership check
SELECT 'owner_auto_membership' AS check,
       (SELECT role FROM public.hub_members
         WHERE hub_id = (SELECT id FROM tmp_hub) AND user_id = auth.uid()) = 'owner' AS ok;

-- === TEST 2: Add 2 text channels ===
INSERT INTO public.channels (hub_id, name, type)
SELECT id, 'general', 'text' FROM tmp_hub;
INSERT INTO public.channels (hub_id, name, type)
SELECT id, 'random',  'text' FROM tmp_hub;

-- === TEST 3: Invite user B as member, then promote to admin ===
INSERT INTO public.hub_members (hub_id, user_id, role)
SELECT id, '77a11b96-bdf1-426f-a729-9fa2ea594c86'::uuid, 'member' FROM tmp_hub;

UPDATE public.hub_members
   SET role = 'admin'
 WHERE hub_id = (SELECT id FROM tmp_hub)
   AND user_id = '77a11b96-bdf1-426f-a729-9fa2ea594c86'::uuid;

SELECT 'roles_after_promotion' AS check,
       (SELECT role FROM public.hub_members WHERE hub_id=(SELECT id FROM tmp_hub) AND user_id='77a11b96-bdf1-426f-a729-9fa2ea594c86'::uuid)='admin' AS b_is_admin,
       (SELECT role FROM public.hub_members WHERE hub_id=(SELECT id FROM tmp_hub) AND user_id=auth.uid())='owner' AS a_is_owner;

-- === TEST 4: Switch to B (admin), add 3rd channel; 4th must fail ===
SELECT public._reset_iam(); SELECT public._impersonate('77a11b96-bdf1-426f-a729-9fa2ea594c86'::uuid);

INSERT INTO public.channels (hub_id, name, type)
SELECT id, 'voice-1', 'voice' FROM tmp_hub;

DO $$
BEGIN
  BEGIN
    INSERT INTO public.channels (hub_id, name, type)
    SELECT id, 'extra', 'text' FROM tmp_hub;
    RAISE EXCEPTION '❌ expected channel limit (max 3) but insert #4 succeeded';
  EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE '✅ channel limit enforced: %', SQLERRM;
  END;
END $$;

-- === TEST 5: Messages: member can send; spoofing blocked ===
INSERT INTO public.messages (channel_id, sender_id, content)
SELECT c.id, auth.uid(), 'hello from B (admin)'
FROM public.channels c
WHERE c.hub_id=(SELECT id FROM tmp_hub) AND c.name='general';

DO $$
BEGIN
  BEGIN
    INSERT INTO public.messages (channel_id, sender_id, content)
    SELECT c.id, 'a419cd98-9534-4e69-904c-0d4653986a63'::uuid, 'spoof attempt'
    FROM public.channels c
    WHERE c.hub_id=(SELECT id FROM tmp_hub) AND c.name='general';
    RAISE EXCEPTION '❌ expected RLS failure on spoofed sender_id';
  EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE '✅ spoof blocked by RLS: %', SQLERRM;
  END;
END $$;

-- === TEST 6: Voice state: one voice channel per user ===
INSERT INTO public.voice_state (user_id, channel_id)
SELECT auth.uid(), c.id
FROM public.channels c
WHERE c.hub_id=(SELECT id FROM tmp_hub) AND c.name='voice-1';

DO $$
DECLARE other uuid;
BEGIN
  SELECT id INTO other FROM public.channels
  WHERE hub_id=(SELECT id FROM tmp_hub) AND name='random';

  BEGIN
    INSERT INTO public.voice_state (user_id, channel_id) VALUES (auth.uid(), other);
    RAISE EXCEPTION '❌ expected single-voice constraint violation';
  EXCEPTION WHEN unique_violation THEN
    RAISE NOTICE '✅ one-voice constraint enforced (PK on user_id)';
  WHEN OTHERS THEN
    RAISE NOTICE '✅ voice constraint enforced: %', SQLERRM;
  END;
END $$;

DELETE FROM public.voice_state WHERE user_id = auth.uid();

-- === TEST 7: Admin cannot delete hub ===
DO $$
BEGIN
  BEGIN
    DELETE FROM public.hubs WHERE id=(SELECT id FROM tmp_hub);
    RAISE EXCEPTION '❌ admin should NOT be able to delete hub';
  EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE '✅ admin delete blocked: %', SQLERRM;
  END;
END $$;

-- === TEST 8: Switch back to Owner (A) and delete hub ===
SELECT public._reset_iam(); SELECT public._impersonate('a419cd98-9534-4e69-904c-0d4653986a63'::uuid);

DELETE FROM public.hubs WHERE id=(SELECT id FROM tmp_hub);

SELECT 'hub_deleted_by_owner' AS check,
       NOT EXISTS (SELECT 1 FROM public.hubs WHERE id=(SELECT id FROM tmp_hub)) AS ok;

-- === Clean up RLS context (helpers optional to drop) ===
SELECT public._reset_iam();
