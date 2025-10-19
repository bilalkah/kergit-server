BEGIN;

-- 1) Ensure a hub owned by your existing Supabase user
WITH upsert_hub AS (
  INSERT INTO public.hubs (name, owner_id)
  VALUES ('Global Hub', 'fb2de03f-a892-4a79-b3bc-a94ad7c8a7f2')
  ON CONFLICT (owner_id, name) DO UPDATE
    SET owner_id = EXCLUDED.owner_id
  RETURNING id
),
picked_hub AS (
  -- if hub already existed, take its id
  SELECT id FROM upsert_hub
  UNION ALL
  SELECT h.id
  FROM public.hubs h
  WHERE h.owner_id = 'fb2de03f-a892-4a79-b3bc-a94ad7c8a7f2'
    AND h.name = 'Global Hub'
  LIMIT 1
),

-- 2) Ensure #general channel (type text) under that hub
ins_chan AS (
  INSERT INTO public.channels (hub_id, name, type)
  SELECT id, 'general', 'text' FROM picked_hub
  ON CONFLICT (hub_id, name) DO NOTHING
  RETURNING id
),
picked_chan AS (
  SELECT id FROM ins_chan
  UNION ALL
  SELECT c.id
  FROM public.channels c
  JOIN picked_hub h ON c.hub_id = h.id
  WHERE c.name = 'general'
  LIMIT 1
),

-- 3) Ensure the owner has a membership row (role=owner)
ensure_owner AS (
  INSERT INTO public.hub_members (hub_id, user_id, role)
  SELECT id, 'fb2de03f-a892-4a79-b3bc-a94ad7c8a7f2', 'owner' FROM picked_hub
  ON CONFLICT (hub_id, user_id) DO NOTHING
  RETURNING hub_id
)

-- 4) Return the effective IDs so you can copy/paste if needed
SELECT
  (SELECT id FROM picked_hub)  AS hub_id,
  (SELECT id FROM picked_chan) AS channel_id;

COMMIT;


BEGIN;

-- Function runs as its owner (postgres) and bypasses your normal member-only INSERT policy.
CREATE OR REPLACE FUNCTION public.auto_join_default_hub()
RETURNS TRIGGER
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public, pg_temp
AS $$
DECLARE
  v_hub_id uuid;
BEGIN
  -- Find your default hub by name; fail loudly if it doesn't exist.
  SELECT id INTO v_hub_id
  FROM public.hubs
  WHERE name = 'Global Hub'
  LIMIT 1;

  IF v_hub_id IS NULL THEN
    RAISE EXCEPTION 'Default hub "Global Hub" not found. Create it first.';
  END IF;

  -- Insert membership as 'member' for the newly registered auth.users row.
  -- ON CONFLICT safely no-ops if (hub_id, user_id) already exists
  -- (e.g., owners created earlier via your owner-membership trigger).
  INSERT INTO public.hub_members(hub_id, user_id, role)
  VALUES (v_hub_id, NEW.id, 'member')
  ON CONFLICT (hub_id, user_id) DO NOTHING;

  RETURN NEW;
END $$;

-- Recreate trigger (safe to run multiple times)
DROP TRIGGER IF EXISTS trg_auto_join_default_hub ON auth.users;
CREATE TRIGGER trg_auto_join_default_hub
  AFTER INSERT ON auth.users
  FOR EACH ROW
  EXECUTE FUNCTION public.auto_join_default_hub();

COMMIT;


BEGIN;

WITH default_hub AS (
  SELECT id AS hub_id
  FROM public.hubs
  WHERE name = 'Global Hub'
  LIMIT 1
)
INSERT INTO public.hub_members (hub_id, user_id, role)
SELECT d.hub_id, u.id, 'member'
FROM auth.users u
CROSS JOIN default_hub d
LEFT JOIN public.hub_members m
  ON m.hub_id = d.hub_id AND m.user_id = u.id
WHERE m.hub_id IS NULL;  -- only users not already in the hub

COMMIT;


