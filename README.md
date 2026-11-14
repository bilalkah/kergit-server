# Kergit

Kergit is a realtime chat and voice system built around a C++ backend, protobuf over WebSocket, a Nuxt web client, and LiveKit for media.

[Project page](https://bilalkah.github.io/kergit-server/) · [Architecture](docs/ARCHITECTURE.md) · [Voice lifecycle](docs/VOICE.md)

## Status

Kergit is an advanced prototype / personal systems project. It is not production-ready. The core text and state-sync paths are usable, but setup, operations, and some voice flows still need cleanup.

## Stack

- C++ backend with Bazel
- Protobuf over WebSocket
- Nuxt 4 / Vue 3 web and admin clients
- LiveKit for voice/media
- PostgreSQL + Supabase for persistence and auth
- Redis for short-lived coordination
- Caddy for local and edge routing

## System Shape

- `server/`, `app/`, `net/`, and `control/` contain the backend runtime.
- `proto/` defines the shared client/server protocol.
- `clients/web/` is the main user-facing client.
- `clients/admin/` is an operator-only dashboard.
- `docker/` and `livekit/conf/` contain the local stack.

Core docs:

- `docs/ARCHITECTURE.md`
- `docs/SETUP.md`
- `docs/ENVIRONMENT.md`
- `docs/SECURITY.md`
- `docs/ROADMAP.md`

## Features

- authenticated sessions with Supabase-backed identity
- hub and channel state sync
- realtime text chat and invite links
- session-owned voice join / revoke / resume flows
- operator metrics and control surfaces

## Run Locally

```bash
git submodule update --init --recursive
cp .env.dev .env
./docker/run-server.sh
```

Stop the stack with:

```bash
./docker/stop-server.sh
```

Useful variants:

```bash
./docker/run-server.sh --prod
```

For backend-only operation, use the independent scripts:

```bash
./docker/services/backend/run-container.sh
./docker/services/backend/exec-server.sh
```

The committed `.env.dev` contains public, non-secret defaults and does not connect
PostgreSQL or Supabase. Put real local credentials only in the ignored `.env` when
testing persistence, authentication, or storage. See `docs/SETUP.md` for prerequisites,
production-mode requirements, and client-only workflows.

## Experimental / Current Limits

- voice/video UX and dynamic multi-node LiveKit behavior still need more validation
- admin and control-plane routes are operator-only, not public endpoints
- some internal protocol and build identifiers still use legacy `sercom` names for compatibility

## Build and Test

Create the ignored environment file from the safe tracked defaults before invoking
Bazel directly. CI performs the same copy.

```bash
cp .env.dev .env
bazel build //...
bazel test //...
cd clients/web && pnpm test
```

If the web protobuf artifacts are missing:

```bash
./clients/web/docker/generate-proto.sh
```

## Contributing

See `CONTRIBUTING.md`. Good early contributions are targeted tests, setup fixes, documentation improvements, and small reliability fixes.
