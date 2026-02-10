# Supabase Verifier Optimization Log

This document tracks notable changes to the Supabase JWT verifier.

## 2026-02-10
- Refactored to a single ES256 verification flow in `infra/security/token/SupabaseVerifier.cpp`.
- Removed fast/slow dual-path logic and custom scanners/decoders.
- Verification now uses:
  - `jwt::decode()` for header/payload parsing and claims extraction.
  - OpenSSL EVP for ES256 signature verification with cached `EVP_PKEY`.
- Enforced `nbf` when present and added explicit error mappings:
  - Unknown `kid` -> `KeyNotFound`
  - `nbf` in future -> `TokenNotYetValid`
  - Issuer mismatch -> `IssuerMismatch`
  - Audience mismatch -> `AudienceMismatch`
- Environment gates are limited to:
  - `SUPABASE_JWT_CURRENT_KEY` (required)
  - `SUPABASE_JWT_STANDBY_KEY` (optional)
  - `SUPABASE_EXPECTED_ISS` (optional)
  - `SUPABASE_EXPECTED_AUD` (optional)
