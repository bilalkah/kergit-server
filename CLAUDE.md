# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Sercom is a realtime chat and voice communication server written in C++23. It uses WebSocket transport (uWebSockets), Protocol Buffers for serialization, PostgreSQL for persistence, Redis for caching, and LiveKit for voice media. Clients are Nuxt (Vue) web apps managed as git submodules.

## Build System

Bazel 8.5.1 with `MODULE.bazel` for dependency management. GCC 13 toolchain targeting C++23.

### Key build commands

```bash
# Build the main server binary
bazel build --config=vanilla //server:fake_discord

# Build with debug symbols
bazel build --config=vanilla_dbg //server:fake_discord

# Build optimized
bazel build --config=vanilla_opt //server:fake_discord

# Run the server directly
bazel run --config=vanilla //server:fake_discord

# Build with SSL/QUIC support
bazel build --config=sslquic //server:fake_discord

# Run all tests
bazel test --config=testing //...

# Run a single test
bazel test --config=testing //utils/test:logger_test

# Run benchmarks
bazel test --config=benchmark //infra/security/test:jwt_verifier_benchmark_test

# Refresh compile_commands.json (for IDE integration)
bazel run :refresh_compile_commands

# Format C++ and BUILD files
bazel run :format

# Generate dependency graph SVG
bazel run :depgraph -- //server:fake_discord server.svg --scope=local
```

### Build configurations (.bazelrc)

- `vanilla` / `vanilla_dbg` / `vanilla_opt` — without SSL
- `sslquic` / `sslquic_dbg` / `sslquic_opt` — with BoringSSL + QUIC
- `testing` — debug + verbose test output, no caching
- `benchmark` — optimized, no caching

## Running the Full Stack

```bash
./docker/run-server.sh              # Full stack: Redis, LiveKit, Caddy, server, web+admin clients
./docker/run-server.sh --server-only # Just the C++ server in Docker
./docker/run-server.sh --prod       # Optimized build
./docker/run-server.sh --multi      # Multi-node LiveKit
./docker/stop-server.sh             # Stop everything
```

Runtime configuration via `.env` at repo root or environment variables. See `core/ServerConfig.h` and `ServerConfigFiller::fill_from_env` for all config knobs.

Default ports: web app :3000, admin :3001, Caddy TLS :443, socket server :9001, control HTTP :8081, LiveKit :7880.

## Architecture

### Server composition (`server/Server.cpp`)

`Server` owns three subsystems:
1. **AppStack** (`app/`) — application logic: services, commands, event queue, worker pool
2. **NetworkRouter** (`net/`) — one or more `NetworkStack` instances handling WebSocket I/O
3. **HttpServer** (`control/`) — control plane REST API (health, metrics)

### Request flow

Inbound: WebSocket → `TextWSServer` → `EventQueue` (high/low priority) → `WorkerPool` → `Dispatcher` → `ICommand` handler

Outbound: Command handler → `OutgoingMessage` → `NetworkRouter` → per-stack `OutgoingQueue` → `OutgoingWorker` → WebSocket send

### App layer (`app/`)

- **`AppStack`** — orchestrator; bootstraps DB, Redis, all services, managers, dispatcher, worker pool, LiveKit webhook server
- **`dispatcher/`** — maps `Envelope_Type` → `ICommand` implementations
- **`commands/`** — handler implementations organized by domain (session, hub, channel, message, activity, user, system, voice)
- **`services/`** — business logic (AuthService, UserService, HubService, ChannelService, PresenceService, VoiceService, InviteService) each with caches
- **`managers/`** — SessionManager (user↔connection mapping), SubscriptionManager (pub/sub topics)
- **`queue/`** — `EventQueue` with high/low priority and eviction policy
- **`worker/`** — `WorkerPool` with N threads, duplicate-command dedup

### Net layer (`net/`)

- **`NetworkStack`** — owns ConnectionRegistry, OutgoingQueue, TextWSServer, HeartbeatService, OutgoingWorker
- **`NetworkRouter`** — routes outgoing messages to the correct stack by `NetStackId`
- **`transport/websocket/`** — uWebSockets-based transport; binary protobuf only; PING fast-path in transport
- **`connection/`** — `ConnectionRegistry` with shared_mutex; auth states: UNAUTHED → AUTHONFLY → AUTHED
- **`outbound/`** — per-connection outbox (cap 128), slow-connection detection and eviction

### Infrastructure (`infra/`)

- **`persistence/`** — `PersistenceGateway` facade over libpqxx connection pools and repositories (User, Hub, Channel, Message)
- **`redis/`** — `RedisClient` wrapping redis-plus-plus
- **`security/`** — `SupabaseVerifier` for JWT validation (jwt-cpp + BoringSSL)

### LiveKit integration (`livekit/`)

- **`livekit/docs/`** — Docs about livekit webhook events and roomserviceapi
- **`token/`** — JWT generation for LiveKit participants
- **`webhook/`** — HTTP server receiving LiveKit room events
- **`crypto/`** — `E2EEKeyManager` for per-voice-channel encryption keys
- **`cli/`** — Twirp HTTP client for LiveKit RoomService
- **`routing/`** — `LivekitNodeRegistry` for node discovery and load balancing

### Domain models (`domains/`)

Core types: User, Hub, Channel, Message. Type-safe ID wrappers in `domains/ids/Ids.h` (HubId, ChannelId, UserId, MessageId, ConnId, NetStackId, GlobalConnId) with custom hash specializations.

### Protocol (`proto/`)

Organized by layer:
- `proto/envelope.proto` — top-level wrapper with Type enum (~62 types), version field, bytes payload
- `proto/domain/` — User, Hub, Channel, Message models
- `proto/command/` — client→server intents (session, activity, hub, channel, message, user)
- `proto/event/` — server→client notifications
- `proto/system/` — heartbeat

Each `.proto` has a `BUILD` defining `proto_library` → `cc_proto_library`.

### Clients (git submodules)

- `clients/web/` — Nuxt web client (sercom-web-client)
- `clients/admin/` — Nuxt admin dashboard (sercom-admin-client)

## Code Style

- C++23, GCC 13
- `.clang-format`: Google base style, 100 column limit, 4-space indent, sorted includes
- Format with `bazel run :format`
- Bazel BUILD files formatted with buildifier
