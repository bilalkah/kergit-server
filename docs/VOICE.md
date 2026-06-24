# Voice Subsystem — Flows & Lifecycle

Server-authoritative realtime voice. The C++ backend is the single source of truth for
voice state; LiveKit is the media plane only. Voice ownership is **per-session** (a user may
have multiple app sessions, but only one owns the active voice connection). LiveKit
membership is never trusted as ground truth — webhook events are reconciled against server
state.

Key services: [`VoiceService`](../app/services/voice/VoiceService.cpp),
[`ChannelKeyService`](../app/services/voice/ChannelKeyService.cpp),
[`VoiceSessionManager`](../app/services/voice/VoiceSessionManager.cpp), and the reconcile
evidence tracker. Join is driven by
[`JoinVoiceChannelCommand`](../app/commands/activity/JoinVoiceChannelCommand.cpp).

---

## 1. Voice ownership lifecycle (server-authoritative, per-session)

```mermaid
stateDiagram-v2
    [*] --> Idle: no voice session

    Idle --> Owner: participant_joined webhook<br/>(correlated intent)<br/>sessions_.join()

    Owner --> Owner: app WS drop / refresh<br/>on_session_destroyed → reconcile request ONLY<br/>(recovery-first: membership KEPT)
    Owner --> Owner: takeover (different session, same channel)<br/>verified_kick_user + new owner session
    Owner --> Owner: switch channel (same session)<br/>kick old + join new

    Owner --> Idle: participant_left (trusted owner session)<br/>sessions_.leave() + clear resume_id
    Owner --> Idle: reconcile force_local_leave<br/>(confirmed gone after grace)
    Owner --> Idle: room_finished → reconcile → empty

    note right of Owner
        Invariant: one owner SESSION per user.
        Voice keyed by UserId; multiple app
        sessions may exist, only one owns voice.
    end note
```

---

## 2. Join command → token issue (`JoinVoiceChannelCommand` → `join_voice`)

```mermaid
flowchart TD
    START([JoinVoiceChannelRequest]) --> V{channel valid,<br/>VOICE type,<br/>hub member?}
    V -->|no| ERR[CommandError:<br/>NOT_FOUND / FORBIDDEN /<br/>INVALID_ARGUMENT]
    V -->|yes| OWN{existing voice owner?}

    OWN -->|"different session<br/>(taking_over)"| KICK1[verified_kick_user old_channel<br/>+ mark_channel_takeover if last one<br/>+ voice_self_revoked to old session]
    OWN -->|"same session,<br/>other channel<br/>(switching_channel)"| KICK2[verified_kick_user old_channel<br/>+ self_status_disconnected to requester]
    OWN -->|none / same channel| JV
    KICK1 --> KFAIL{kick ok?}
    KICK2 --> KFAIL
    KFAIL -->|no| ERR2[CommandError:<br/>INTERNAL_ERROR<br/>'Failed to revoke previous session']
    KFAIL -->|yes| JV

    JV[generate intent_nonce<br/>stage pending_join_intent] --> JOIN[join_voice channel,user,session,nonce]

    JOIN --> RKB{rekey_blocks_join?<br/>rekey in progress & not empty}
    RKB -->|yes| ERR3[error: voice_rekey_in_progress]
    RKB -->|no| NODE{any_node available?}
    NODE -->|no| ERR4[no token]
    NODE -->|yes| CLR[clear_channel_remote_missing_confirmation] --> ACQ[acquire_for_join<br/>see Diagram 3]

    ACQ --> AOK{key acquired?}
    AOK -->|no| ERR5[error: voice_key_unavailable /<br/>voice_rekey_in_progress]
    AOK -->|yes| MINT[mint_participant_token<br/>identity=user, room=channel,<br/>can_publish/subscribe, ttl=10m]
    MINT --> RESUME[resume_registry_.rotate user]
    RESUME --> TOK([return token + livekit_url +<br/>e2ee_key + key_index + resume_id])
    TOK --> EMIT[voice_token_issued → client connects to LiveKit]
```

---

## 3. E2EE key acquisition (`acquire_for_join`) — includes the join-rotation debounce

The debounce lives **strictly inside** the `other_members_present` branch, which is only
reached when the user is **not** already a member. Refresh / reconnect / takeover all
short-circuit on `already_member` and never touch it. The debounce only collapses a single
user's repeated join-rotations on the same channel within `VOICE_E2EE_JOIN_ROTATE_DEBOUNCE_SEC`
(default 15s) — i.e. the failed-rejoin storm — and never suppresses a departure rotation.

```mermaid
flowchart TD
    A[acquire_for_join channel,user] --> M{already_member?<br/>user_channel == channel}
    M -->|yes<br/>resume/reconnect/takeover| GET1[get_key → current key<br/>no rotation]

    M -->|no| OMP{other_members_present?<br/>channel not empty}

    OMP -->|"yes (members A,B…)"| DEB{record_join_and_should_<br/>debounce_rotation?<br/>same user join-rotated < 15s}
    DEB -->|"yes (retry storm /<br/>fast rejoin)"| GET2[get_key → reuse current key<br/>log e2ee_join_rotation_debounced<br/>NO broadcast, NO churn]
    GET2 --> KMISS{key present?}
    KMISS -->|no| ROT
    KMISS -->|yes| OK
    DEB -->|"no (first/genuine new join)"| ROT[rotate_and_broadcast 'participant_join'<br/>fresh random key, index++,<br/>persist, VOICE_KEY_UPDATE to hub]

    OMP -->|"no (empty / first join)"| GET3[get_key]
    GET3 --> HAS{key in memory?}
    HAS -->|yes| PERSIST[persist_to_storage] --> OK
    HAS -->|no| EMPTY{is_channel_effectively_empty?<br/>server + LiveKit agree}
    EMPTY -->|"yes (empty)"| GEN[get_or_create_key → FRESH key<br/>log e2ee_key_generated<br/>reason=room_effectively_empty]
    EMPTY -->|"no (active room),<br/>key in Redis"| LOAD[load_from_storage → set_key<br/>log e2ee_key_loaded source=redis]
    EMPTY -->|"active room, key gone<br/>from memory AND Redis"| RECOVER[SEAMLESS recovery:<br/>get_or_create_key → FRESH key<br/>broadcast VOICE_KEY_UPDATE to members<br/>log e2ee_key_recovered_active_room<br/>NO kick — nobody dropped]

    GET1 --> OK([AcquireResult.key])
    ROT --> OK
    GEN --> OK
    LOAD --> OK
    RECOVER --> OK
```

---

## 4. LiveKit webhook dispatch (`handle_livekit_event`)

```mermaid
flowchart TD
    WH([LiveKit webhook event]) --> SEEN{mark_webhook_event_seen?<br/>Redis dedup}
    SEEN -->|"duplicate"| DROP[ignore]
    SEEN -->|new| STALE{ROOM_* & stale?<br/>older than last room event}
    STALE -->|yes| RCS[reconcile request<br/>'stale_room_event']
    STALE -->|no| TYPE{event.type}

    TYPE -->|ROOM_STARTED| RS[increment_room on node]
    TYPE -->|ROOM_FINISHED| RF{consume_channel_takeover_guard?}
    RF -->|yes| RFG[room_finished_guarded<br/>skip — takeover in progress]
    RF -->|no| RFR[reset confirmations +<br/>reconcile 'room_finished']

    TYPE -->|PARTICIPANT_JOINED| PJ[bind/confirm node<br/>log node_mismatch if differs<br/>→ handle_participant_joined<br/>→ increment_user if entered]
    TYPE -->|"PARTICIPANT_LEFT /<br/>CONNECTION_ABORTED"| PL[→ handle_participant_left<br/>→ decrement_user if left<br/>node_mismatch → reconcile]
    TYPE -->|"TRACK_PUBLISHED /<br/>UNPUBLISHED / EGRESS_*"| TEL[livekit_telemetry_event<br/>no state change]
```

---

## 5. `handle_participant_joined` — intent correlation & ownership

```mermaid
flowchart TD
    PJ([participant_joined]) --> READ[read pending_join_intent]
    READ --> COR{has_correlated_intent?<br/>channel + session_id + nonce match<br/>OR sole session fallback}

    COR -->|no| OWNCHK{already_owner_here?<br/>same session already owns<br/>this channel}
    OWNCHK -->|"yes (confirming dup<br/>after reconcile adopt)"| SKIP[log join_already_owner<br/>return — never re-kick]
    OWNCHK -->|no| MISMATCH[log join_intent_mismatch<br/>reconcile request<br/>kick_user — remove untrusted]

    COR -->|yes| JOIN[sessions_.join channel,user,session<br/>apply muted/deafened from intent]
    JOIN --> FIRST{first_join_in_channel?}
    FIRST -->|yes| SNAP[set_channel_started_at<br/>publish_voice_snapshot]
    FIRST -->|no| UP
    SNAP --> UP[publish_voice_participant_upsert]
    UP --> SELF[publish_self_status connected]
    SELF --> RESYNC[resync_voice_key_for_user<br/>deliver LIVE key under rotation lock]
    RESYNC --> INTENT[clear / update pending intent<br/>handle channel-switch correlation]
```

---

## 6. `handle_participant_left` — trusted exit vs. switch vs. ignore

```mermaid
flowchart TD
    PL([participant_left]) --> CALC[compute flags:<br/>matches_owner_session,<br/>nonce_mismatch_to_pending]
    CALC --> WHICH{classify}

    WHICH -->|"leaving_current_voice<br/>(owner session leaves<br/>current channel)"| LV[sessions_.leave<br/>publish_participant_remove<br/>self_status disconnected<br/>clear resume_id]
    LV --> EMP1{became_empty?}
    EMP1 -->|yes| CE1[on_channel_empty<br/>clear key + state]
    EMP1 -->|no| ROT1[rotate_and_broadcast<br/>'participant_left'<br/>lock out departed member]

    WHICH -->|"old_leave_for_switch<br/>(from-channel of a<br/>channel switch)"| SW[publish_participant_remove<br/>mark old_leave_seen]
    SW --> EMP2{channel empty?}
    EMP2 -->|yes| CE2[on_channel_empty]
    EMP2 -->|no| ROT2[rotate_and_broadcast<br/>'participant_switch_away']

    WHICH -->|"neither<br/>(stale / foreign / wrong session)"| IGN[log ignored_participant_left<br/>reconcile request<br/>NO state change — never trust blindly]
```

---

## 7. Reconcile — the 6 cases

Reconcile is the safety net that keeps server state and LiveKit convergent **without ever
acting on bad data**. It never kicks healthy users and never aggressively cleans up voice on
app-session loss.

```mermaid
flowchart TD
    subgraph TRIG[Triggers]
        A1[App WS drop<br/>on_session_destroyed]
        A2[Webhook: ignored/mismatch/<br/>node_mismatch/room_finished]
        A3[Periodic loop<br/>RECONCILE_INTERVAL_SEC]
    end
    A1 --> RC[reconcile_channel_state]
    A2 --> RC
    A3 --> RC

    RC --> Q{all node queries ok?}
    Q -->|"no (inconclusive)"| NOP[reset confirmations, do nothing<br/>CASE 3/6: never act on bad data]
    Q -->|yes| P{participant present in LiveKit?}

    P -->|yes, known locally| KEEP[clear missing-evidence, KEEP voice<br/>CASE 2: app gone, voice alive → recover]
    P -->|"no (missing)"| G{grace elapsed?<br/>2 obs spanning MISSING_CLEAR_SEC}
    G -->|not yet| DEFER[record evidence / defer<br/>CASE 3 & 5]
    DEFER -.->|recovers| KEEP
    G -->|"yes, still gone"| LEAVE[force_local_leave<br/>CASE 4: confirmed gone → rotate]

    P -->|present, unknown locally| CORR{intent/session correlates?}
    CORR -->|yes| REJOIN[synthetic participant_joined = recover<br/>CASE 5]
    CORR -->|no| KICKR[remove untrusted participant<br/>CASE 6]
```

| Case | Situation | Action |
|---|---|---|
| 1 | Normal join | Single voice owner per user (set by `handle_participant_joined`) |
| 2 | App gone, voice alive in LiveKit | KEEP voice → recover on reconnect |
| 3 | Inconclusive node queries | Do nothing; never act on bad data |
| 4 | Confirmed gone (trusted left / grace elapsed) | `force_local_leave` + rotate |
| 5 | App + LiveKit both lost then restored | Synthetic `participant_joined` = recover |
| 6 | Present in LiveKit, no correlating intent | Remove untrusted participant |

---

## 8. E2EE key state machine (per channel)

```mermaid
stateDiagram-v2
    [*] --> NoKey
    NoKey --> Active: first join into empty room<br/>get_or_create_key (fresh, idx 0)
    NoKey --> Active: recovery — load_from_storage / restore
    NoKey --> Active: key lost on a live room (join path) →<br/>SEAMLESS recovery: mint fresh key +<br/>broadcast VOICE_KEY_UPDATE (no kick)
    NoKey --> NoKey: restart recovery, no stored key →<br/>DEFER (no kick): members keep their key,<br/>re-key on next join/leave

    Active --> Active: genuine new join → rotate_key (idx++)<br/>broadcast VOICE_KEY_UPDATE
    Active --> Active: participant_left / switch_away /<br/>force_left → rotate (backward secrecy)
    Active --> Active: debounced join (same user < 15s)<br/>REUSE key, no rotation, no broadcast
    Active --> Active: resume/reconnect → reuse key

    Active --> NoKey: channel empty (on_channel_empty)<br/>clear_channel: key + storage + guards

    note right of Active
        Rotation always mints FRESH random
        material (never a ratchet). Index wraps
        mod keyring size (16) for overlap.
        resync_to_user delivers the live key
        on every confirmed join.
    end note
```

---

## Convergence invariants

- **One key per channel.** `acquire_for_join` returns either the channel's current key or a
  fresh rotation — there is no path that returns per-user-divergent material.
- **Departures always rotate.** `participant_left` / `participant_switch_away` /
  `participant_force_left` rotate directly via `rotate_and_broadcast`, never through
  `acquire_for_join`, so the debounce can never suppress a lock-out rotation.
- **Confirmed joins re-sync.** Every real `participant_joined` calls
  `resync_voice_key_for_user`, delivering the live key under the rotation lock — so a stale
  token key always self-heals.
- **Recovery-first.** App-session loss never removes voice membership; it only requests a
  reconcile. Membership is removed only on a trusted `participant_left` or a grace-confirmed
  reconcile.

## Relevant env vars

| Var | Default | Effect |
|---|---|---|
| `VOICE_E2EE_KEY_TTL_SEC` | 86400 | At-rest key TTL in Redis |
| `VOICE_E2EE_REKEY_GUARD_SEC` | 30 | Window a forced rekey blocks joins |
| `VOICE_E2EE_JOIN_ROTATE_DEBOUNCE_SEC` | 15 | Suppress repeat join-rotations by the same user on a channel |
