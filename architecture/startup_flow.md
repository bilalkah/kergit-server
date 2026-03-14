# Server Startup Flow

This file reflects the current startup path implemented by:
- `docker/run-server.sh`
- `docker/docker-compose.yml`
- `server/main.cpp`
- `server/Server.cpp`

## End-to-End Flow

```mermaid
flowchart TB
    START[./docker/run-server.sh]

    START --> REDIS[docker compose up -d redis]
    REDIS --> LK[LiveKit\n(single: node1 or multi: node1+node2)]
    LK --> CADDY[docker compose up -d caddy]
    CADDY --> DEV[docker compose up -d ubuntu-dev]
    DEV --> ADMIN[Start admin Nuxt container (:3001)]
    ADMIN --> WEB[Start web Nuxt container (:3000)]
    WEB --> BAZEL[docker exec kergit-ubuntu-server\nbazel run //server:fake_discord]
    BAZEL --> MAIN[server/main.cpp]
```

## Infrastructure Brought Up by Script

From `docker/run-server.sh`:
1. Redis
2. LiveKit
- default: `livekit-node1`
- `--multi`: `livekit-node1` + `livekit-node2`
3. Caddy
4. Ubuntu dev container
5. Admin client container
6. Web client container
7. Foreground `bazel run //server:fake_discord`

Important:
- Script uses `exec docker exec -it ... bazel run ...`, so it stays attached to the server process.

## Caddy Routing (`docker/caddy/conf.dev/Caddyfile` / `docker/caddy/conf.prod/Caddyfile`)

```mermaid
flowchart LR
    IN[:443 TLS] --> WS[/ws*]
    IN --> LK[/livekit*]
    IN --> FE[/*]

    WS --> S1[:9001]
    WS --> S2[:9002]

    LK --> L1[:7880]
    LK --> L2[:7890]

    FE --> WEB[:3000]
```

Details:
- `/ws*` load balances to `9001`/`9002` (`least_conn`).
- If `Authorization` header missing, Caddy injects it from `ws_auth` cookie.
- `/livekit*` routes using `X-Room-ID` derived from `room_id` query param.

## Server Process Startup

`server/main.cpp`:
1. Load `.env`
2. Fill `ServerConfig` from env
3. Construct `server::Server`
4. `server.start()`

`server::Server::start()` (`server/Server.cpp`):
1. init event logger
2. `init_stacks()`
3. `app_stack_.bootstrap()`
4. `app_stack_.start()`
5. start metrics timeseries
6. `network_router_.start_all()`
7. `control_http_.start()`

## Stack Initialization Rules

From `Server::init_stacks()` and `ServerConfigFiller`:
- If `SOCKET_PORT` is a list (`[9001,9002,...]`): one `NetworkStack` per listed port.
- Otherwise: one `NetworkStack` per `socket_threads` value (default `2`) using single `network.port`.

Each stack:
- attaches app event sink
- is registered in `NetworkRouter`

## Default Ports in Current Dev Setup

- `443`: Caddy TLS entry
- `9001`, `9002`: backend websocket stacks (when port list configured)
- `8081`: control HTTP (`/health`, `/metrics/*`)
- `3000`: web client
- `3001`: admin client
- `7880`, `7890`: LiveKit signaling nodes
- `7881`, `7891`: LiveKit TCP fallback
- `50000-50100`, `50101-50200`: LiveKit RTC UDP ranges
- `6379`: Redis

## Shutdown Order

`server::Server::stop()`:
1. Stop control HTTP
2. Stop app stack (workers + async writer)
3. Stop all network stacks
4. Stop metrics timeseries
5. Shutdown event logger

`docker/stop-server.sh` additionally:
- stops both Nuxt client containers
- `docker compose down`
- force-removes known stray containers
