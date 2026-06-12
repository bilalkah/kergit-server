# Kergit

Kergit is a realtime chat and voice system built around a C++ backend, protobuf over WebSocket, a Nuxt web client, and LiveKit for media.

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
cp .env.example .env
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

Replace the placeholders in `.env` before starting the stack. See `docs/SETUP.md` for
prerequisites, production-mode requirements, and client-only workflows.

## Experimental / Current Limits

- voice/video UX and dynamic multi-node LiveKit behavior still need more validation
- admin and control-plane routes are operator-only, not public endpoints
- some internal protocol and build identifiers still use legacy `sercom` names for compatibility
- some full backend targets still need C++ standard/build-setting cleanup

## Build and Test

The default full backend build currently fails because some targets still compile
as C++17 while shared code requires C++20/23. This is a known limitation, not a
working release check.

```bash
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
