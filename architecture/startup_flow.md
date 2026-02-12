# Server Startup Flow

## Complete Startup Trace: docker/run-server.sh → Ready State

```mermaid
flowchart TB
    subgraph Shell["run-server.sh"]
        START[./docker/run-server.sh]
    end

    subgraph DockerInfra["Docker Infrastructure Startup"]
        direction TB
        REDIS[1. Redis Container<br/>Port 6379]
        LK[2. LiveKit Server<br/>Port 7880]
        CADDY[3. Caddy Reverse Proxy<br/>Port 443]
        UBUNTU[4. Ubuntu Dev Container<br/>host network]
        ADMIN[5. Admin Client<br/>Port 3001]
        WEB[6. Web Client<br/>Port 3000]
    end

    subgraph ServerExec["Server Process (Bazel Run)"]
        BAZEL["bazel run //server:fake_discord"]
        MAIN["main.cpp"]
    end

    START --> REDIS
    REDIS --> LK
    LK --> CADDY
    CADDY --> UBUNTU
    UBUNTU --> ADMIN
    ADMIN --> WEB
    WEB --> BAZEL
    BAZEL --> MAIN
```

## Detailed Startup Sequence

```mermaid
sequenceDiagram
    participant Shell as run-server.sh
    participant Docker as Docker Compose
    participant Redis
    participant LiveKit
    participant Caddy
    participant Container as Ubuntu Container
    participant Bazel
    participant Main as main.cpp
    participant Server
    participant AppStack
    participant NetRouter as NetworkRouter
    participant NetStack as NetworkStack

    %% Phase 1: Infrastructure
    rect rgb(230, 240, 250)
        Note over Shell,Redis: Phase 1: Infrastructure Services
        Shell->>Docker: docker compose up -d redis
        Docker->>Redis: Start Redis container
        Redis-->>Docker: Listening on :6379
    end

    %% Phase 2: LiveKit
    rect rgb(240, 250, 230)
        Note over Shell,LiveKit: Phase 2: Voice Server
        Shell->>Docker: docker compose up -d livekit-node1
        Docker->>LiveKit: Start LiveKit with config
        LiveKit->>Redis: Connect for clustering
        LiveKit-->>Docker: Listening on :7880 (WS), :7881 (TCP)
    end

    %% Phase 3: Caddy
    rect rgb(250, 240, 230)
        Note over Shell,Caddy: Phase 3: Load Balancer
        Shell->>Docker: docker compose up -d caddy
        Docker->>Caddy: Start with Caddyfile
        Caddy-->>Docker: Listening on :443 (HTTPS)
    end

    %% Phase 4: Dev Container
    rect rgb(240, 230, 250)
        Note over Shell,Container: Phase 4: Dev Container
        Shell->>Docker: docker compose up -d ubuntu-dev
        Docker->>Container: Start container (host network)
        Container-->>Docker: Ready (sleep infinity)
    end

    %% Phase 5: Clients
    rect rgb(250, 230, 240)
        Note over Shell,Container: Phase 5: Web Clients
        Shell->>Shell: Start admin client (:3001)
        Shell->>Shell: Start web client (:3000)
    end

    %% Phase 6: Server Process
    rect rgb(230, 250, 240)
        Note over Shell,NetStack: Phase 6: Socket Server
        Shell->>Container: docker exec ... bazel run
        Container->>Bazel: Build & run //server:fake_discord
        Bazel->>Main: Execute main()
        
        Main->>Main: Load .env file
        Main->>Main: Fill ServerConfig from env
        Main->>Server: new Server(config)
        
        Server->>AppStack: Create AppStack(config)
        AppStack->>AppStack: Create EventQueue(30000)
        
        Server->>NetRouter: Create NetworkRouter
        
        Main->>Server: server.start()
        Server->>Server: init_stacks()
        
        loop For each socket_thread (default: 2)
            Server->>NetStack: new NetworkStack(config)
            Server->>NetStack: attach_event_sink(app.event_sink())
            Server->>NetRouter: register_stack(netstack)
        end
        
        Server->>AppStack: attach_outbound_sink(router)
        Server->>AppStack: bootstrap()
        
        AppStack->>AppStack: init_database() → PersistenceGateway
        AppStack->>AppStack: init_managers() → Session, Subscription
        AppStack->>AppStack: init_services() → Auth, Hub, Channel...
        AppStack->>AppStack: init_dispatcher() → Register commands
        AppStack->>AppStack: init_workers() → WorkerPool
        
        Server->>AppStack: start()
        AppStack->>AppStack: worker_pool.start()
        
        Server->>NetRouter: start_all()
        NetRouter->>NetStack: start()
        NetStack->>NetStack: WebSocket listen on :9001
        
        NetStack-->>Main: Server ready!
    end
```

## Network Topology After Startup

```mermaid
flowchart TB
    subgraph Internet["Internet / LAN"]
        BROWSER[Browser Client]
        MOBILE[Mobile App]
    end

    subgraph Host["Host Machine (network_mode: host)"]
        subgraph Caddy["Caddy Reverse Proxy :443"]
            TLS[TLS Termination]
            ROUTE1["/ws* → :9001"]
            ROUTE2["/livekit* → :7880"]
            ROUTE3["/* → :3000"]
        end

        subgraph SocketServer["Socket Server :9001"]
            NS1[NetworkStack 1<br/>WebSocket Transport]
            NS2[NetworkStack 2<br/>WebSocket Transport]
            ROUTER[NetworkRouter]
            APPSTACK[AppStack]
            WORKERS[Worker Pool]
        end

        subgraph LiveKit["LiveKit :7880"]
            LK_WS[WebSocket Signaling]
            LK_RTC[WebRTC :50000-50100]
        end

        subgraph WebClient["Web Client :3000"]
            NUXT[Nuxt.js App]
        end

        subgraph AdminClient["Admin Client :3001"]
            ADMIN[Admin Panel]
        end

        subgraph Redis["Redis :6379"]
            CACHE[In-Memory Store]
        end

        subgraph Database["PostgreSQL :5432"]
            DB[(Supabase DB)]
        end
    end

    %% Client connections
    BROWSER -->|HTTPS :443| TLS
    MOBILE -->|HTTPS :443| TLS

    %% Caddy routing
    TLS --> ROUTE1
    TLS --> ROUTE2
    TLS --> ROUTE3

    ROUTE1 -->|WebSocket| NS1
    ROUTE1 -->|WebSocket| NS2
    ROUTE2 -->|WebSocket| LK_WS
    ROUTE3 -->|HTTP| NUXT

    %% Internal connections
    NS1 --> ROUTER
    NS2 --> ROUTER
    ROUTER <--> APPSTACK
    APPSTACK --> WORKERS
    APPSTACK -->|SQL| DB
    
    LK_WS --> CACHE
    LK_RTC <-->|UDP/TCP| BROWSER
```

## Port Mapping Summary

```mermaid
flowchart LR
    subgraph Ports["Port Assignments"]
        direction TB
        P443["443 - Caddy HTTPS<br/>(entry point)"]
        P9001["9001 - Socket Server<br/>(WebSocket)"]
        P8081["8081 - Control HTTP<br/>(admin API)"]
        P3000["3000 - Web Client<br/>(Nuxt)"]
        P3001["3001 - Admin Client<br/>(Nuxt)"]
        P7880["7880 - LiveKit WS<br/>(signaling)"]
        P7881["7881 - LiveKit TCP<br/>(fallback)"]
        P50000["50000-50100 - LiveKit<br/>(WebRTC media)"]
        P6379["6379 - Redis<br/>(cache/pubsub)"]
        P5432["5432 - PostgreSQL<br/>(database)"]
    end
```

## Connection Flow: From Browser to Server

```mermaid
sequenceDiagram
    participant Browser
    participant Caddy as Caddy :443
    participant WS as Socket Server :9001
    participant App as AppStack
    participant DB as PostgreSQL

    Browser->>Caddy: wss://server.local/ws
    Note over Caddy: TLS termination<br/>Extract JWT from cookie/header
    
    Caddy->>WS: Upgrade: websocket<br/>Authorization: supabase {token}
    
    WS->>WS: Parse JWT, verify signature
    WS->>App: ConnectionEvent(conn_id, user_id)
    
    Browser->>Caddy: AUTHENTICATE command
    Caddy->>WS: Forward
    WS->>App: MessageEvent
    App->>App: Create session for user
    App-->>WS: AUTH_SUCCESS
    WS-->>Caddy: Forward
    Caddy-->>Browser: Authenticated!
    
    Browser->>Caddy: BOOTSTRAP command
    Caddy->>WS: Forward
    App->>DB: Fetch user hubs
    App->>DB: Build snapshots
    App-->>WS: BOOTSTRAP response
    WS-->>Browser: Hub data + channels + members
```

## Component Wiring Diagram

```mermaid
flowchart TB
    subgraph main["main.cpp"]
        ENV[Load .env]
        CFG[ServerConfig]
        SRV[Server]
    end

    subgraph server["Server"]
        direction TB
        AS[AppStack]
        NR[NetworkRouter]
        HTTP[HttpServer]
    end

    subgraph appstack["AppStack Initialization"]
        direction TB
        EQ[EventQueue<br/>capacity: 30000]
        PG[PersistenceGateway<br/>read_pool + write_pool]
        SM[SessionManager]
        SUBM[SubscriptionManager]
        AUTH[AuthService]
        IDS[PublicIdService]
        USER[UserService]
        CHAN[ChannelService]
        HUB[HubService]
        PRES[PresenceService]
        NOTIF[HubNotifier]
        SNAP[HubSnapshotBuilder]
        LK[LiveKitTokenService]
        CTX[CommandContext]
        DISP[Dispatcher]
        WP[WorkerPool<br/>threads: 3]
    end

    subgraph netstack["NetworkStack (x2)"]
        direction TB
        CR[ConnectionRegistry]
        OQ[OutgoingQueue<br/>capacity: 50000]
        WST[WebSocketTransport<br/>:9001]
        HB[HeartbeatService]
        OW[OutgoingWorker]
        FE[FlushEngine]
    end

    %% main flow
    ENV --> CFG
    CFG --> SRV
    SRV --> AS
    SRV --> NR
    SRV --> HTTP

    %% appstack init order
    AS --> EQ
    AS --> PG
    AS --> SM
    AS --> SUBM
    AS --> AUTH
    AS --> IDS
    AS --> USER
    AS --> CHAN
    AS --> HUB
    AS --> PRES
    AS --> NOTIF
    AS --> SNAP
    AS --> LK
    AS --> CTX
    AS --> DISP
    AS --> WP

    %% netstack components
    NR --> CR
    NR --> OQ
    NR --> WST
    NR --> HB
    NR --> OW
    NR --> FE

    %% wiring
    WST -.->|events| EQ
    WP -.->|responses| OQ
```

## Initialization Order

| Step | Component | Action |
|------|-----------|--------|
| 1 | `main.cpp` | Load `.env` file |
| 2 | `main.cpp` | Fill `ServerConfig` from environment |
| 3 | `main.cpp` | Create `Server(config)` |
| 4 | `Server` | Create `AppStack(config)` |
| 5 | `AppStack` | Create `EventQueue(30000)` |
| 6 | `Server` | Create `NetworkRouter` |
| 7 | `Server::init_stacks()` | Loop: Create `NetworkStack` × socket_threads |
| 8 | `Server::init_stacks()` | Attach NetworkStack → AppStack.event_sink |
| 9 | `Server::init_stacks()` | Register NetworkStack → NetworkRouter |
| 10 | `Server` | Attach AppStack → NetworkRouter (outbound) |
| 11 | `AppStack::bootstrap()` | `init_database()` → PersistenceGateway |
| 12 | `AppStack::bootstrap()` | `init_managers()` → Session, Subscription |
| 13 | `AppStack::bootstrap()` | `init_services()` → All services |
| 14 | `AppStack::bootstrap()` | `init_dispatcher()` → Register commands |
| 15 | `AppStack::bootstrap()` | `init_workers()` → Create WorkerPool |
| 16 | `Server::start()` | `AppStack.start()` → Start worker threads |
| 17 | `Server::start()` | `NetworkRouter.start_all()` → Listen on :9001 |
| 18 | `Server::start()` | `HttpServer.start()` → Listen on :8081 |

## Environment Variables

| Variable | Default | Used By |
|----------|---------|---------|
| `LISTEN_HOST` | `0.0.0.0` | NetworkStack |
| `SOCKET_PORT` | `9001` | NetworkStack |
| `SOCKET_PATTERN` | `/*` | NetworkStack |
| `OUTBOUND_QUEUE_CAPACITY` | `50000` | OutgoingQueue |
| `EVENT_QUEUE_CAPACITY` | `30000` | EventQueue |
| `DB_ENGINE` | `postgresql` | PersistenceGateway |
| `DB_HOST` | `localhost` | PersistenceGateway |
| `DB_PORT` | `5432` | PersistenceGateway |
| `DB_USER` | `postgres` | PersistenceGateway |
| `DB_PASSWORD` | `password` | PersistenceGateway |
| `DB_NAME` | `postgres` | PersistenceGateway |
| `DB_POOL_SIZE` | `3` | PersistenceGateway |
| `LIVEKIT_API_KEY` | (required) | LiveKitTokenService |
| `LIVEKIT_API_SECRET` | (required) | LiveKitTokenService |
| `CONTROL_HOST` | `127.0.0.1` | HttpServer |
| `CONTROL_PORT` | `8081` | HttpServer |
| `REDIS_HOST` | `redis` | (LiveKit uses) |
| `REDIS_PORT` | `6379` | (LiveKit uses) |

## Docker Container State After Startup

```
┌─────────────────────────────────────────────────────────────────────┐
│  RUNNING CONTAINERS                                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │   redis     │  │  livekit    │  │   caddy     │                  │
│  │   :6379     │  │   :7880     │  │   :443      │                  │
│  │   alpine    │  │   :7881     │  │   TLS       │                  │
│  └─────────────┘  │   :50000+   │  └─────────────┘                  │
│                   └─────────────┘                                    │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  sercom-dev-ubuntu (host network)                             │  │
│  │  ┌─────────────────────────────────────────────────────────┐  │  │
│  │  │  bazel run //server:fake_discord                        │  │  │
│  │  │  └── Socket Server :9001                                │  │  │
│  │  │  └── Control HTTP :8081                                 │  │  │
│  │  └─────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌─────────────┐  ┌─────────────┐                                   │
│  │ web-client  │  │admin-client │                                   │
│  │   :3000     │  │   :3001     │                                   │
│  │   Nuxt      │  │   Nuxt      │                                   │
│  └─────────────┘  └─────────────┘                                   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## Request Flow Summary

```
Browser → :443 (Caddy)
           │
           ├── /ws/*      → :9001 (Socket Server) → WebSocket connection
           │                  └── Events → AppStack → Commands → Response
           │
           ├── /livekit/* → :7880 (LiveKit) → Voice signaling
           │                  └── WebRTC → :50000-50100
           │
           └── /*         → :3000 (Web Client) → Nuxt app
```
