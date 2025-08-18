# Foundation Plan (v0.1)

A step‑by‑step roadmap to reach a secure, usable text‑chat baseline **before** any voice/video work. We’ll update this doc as we iterate.

---
## Guiding Principles
- Keep the gateway thin; put rules in middleware (rate‑limit, session, auth).
- Secure transport first; then identity; then product flows.
- Ship in vertical slices with clear acceptance criteria.

---
## Step 1 — Data Model & Migrations (Hubs/Channels/Membership)
**Goal:** Minimal schema to support hubs → channels → membership → messages.

**Deliverables**
- Tables: `users`, `hubs`, `channels`, `channel_members`, `messages`.
- Versioned SQL migrations (V001__init.sql, V002__indexes.sql).

**Acceptance**
- Can create a hub; can create a text channel under a hub.
- New user sees **no channels** until they join a hub.
- Basic indexes exist: `users(username)`, `messages(channel_id, sent_at desc)`.

**Notes**
- Keep fields minimal (owner_id, is_public, timestamps). No invites yet.

---
## Step 2 — Transport Security (WSS, Origin, Handshake Limits)
**Goal:** Ensure the server only talks to our web app and traffic is encrypted.

**Deliverables**
- WSS enabled (self‑signed in dev, real cert in staging/prod).
- Origin allow‑list at WebSocket **upgrade**; reject others.
- Handshake rate‑limit per IP (e.g., 5 upgrades/min).
- Message size caps & backpressure checks.
- On successful upgrade, server issues **ephemeral session id** via `welcome` message.

**Acceptance**
- Non‑allowed origins can’t connect.
- TLS verified (manual check; capture shows encrypted frames).
- Excess upgrades from one IP are throttled.

---
## Step 3 — Sessions & Auth (Sign Up / Sign In)
**Goal:** Users can register and log in safely; session lifecycle is enforced.

**Deliverables**
- Password hashing (Argon2id or bcrypt).
- Short‑lived **access token** (JWT) + in‑memory session store with TTL.
- Commands: `register`, `login`, `logout`.
- Auth middleware guarding protected commands.
- Structured error responses (codes + human messages).

**Acceptance**
- New user can register, then login, then receives access token.
- Protected command fails when unauthenticated; succeeds when authenticated.
- Access token expiry enforced; refresh (optional) deferred to later.

**Notes**
- Keep refresh tokens out for now (add later if needed). Keep it simple.

---
## Step 4 — Onboarding Flow (Blank Home, Join/Create Hub)
**Goal:** App opens to a blank state; users must create or join a hub first.

**Deliverables**
- Web client states: `unauthenticated → authenticated → no-hub`.
- Screens: **Create Hub**, **Join Hub** (via invite code or hub id).
- Server commands: `hub.create`, `hub.join`, `hub.listMine`.
- No auto‑subscription to any channel; user sees empty until in a hub.

**Acceptance**
- Fresh account logs in and sees empty home.
- Creating a hub moves user to hub view (still no channels shown).
- Joining by code/id adds membership; hub appears in the list.

**Notes**
- Invite codes can be random short strings stored on the hub.

---
## Step 5 — Channels & Text Messaging (MVP)
**Goal:** Create/list/join text channels inside a hub; send/receive messages.

**Deliverables**
- Commands: `channel.create`, `channel.list`, `channel.join`, `channel.leave`.
- Commands: `message.send`, `message.history` (paged by `sent_at desc`).
- Presence kept minimal (no typing indicators yet).

**Acceptance**
- User in a hub can create a channel, join it, and send/receive messages.
- History loads in pages; payload size limits enforced.

**Notes**
- No general channel by default. All channels explicit opt‑in.

---
## Step 6 — Security Hardening (Round 1)
**Goal:** Reasonable defense‑in‑depth before scale tests.

**Deliverables**
- Per‑command rate limits (strict on `login/register`).
- Input validation schemas; max payload size per command.
- Audit log for auth events (register/login/logout failures/success).
- Basic metrics (connections, auth_ok/fail, cmd_rate, cmd_latency_ms).

**Acceptance**
- Brute‑force login attempts are throttled.
- Malformed payloads get structured errors (no crashes).
- Metrics visible via logs or a simple dashboard.

---
## Stretch (Defer until the above is solid)
- Invite/role management (owner/admin/member).
- Refresh tokens with revoke list.
- Pagination cursors, message edits/deletes.
- Native clients (desktop/mobile) with mTLS.

---
## Change Log
- v0.1 — Initial plan drafted.

---
## How We’ll Work This Plan
- We’ll pick **one step at a time**.
- Each step has small PRs: server, web, tests.
- We’ll update this doc (checklists + change log) as we ship.

