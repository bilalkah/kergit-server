# JoinVoiceChannelCommand + Webhook Truth Flow

## Summary
`JoinVoiceChannelCommand` is intent/control only. LiveKit webhook handlers are the only confirmation path for
voice state mutation, database writes, participants/activity fanout, and final self-status.

## Command vs Webhook Responsibilities

### Command phase (`VOICE_JOIN`)
- Validate requester/session/channel/hub membership.
- Handle takeover/switch control actions.
- Write pending intent to Redis.
- Mint and return token when command-phase prerequisites succeed.
- Return early on failures; do not mutate authoritative voice state.

### Webhook phase (authoritative truth)
- Validate webhook authenticity and event/session correlation.
- Mutate `voice_sessions`.
- Persist DB join/leave.
- Publish `VOICE_CHANNEL_PARTICIPANTS` + `VOICE_ACTIVITY_EVENT`.
- Publish final `VOICE_SELF_STATUS` to user sessions.
- Execute channel-finish final cleanup.

## Input Contract
- Envelope: `VOICE_JOIN`
- Payload: `JoinVoiceChannelRequest`
- Command path: join/switch intent only (no state enum).

## Validation Gate (Command)
Order:
1. Resolve authenticated user by requester connection.
2. Resolve requester app session id.
3. Resolve channel by `channel_id`.
4. Validate channel belongs to `hub_id`.
5. Validate channel type is `VOICE`.
6. Validate requester is hub member.

Fail fast:
- auth/session failures: drop unauthorized
- logical failures: command error

No command-side mutation before this gate passes.

## Metadata + Pending Intent Model

### Token metadata
Participant token metadata carries JSON:
- `node_id`
- `app_session_id`
- `intent_nonce`

### Redis pending intent
Keyed by user (and scoped for join/switch), JSON payload includes:
- user id
- requester `session_id`
- nonce
- target channel
- optional previous channel (switch/takeover)
- session media flags (`muted`, `deafened`)
- correlation flags for switch (`old_leave_seen`, `new_join_seen`)
- expiration unix timestamp

TTL:
- `expires_in + 10s`

### Webhook correlation rule
Webhook join applies only if metadata (`session_id`, `intent_nonce`) matches pending intent.

Fallback:
- If metadata is missing and exactly one active app session exists for user, allow single-session recovery.
- Otherwise reject and kick participant.

### Dedup
- Store `event_id` in Redis seen-key with 24h TTL.
- Ignore duplicate webhook events idempotently.

## Kick Policy (Takeover/Switch)
Verified kick helper:
1. `ListParticipants(room)` pre-check.
2. If target already absent => success.
3. If present => call `RemoveParticipant`.
4. Sleep 300ms once.
5. `ListParticipants(room)` re-check.
6. If still present => fail.

Rules:
- Never mint/send new usable token outcome if verified kick fails.
- On takeover/switch kick failure: delete pending intent and return internal error.

## Command Paths

### 1) JOIN: first connect (no existing owner)
Command phase:
1. Mint token payload data.
2. Write pending-join intent (`ttl = expires_in + 10s`).
3. Send `VOICE_TOKEN_ISSUED` to requester.

Command phase does not:
- mutate `voice_sessions`
- persist DB join
- broadcast participants/activity
- send final self-status

Webhook confirmation (`participant_joined`):
1. Dedup by `event_id`.
2. Validate pending intent + metadata.
3. Create ownership in `voice_sessions`.
4. Persist DB join.
5. Fanout participants + `ACTIVITY_JOINED`.
6. Send final `VOICE_SELF_STATUS(connected=true, is_owner=true)`.
7. Consume pending record.

If pending is missing/expired/mismatched:
- kick participant
- do not mutate state

### 2) JOIN: takeover (different owner session exists)
Command phase order:
1. Detect old owner session/channel.
2. Verify-kick old participant from old channel.
3. After successful kick:
  - send `VOICE_SELF_REVOKED` to old owner session connections
  - send provisional disconnected self-status to old owner session connections
4. Mint token for requester.
5. Write pending join intent.
6. Send `VOICE_TOKEN_ISSUED` to requester.

If kick fails:
- no token returned
- no state mutation
- return internal error

Webhook phase:
- old `participant_left`: persist leave + leave fanout
- new `participant_joined`: validate pending, apply join, persist join, joined fanout, final self-status

Both old-leave and new-join must be visible in DB via webhook-confirmed paths.

### 3) JOIN: channel switch (same app session, different channel)
Command phase order:
1. Detect `from_channel` and `to_channel`.
2. Verify-kick same user from `from_channel`.
3. Mint token for `to_channel`.
4. Write pending-switch intent with `from_channel`, `to_channel`, session media flags.
5. Send token.

Webhook phase:
- `participant_left(from_channel)`:
  - persist leave
  - fanout old channel participants + `ACTIVITY_LEFT`
  - set `old_leave_seen=true`
  - if empty => `on_channel_empty` (E2EE cleanup only)
- `participant_joined(to_channel)`:
  - validate pending switch metadata
  - create/move ownership for same session
  - persist join
  - fanout new channel participants + `ACTIVITY_JOINED`
  - send final connected self-status
  - set `new_join_seen=true`
- consume pending-switch when both flags are true.

Out-of-order old/new webhooks must stay idempotent.

## Webhook Transport/Auth
- `LivekitWebhookServer` remains thin:
  - read raw body
  - verify signature (`Authorization` JWT + payload hash)
  - parse and normalize
  - dispatch callback
- Business logic belongs in voice webhook handler (`VoiceService` path), not transport layer.

## Event Model Requirements
Normalized webhook event includes:
- `event_id`
- `participant_sid`
- `participant_metadata` (raw)
- parsed metadata fields (`node_id`, `app_session_id`, `intent_nonce`)
- event type, user id, channel id, timestamp

## Cleanup Semantics
- `on_channel_empty(channel)`:
  - E2EE cleanup/rotation only.
- `on_channel_finish(channel)`:
  - final room/node/channel cleanup (webhook-driven, e.g. `room_finished`).

## Session-Scoped Mute/Deafen Rules
- Mute/deafen belong to app session context.
- Preserve mute/deafen only when the same app session switches channels.
- On takeover (different session), use new session's state (default `false/false` unless explicitly updated).
- `VoiceChannelActivity` remains source of mute/deafen updates and replay.

## Reliability Requirements
- Duplicate webhook events ignored by `event_id` dedup.
- Missing/expired/mismatched pending intent => reject and kick.
- Kick "already absent" is success.
- Command does not block waiting webhook.

## Client Rollout Notes
- Leave path is local LiveKit disconnect + webhook-confirmed server updates.
- Client must handle revoke/disconnect sequencing and non-immediate join confirmation.
