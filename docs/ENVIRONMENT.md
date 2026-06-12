# Environment Variables

Copy `.env.example` to `.env` and replace the placeholders with values for your own environment.

This document focuses on the runtime variables that matter for a basic local or public deployment path. All example values below are placeholders only.

## Required Variables

| Variable | Example value | Used by | Notes |
|---|---|---|---|
| `DB_HOST` | `db.example.com` | backend | Required for PostgreSQL connectivity |
| `DB_PORT` | `5432` | backend | PostgreSQL port |
| `DB_NAME` | `postgres` | backend | Database name |
| `DB_USER` | `replace-with-your-db-user` | backend | Database user |
| `DB_PASSWORD` | `replace-with-your-db-password` | backend | Database password |
| `DB_SSL` | `1` | backend | Enable SSL when your DB requires it |
| `DB_SSL_ROOT_CERT` | `certs/database/root.crt` | backend | Local path to your database provider's CA certificate when `DB_SSL=1`; intentionally untracked |
| `SUPABASE_JWT_CURRENT_KEY` | `{"kty":"EC","crv":"P-256","kid":"replace-me","x":"replace-me","y":"replace-me","alg":"ES256","use":"sig"}` | backend auth | Supabase JWK used to verify auth tokens |
| `WEB_DOMAIN` | your canonical app origin | backend + Caddy | Sole public app origin; must be HTTPS with a lowercase host and no path, trailing slash, credentials, query, fragment, or explicit `:443` |
| `NUXT_PUBLIC_SUPABASE_URL` | your canonical Supabase project origin | web client + backend + Caddy | Sole Supabase origin with the same canonical requirements; the backend issuer and Caddy CSP entry are derived from it |
| `NUXT_PUBLIC_SUPABASE_ANON_KEY` | `replace-with-your-public-anon-key` | web client + server routes | Public anon key used by browser/server auth helpers |
| `SUPABASE_SERVICE_ROLE_KEY` | `replace-with-your-service-role-key` | web server routes | Required for attachment and admin-style server operations |
| `LIVEKIT_API_KEY` | `example-key` | backend + LiveKit webhook config | Must match a key in the LiveKit YAML files |
| `LIVEKIT_API_SECRET` | `replace-with-your-own-long-random-livekit-secret` | backend + LiveKit webhook config | Secret paired with `LIVEKIT_API_KEY` |
| `LIVEKIT_WEBHOOK_URL` | `http://server-node:8080/webhook` | local Docker LiveKit config | `docker/run-server.sh` renders this into the local LiveKit YAML copy before startup |
| `LIVEKIT_NODES` | JSON node registry | backend + startup + Caddy + admin | Defines every Compose-managed LiveKit node and its signal, RTC, UDP, metrics, and production IP settings |

Public invite URLs, LiveKit node URLs, websocket origin checks, and the Caddy site address are
derived from `WEB_DOMAIN`. The Supabase JWT issuer is derived by appending `/auth/v1` to
`NUXT_PUBLIC_SUPABASE_URL`.

Retired public URL variables are rejected at startup instead of being treated as overrides.

## Strongly Recommended Variables

| Variable | Example value | Used by | Notes |
|---|---|---|---|
| `SUPABASE_EXPECTED_AUD` | `authenticated` | backend auth | Tightens JWT verification |

`LIVEKIT_NODES` must be a non-empty JSON array. Each entry requires `id`, `signal_port`,
`rtc_tcp_port`, `rtc_udp_start`, `rtc_udp_end`, and `prometheus_port`. Production startup also
requires `node_ip`. Node IDs become Compose service names and stable public paths:
`${WEB_DOMAIN}/livekit/{id}` and `${WEB_DOMAIN}/admin-livekit-metrics/{id}`.
IDs and TCP ports must not collide with shared stack services, and RTC UDP ranges must not
overlap. Apply registry changes by restarting the stack; hot node add/remove is not supported.

## Local Docker Defaults

These already have sensible defaults in code or Compose, but you may override them in `.env`:

| Variable | Example value | Notes |
|---|---|---|
| `LISTEN_HOST` | `0.0.0.0` | Websocket listener host |
| `SOCKET_PORT` | `9001` | Websocket listener port |
| `SOCKET_PATTERN` | `/*` | Websocket path pattern |
| `WS_ORIGIN_POLICY_PATH` | `config/ws_origin_policy.yaml` | Trusted proxy CIDR policy file; the allowed browser origin comes from `WEB_DOMAIN` |
| `CONTROL_HOST` | `0.0.0.0` | Control HTTP bind host |
| `CONTROL_PORT` | `8081` | Control HTTP bind port |
| `REDIS_HOST` | `redis-node` | Local Docker Redis hostname |
| `REDIS_PORT` | `6379` | Redis port |
| `METRICS_LOG` | `0` | Enable periodic backend metrics logging when set to `1` |

The bundled Redis container is intentionally non-persistent. It stores ephemeral application
coordination and LiveKit cluster state; persisting that state across a full-stack restart can
restore stale rooms and participants. Durable application data belongs in PostgreSQL.

## Caddy / Ingress Variables

The public site address, browser origin check, websocket CSP origin, and Supabase CSP origin are
derived from the two required canonical origins above.

| Variable | Example value | Notes |
|---|---|---|
| `CADDY_TLS_CERT_FILE` | `/certs/dev/localhost.pem` | TLS certificate path inside the Caddy container |
| `CADDY_TLS_KEY_FILE` | `/certs/dev/localhost-key.pem` | TLS key path inside the Caddy container |

Docker-internal Caddy upstreams are fixed by `docker/docker-compose.yml`. They intentionally cannot
be overridden from `.env`, preventing stale service names from breaking Docker DNS after upgrades.

## Admin UI Variables

| Variable | Example value | Notes |
|---|---|---|
| `NUXT_ADMIN_APP_BASE_URL` | `/admin/` | Admin app base path |
| `NUXT_PUBLIC_ADMIN_METRICS_BASE` | `/admin-api` | Public admin metrics route prefix |
| `NUXT_PUBLIC_ADMIN_LIVEKIT_METRICS_BASE` | `/admin-livekit-metrics` | Public LiveKit metrics route prefix |
| `ADMIN_ALLOWED_CIDR` | `127.0.0.1/32` | Explicit original-client-IP admin allowlist; production rejects `private_ranges` |
| `ADMIN_METRICS_PROXY_TARGET` | `http://host.docker.internal:8081` | Direct admin-dev proxy target |
| `ADMIN_LIVEKIT_PROXY_HOST` | `host.docker.internal` | Direct admin-dev LiveKit metrics proxy host |

## Optional Tuning Variables

| Variable | Example value | Notes |
|---|---|---|
| `MAX_CONNECTIONS` | `255` | Max websocket connections per stack |
| `MAX_MESSAGE_SIZE` | `262144` | Max inbound websocket payload size |
| `OUTBOUND_QUEUE_CAPACITY` | `50000` | Outbound queue size |
| `WORKER_THREADS` | `3` | Worker pool size |
| `MAX_SESSIONS_PER_USER` | `0` | `0` means unlimited |
| `EVENT_QUEUE_CAPACITY` | `30000` | Event queue size |
| `DB_WRITE_QUEUE_CAPACITY` | `10000` | Async message write queue size |
| `DB_WRITE_MAX_RETRIES` | `3` | Async DB write retry count |
| `DB_WRITE_RETRY_MS` | `25` | Async DB write retry delay |
| `LIVEKIT_RECONCILE_INTERVAL_SEC` | `10` | Voice reconcile loop interval |
| `LIVEKIT_MISSING_CLEAR_SEC` | `60` | Clear stale app voice membership only after every LiveKit endpoint continuously reports it absent for this duration |
| `VOICE_E2EE_STORAGE_SECRET` | `replace-with-a-separate-secret` | Overrides the secret used to protect stored E2EE room keys |
| `VOICE_E2EE_KEY_TTL_SEC` | `86400` | E2EE room-key TTL |
| `VOICE_E2EE_REKEY_GUARD_SEC` | `60` | Rekey guard interval |
| `SUPABASE_ATTACHMENTS_BUCKET` | `chat-attachments` | Storage bucket used by chat uploads |
| `SUPABASE_ATTACHMENT_SIGN_TTL_SEC` | `900` | Signed attachment URL TTL |
| `CHAT_ATTACHMENT_MAX_FILES` | `6` | Client/server attachment limit |
| `CHAT_ATTACHMENT_MAX_SIZE_BYTES` | `15728640` | Max attachment size |
| `CHAT_LINK_PREVIEW_TIMEOUT_MS` | `3000` | Link-preview fetch timeout |
| `CHAT_LINK_PREVIEW_CACHE_TTL_SEC` | `900` | Link-preview cache TTL |
| `CHAT_LINK_PREVIEW_MAX_HEAD_BYTES` | `262144` | Head/body read cap |
| `CHAT_LINK_PREVIEW_MAX_OEMBED_BYTES` | `131072` | OEmbed read cap |
| `CHAT_LINK_PREVIEW_MAX_REDIRECTS` | `3` | Redirect cap |
| `CHAT_LINK_PREVIEW_CACHE_MAX_ENTRIES` | `1000` | Link-preview cache size |

## Do Not Commit

Never commit:

- your real `.env`
- private keys or generated certs under `certs/`
- your database provider's CA certificate under `certs/database/`
- production-only Caddy or LiveKit overrides
