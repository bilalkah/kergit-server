-- 009_update_legal_proof_hashes.sql
-- Rerunnable update for current legal document hashes.
--
-- Purpose:
-- - Update the signup legal proof trigger function to the current
--   User Agreement / Privacy Notice (alpha-1, tr-TR) document hashes.
--
-- Safety:
-- - This migration is non-destructive and safe to run as an update.
-- - It does NOT drop or recreate the legal proof tables.
-- - It does NOT delete existing proof rows. Existing acceptance/delivery
--   rows remain as historical records of previously shown documents.
-- - It only replaces kergit_app.record_signup_legal_proof() constants.
-- - The existing AFTER INSERT trigger on auth.users keeps pointing at the
--   replaced function by name, so no trigger drop/recreate is required.
--
-- Current documents:
--   terms alpha-1/tr-TR:
--     155811d1bf243f68381140a63aafc5680233a413e7a7ff2289265d2c8ec75ad9
--   privacy notice alpha-1/tr-TR:
--     f82668be160aeedc2a95e742ef325b1384c6d5b0b2ac2943fa80f514f29f7404

BEGIN;

CREATE OR REPLACE FUNCTION kergit_app.record_signup_legal_proof()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = ''
AS $$
DECLARE
  v_terms_version constant text := 'alpha-1';
  v_privacy_notice_version constant text := 'alpha-1';
  v_locale constant text := 'tr-TR';

  v_terms_hash constant text :=
    '155811d1bf243f68381140a63aafc5680233a413e7a7ff2289265d2c8ec75ad9';

  v_privacy_notice_hash constant text :=
    'f82668be160aeedc2a95e742ef325b1384c6d5b0b2ac2943fa80f514f29f7404';

  v_metadata jsonb := COALESCE(NEW.raw_user_meta_data, '{}'::jsonb);
  v_recorded_at timestamptz := COALESCE(NEW.created_at, now());
BEGIN
  IF v_metadata ->> 'legal_terms_accepted' IS DISTINCT FROM 'true'
    OR v_metadata ->> 'legal_terms_version' IS DISTINCT FROM v_terms_version
    OR v_metadata ->> 'legal_terms_locale' IS DISTINCT FROM v_locale
    OR v_metadata ->> 'legal_terms_hash' IS DISTINCT FROM v_terms_hash
  THEN
    RAISE EXCEPTION 'Current User Agreement acceptance proof is required';
  END IF;

  IF v_metadata ->> 'legal_privacy_notice_delivered' IS DISTINCT FROM 'true'
    OR v_metadata ->> 'legal_privacy_notice_version' IS DISTINCT FROM v_privacy_notice_version
    OR v_metadata ->> 'legal_privacy_notice_locale' IS DISTINCT FROM v_locale
    OR v_metadata ->> 'legal_privacy_notice_hash' IS DISTINCT FROM v_privacy_notice_hash
  THEN
    RAISE EXCEPTION 'Current privacy notice delivery proof is required';
  END IF;

  INSERT INTO kergit_app.legal_terms_acceptances (
    user_id,
    terms_version,
    locale,
    document_hash,
    accepted_at
  )
  VALUES (
    NEW.id,
    v_terms_version,
    v_locale,
    v_terms_hash,
    v_recorded_at
  )
  ON CONFLICT (user_id, terms_version, locale, document_hash) DO NOTHING;

  INSERT INTO kergit_app.privacy_notice_deliveries (
    user_id,
    notice_version,
    locale,
    document_hash,
    delivered_at
  )
  VALUES (
    NEW.id,
    v_privacy_notice_version,
    v_locale,
    v_privacy_notice_hash,
    v_recorded_at
  )
  ON CONFLICT (user_id, notice_version, locale, document_hash) DO NOTHING;

  RETURN NEW;
END
$$;

REVOKE ALL ON FUNCTION kergit_app.record_signup_legal_proof() FROM anon, authenticated;

COMMIT;
