-- 011_create_profile_on_auth_signup.sql

BEGIN;

CREATE OR REPLACE FUNCTION kergit_app.create_profile_from_auth_user()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = ''
AS $$
DECLARE
  v_metadata jsonb := COALESCE(NEW.raw_user_meta_data, '{}'::jsonb);
  v_user_name text;
  v_display_name text;
  v_avatar_seed text;
BEGIN
  v_user_name := lower(btrim(COALESCE(
    v_metadata ->> 'user_name',
    v_metadata ->> 'username',
    ''
  )));

  v_display_name := btrim(COALESCE(
    v_metadata ->> 'display_name',
    v_user_name
  ));

  v_avatar_seed := COALESCE(
    NULLIF(btrim(v_metadata ->> 'avatar_seed'), ''),
    'Caleb'
  );

  IF v_user_name = '' THEN
    RAISE EXCEPTION 'Profile user_name is required';
  END IF;

  IF v_display_name = '' THEN
    RAISE EXCEPTION 'Profile display_name is required';
  END IF;

  INSERT INTO kergit_app.profiles (
    user_id,
    user_name,
    display_name,
    avatar_seed
  )
  VALUES (
    NEW.id,
    v_user_name,
    v_display_name,
    v_avatar_seed
  )
  ON CONFLICT (user_id) DO NOTHING;

  RETURN NEW;
END
$$;

DROP TRIGGER IF EXISTS trg_create_profile_from_auth_user
  ON auth.users;

CREATE TRIGGER trg_create_profile_from_auth_user
  AFTER INSERT ON auth.users
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.create_profile_from_auth_user();

REVOKE ALL ON FUNCTION kergit_app.create_profile_from_auth_user()
FROM anon, authenticated;

COMMIT;