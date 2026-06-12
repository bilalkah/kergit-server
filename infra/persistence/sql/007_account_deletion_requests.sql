-- account_deletion_requests.sql
-- Kergit account deletion workflow tracking.
--
-- Depends on:
-- - pgcrypto extension already available
-- - profiles.sql
-- - hubs.sql
-- - kergit_app.set_updated_at()
--
-- Assumptions:
-- - Account deletion is handled by the Nuxt server auth endpoint:
--   clients/web/server/api/auth/delete-account.post.ts
-- - Client does not call Supabase Admin API directly.
-- - The Nuxt server endpoint verifies the current session and email confirmation.
-- - Account deletion uses app-level soft delete/anonymization:
--   profile anonymization + hub membership cleanup.
-- - Supabase Auth deletion is soft-delete through server-side Admin API.
-- - No RLS policy is defined here.
-- - This table intentionally avoids foreign keys.
--   Deletion records should survive profile/auth-user lifecycle changes.
-- - Owned hubs block account deletion.
-- - Account deletion must not silently delete owned hubs.
-- - Messages are preserved after account deletion.
-- - Message sender remains the anonymized/deleted profile.
-- - Detailed request/security context is written to audit_events.
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
-- Account deletion requests
-- ============================================================

CREATE TABLE IF NOT EXISTS kergit_app.account_deletion_requests (
  id                      uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  subject_user_id         uuid NOT NULL,
  requested_by_user_id    uuid,

  status                  text NOT NULL DEFAULT 'requested',

  requested_at             timestamptz NOT NULL DEFAULT now(),
  started_at               timestamptz,
  blocked_at               timestamptz,

  auth_soft_deleted_at     timestamptz,
  memberships_removed_at   timestamptz,
  profile_anonymized_at    timestamptz,

  finished_at              timestamptz,
  updated_at               timestamptz NOT NULL DEFAULT now(),

  error_code               text,
  failure_reason           text,

  -- Keep metadata compact and non-sensitive.
  -- Good examples:
  --   {"owned_hub_count":2}
  --   {"supabase_status":429}
  --
  -- Bad examples:
  --   email address
  --   request body
  --   tokens
  --   message content
  metadata                 jsonb NOT NULL DEFAULT '{}'::jsonb,

  CONSTRAINT account_deletion_requests_status_check
    CHECK (
      status IN (
        'requested',
        'running',
        'blocked',
        'completed',
        'failed',
        'cancelled'
      )
    ),

  CONSTRAINT account_deletion_requests_error_code_length
    CHECK (
      error_code IS NULL
      OR char_length(error_code) <= 120
    ),

  CONSTRAINT account_deletion_requests_failure_reason_length
    CHECK (
      failure_reason IS NULL
      OR char_length(failure_reason) <= 1000
    ),

  CONSTRAINT account_deletion_requests_metadata_object
    CHECK (jsonb_typeof(metadata) = 'object'),

  CONSTRAINT account_deletion_requests_started_at_check
    CHECK (
      started_at IS NULL
      OR started_at >= requested_at
    ),

  CONSTRAINT account_deletion_requests_blocked_at_check
    CHECK (
      blocked_at IS NULL
      OR blocked_at >= requested_at
    ),

  CONSTRAINT account_deletion_requests_auth_soft_deleted_at_check
    CHECK (
      auth_soft_deleted_at IS NULL
      OR auth_soft_deleted_at >= requested_at
    ),

  CONSTRAINT account_deletion_requests_memberships_removed_at_check
    CHECK (
      memberships_removed_at IS NULL
      OR memberships_removed_at >= requested_at
    ),

  CONSTRAINT account_deletion_requests_profile_anonymized_at_check
    CHECK (
      profile_anonymized_at IS NULL
      OR profile_anonymized_at >= requested_at
    ),

  CONSTRAINT account_deletion_requests_finished_at_check
    CHECK (
      finished_at IS NULL
      OR finished_at >= requested_at
    )
);

CREATE TRIGGER trg_account_deletion_requests_updated_at
  BEFORE UPDATE ON kergit_app.account_deletion_requests
  FOR EACH ROW
  EXECUTE FUNCTION kergit_app.set_updated_at();

-- One user should not have multiple active deletion workflows.
-- blocked/failed/completed are not active, so the user can retry later.
CREATE UNIQUE INDEX IF NOT EXISTS idx_account_deletion_requests_one_active
  ON kergit_app.account_deletion_requests(subject_user_id)
  WHERE status IN ('requested', 'running');

CREATE INDEX IF NOT EXISTS idx_account_deletion_requests_subject_user_id
  ON kergit_app.account_deletion_requests(subject_user_id);

CREATE INDEX IF NOT EXISTS idx_account_deletion_requests_requested_by_user_id
  ON kergit_app.account_deletion_requests(requested_by_user_id);

CREATE INDEX IF NOT EXISTS idx_account_deletion_requests_status_requested_at
  ON kergit_app.account_deletion_requests(status, requested_at DESC);

CREATE INDEX IF NOT EXISTS idx_account_deletion_requests_requested_at
  ON kergit_app.account_deletion_requests(requested_at DESC);

CREATE INDEX IF NOT EXISTS idx_account_deletion_requests_finished_at
  ON kergit_app.account_deletion_requests(finished_at DESC);

-- No direct client DB access.
REVOKE ALL ON TABLE kergit_app.account_deletion_requests FROM anon, authenticated;


-- account_email_reservations.sql
-- Blocks reuse of emails that have already been used for signup.
--
-- Assumptions:
-- - Signup goes through Nuxt server endpoint.
-- - Nuxt server normalizes email and computes email_hash.
-- - Plain email is not stored here.
-- - Deleted users cannot re-register with the same email.
--
-- email_hash should be computed from normalized email:
--   lower(trim(email))
--
-- Prefer server-side HMAC-SHA256 instead of plain SHA256 if possible.


CREATE TABLE IF NOT EXISTS kergit_app.account_email_reservations (
  id              uuid PRIMARY KEY DEFAULT gen_random_uuid(),

  email_hash      text NOT NULL,
  first_user_id   uuid,
  created_at      timestamptz NOT NULL DEFAULT now(),

  CONSTRAINT account_email_reservations_email_hash_sha256
    CHECK (email_hash ~ '^[0-9a-f]{64}$')
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_account_email_reservations_email_hash_unique
  ON kergit_app.account_email_reservations(email_hash);

CREATE INDEX IF NOT EXISTS idx_account_email_reservations_first_user_id
  ON kergit_app.account_email_reservations(first_user_id);

REVOKE ALL ON TABLE kergit_app.account_email_reservations FROM anon, authenticated;

COMMIT;
