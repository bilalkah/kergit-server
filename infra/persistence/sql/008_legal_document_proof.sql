-- legal_document_proof.sql
-- Store legal acceptance/delivery proof without storing legal document bodies.
--
-- Current documents:
--   terms alpha-1/tr-TR:
--     b5e9215b21ef17bb610b370c7a2460219f1572ab4301c226521255d30e28a2fd
--   privacy notice alpha-1/tr-TR:
--     c791a73fa778c8fd70c219c1461bf9fa8a7700d33ccc73592f3933ed474fabe5
--
-- Assumptions:
-- - Client does not access PostgreSQL directly.
-- - Signup sends legal proof metadata to Supabase Auth.
-- - This trigger records proof when auth.users row is created.
-- - This table intentionally avoids foreign keys.
--   Legal proof should survive auth/profile lifecycle changes.
-- - Account deletion uses app-level soft delete/anonymization.
-- - Legal proof stores document version/hash metadata only.
-- - Legal proof does NOT store legal document bodies.
--
-- Do NOT store:
-- - email address
-- - password
-- - access token
-- - refresh token
-- - full request body
-- - message content
-- - file content

BEGIN;

-- ============================================================
-- Legal terms acceptance proof
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.legal_terms_acceptances (
  id            uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  user_id       uuid NOT NULL,

  terms_version text NOT NULL,
  locale        text NOT NULL,
  document_hash text NOT NULL,

  accepted_at   timestamptz NOT NULL,
  created_at    timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT legal_terms_acceptances_terms_version_length
    CHECK (
      char_length(btrim(terms_version)) >= 1
      AND char_length(terms_version) <= 40
    ),

  CONSTRAINT legal_terms_acceptances_locale_length
    CHECK (
      char_length(btrim(locale)) >= 2
      AND char_length(locale) <= 16
    ),

  CONSTRAINT legal_terms_acceptances_document_hash_sha256
    CHECK (document_hash ~ '^[0-9a-f]{64}$')
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_legal_terms_acceptances_unique_document_proof
  ON kergit_app.legal_terms_acceptances (
    user_id,
    terms_version,
    locale,
    document_hash
  );

CREATE INDEX IF NOT EXISTS idx_legal_terms_acceptances_user_id
  ON kergit_app.legal_terms_acceptances(user_id);

CREATE INDEX IF NOT EXISTS idx_legal_terms_acceptances_accepted_at
  ON kergit_app.legal_terms_acceptances(accepted_at DESC);

-- ============================================================
-- Privacy notice delivery proof
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.privacy_notice_deliveries (
  id             uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  user_id        uuid NOT NULL,

  notice_version text NOT NULL,
  locale         text NOT NULL,
  document_hash  text NOT NULL,

  delivered_at   timestamptz NOT NULL,
  created_at     timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT privacy_notice_deliveries_notice_version_length
    CHECK (
      char_length(btrim(notice_version)) >= 1
      AND char_length(notice_version) <= 40
    ),

  CONSTRAINT privacy_notice_deliveries_locale_length
    CHECK (
      char_length(btrim(locale)) >= 2
      AND char_length(locale) <= 16
    ),

  CONSTRAINT privacy_notice_deliveries_document_hash_sha256
    CHECK (document_hash ~ '^[0-9a-f]{64}$')
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_privacy_notice_deliveries_unique_document_proof
  ON kergit_app.privacy_notice_deliveries (
    user_id,
    notice_version,
    locale,
    document_hash
  );

CREATE INDEX IF NOT EXISTS idx_privacy_notice_deliveries_user_id
  ON kergit_app.privacy_notice_deliveries(user_id);

CREATE INDEX IF NOT EXISTS idx_privacy_notice_deliveries_delivered_at
  ON kergit_app.privacy_notice_deliveries(delivered_at DESC);

-- ============================================================
-- Signup legal proof trigger
-- ============================================================
-- This trigger enforces current legal proof metadata during signup.
--
-- Important:
-- - User Agreement requires acceptance.
-- - Privacy/KVKK notice is delivery/read notice proof, not consent.
-- - If legal metadata is missing or stale, signup fails.
-- - Update hashes here whenever legal document text changes.

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
    'b5e9215b21ef17bb610b370c7a2460219f1572ab4301c226521255d30e28a2fd';

  v_privacy_notice_hash constant text :=
    'c791a73fa778c8fd70c219c1461bf9fa8a7700d33ccc73592f3933ed474fabe5';

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

DROP TRIGGER IF EXISTS trg_record_signup_legal_proof
  ON auth.users;

CREATE TRIGGER trg_record_signup_legal_proof
  AFTER INSERT ON auth.users
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.record_signup_legal_proof();

-- No direct client DB access.
REVOKE ALL ON TABLE kergit_app.legal_terms_acceptances FROM anon, authenticated;
REVOKE ALL ON TABLE kergit_app.privacy_notice_deliveries FROM anon, authenticated;
REVOKE ALL ON FUNCTION kergit_app.record_signup_legal_proof() FROM anon, authenticated;

COMMIT;
