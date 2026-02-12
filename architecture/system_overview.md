# System Overview

## High-Level Architecture

```mermaid
flowchart TB
    subgraph Clients["Clients"]
        WEB[Web App]
        MOBILE[Mobile App]
        CLI[CLI Tool]
    end

    subgraph Server["Server"]
        direction TB
        
        subgraph Control["Control Plane"]
            HTTP[HTTP Server<br/>Admin & Health]
        end
        
        subgraph Network["Network Layer"]
            ROUTER[Network Router]
            NS1[Network Stack 1]
            NS2[Network Stack N...]
            WS[WebSocket Transport]
        end
        
        subgraph Application["Application Layer"]
            QUEUE[Event Queue]
            WORKERS[Worker Pool]
            CMDS[Command Handlers]
            SERVICES[Services]
            MANAGERS[Managers]
        end
        
        subgraph Data["Data Layer"]
            PERSIST[Persistence Gateway]
            CACHE[Caches]
        end
    end

    subgraph External["External Services"]
        DB[(PostgreSQL)]
        LIVEKIT[LiveKit<br/>Voice Server]
        SUPABASE[Supabase<br/>Auth Provider]
    end

    %% Client connections
    WEB <-->|WebSocket| WS
    MOBILE <-->|WebSocket| WS
    CLI <-->|WebSocket| WS

    %% Internal flow
    WS --> NS1
    NS1 --> ROUTER
    ROUTER --> QUEUE
    QUEUE --> WORKERS
    WORKERS --> CMDS
    CMDS --> SERVICES
    CMDS --> MANAGERS
    SERVICES --> PERSIST
    PERSIST --> CACHE
    PERSIST --> DB
    
    %% Outbound
    CMDS -->|responses| ROUTER
    ROUTER -->|broadcast| WS

    %% External
    SERVICES -->|auth verify| SUPABASE
    SERVICES -->|voice tokens| LIVEKIT
```

## Server Startup Sequence

```mermaid
sequenceDiagram
    participant Main
    participant Server
    participant AppStack
    participant NetworkRouter
    participant NetworkStack
    participant Transport

    Main->>Main: Load environment config
    Main->>Server: Create Server(config)
    
    Server->>AppStack: Create AppStack
    AppStack->>AppStack: Init database connection
    AppStack->>AppStack: Init managers (Session, Subscription)
    AppStack->>AppStack: Init services (Auth, Hub, Channel, User...)
    AppStack->>AppStack: Register command handlers
    AppStack->>AppStack: Create worker pool
    
    Server->>NetworkRouter: Create NetworkRouter
    Server->>NetworkStack: Create NetworkStack(s)
    NetworkStack->>Transport: Create WebSocket Transport
    
    Server->>Server: Wire components together
    Note over Server: AppStack ←→ NetworkRouter<br/>EventSink ↔ OutboundSink
    
    Main->>Server: start()
    Server->>AppStack: start()
    AppStack->>AppStack: Start worker threads
    Server->>NetworkRouter: start_all()
    NetworkRouter->>NetworkStack: start()
    NetworkStack->>Transport: Listen on port
    
    Note over Transport: Server ready for connections
```

## Client Lifecycle

```mermaid
sequenceDiagram
    participant Client
    participant Transport
    participant App as Application Layer
    participant DB as Database

    %% Connection
    rect rgb(200, 230, 200)
        Note over Client,DB: 1. Connect & Authenticate
        Client->>Transport: WebSocket Connect
        Transport->>App: ConnectionEvent
        Client->>Transport: Authenticate (JWT token)
        App->>App: Verify token with Supabase
        App->>App: Create user session
        App-->>Client: Auth Success
    end

    %% Bootstrap
    rect rgb(200, 220, 240)
        Note over Client,DB: 2. Bootstrap (Get Initial State)
        Client->>Transport: Bootstrap Request
        App->>DB: Fetch user's hubs
        App->>DB: Fetch hub snapshots (members, channels)
        App->>App: Subscribe to hub topics
        App-->>Client: Bootstrap Response (all hubs + snapshots)
    end

    %% Active Usage
    rect rgb(240, 230, 200)
        Note over Client,DB: 3. Active Session
        
        Client->>Transport: Select Channel
        App->>App: Subscribe to channel topic
        App-->>Client: Channel messages
        
        Client->>Transport: Send Message
        App->>DB: Store message
        App->>App: Get channel subscribers
        App-->>Client: Broadcast to all subscribers
        
        Client->>Transport: Join Voice Channel
        App->>App: Mint LiveKit token
        App-->>Client: Voice token + room info
    end

    %% Disconnection
    rect rgb(240, 200, 200)
        Note over Client,DB: 4. Disconnect & Cleanup
        Client->>Transport: Disconnect
        Transport->>App: DisconnectionEvent
        App->>App: Remove all subscriptions
        App->>App: Destroy session
        App->>App: Notify others (user offline)
    end
```

## Message Flow: Sending a Chat Message

```mermaid
flowchart LR
    subgraph Client["Sender"]
        C1[User types message]
    end

    subgraph Server["Server Processing"]
        direction TB
        WS1[WebSocket Receive]
        EQ[Event Queue]
        W[Worker]
        CMD[SendMessageCommand]
        CS[ChannelService]
        SUBS[SubscriptionManager]
        OUT[Outbound Queue]
    end

    subgraph Recipients["Recipients"]
        C2[User A]
        C3[User B]
        C4[User C]
    end

    subgraph Storage["Storage"]
        DB[(Database)]
    end

    C1 -->|1. SendMessage| WS1
    WS1 -->|2. Enqueue| EQ
    EQ -->|3. Pop| W
    W -->|4. Dispatch| CMD
    CMD -->|5. Save| CS
    CS -->|6. Store| DB
    CMD -->|7. Get subscribers| SUBS
    SUBS -->|8. Return connections| CMD
    CMD -->|9. Create broadcasts| OUT
    OUT -->|10. Send| C2
    OUT -->|10. Send| C3
    OUT -->|10. Send| C4
```

## Module Overview

```mermaid
flowchart TB
    subgraph server["server/"]
        MAIN[main.cpp<br/>Entry point]
        SRV[Server<br/>Top orchestrator]
    end

    subgraph core["core/"]
        CFG[ServerConfig<br/>Configuration]
    end

    subgraph net["net/"]
        direction TB
        NR[NetworkRouter<br/>Multi-stack routing]
        NS[NetworkStack<br/>Single transport unit]
        CR[ConnectionRegistry<br/>Track connections]
        OQ[OutgoingQueue<br/>Message buffering]
        WS[WebSocketTransport<br/>uWebSockets server]
        HB[HeartbeatService<br/>Keep-alive pings]
    end

    subgraph app["app/"]
        direction TB
        AS[AppStack<br/>App orchestrator]
        EQ[EventQueue<br/>Inbound events]
        WP[WorkerPool<br/>Parallel processing]
        DISP[Dispatcher<br/>Command routing]
        CMDS[Commands<br/>Business logic]
        MGR[Managers<br/>Session + Subscriptions]
        SVC[Services<br/>Domain operations]
    end

    subgraph infra["infra/"]
        PG[PersistenceGateway<br/>Database access]
        SEC[Security<br/>JWT verification]
    end

    subgraph control["control/"]
        HTTP[HttpServer<br/>Admin API]
    end

    %% Relationships
    MAIN --> SRV
    SRV --> AS
    SRV --> NR
    SRV --> HTTP
    SRV --> CFG

    NR --> NS
    NS --> WS
    NS --> CR
    NS --> OQ
    NS --> HB

    AS --> EQ
    AS --> WP
    AS --> DISP
    DISP --> CMDS
    AS --> MGR
    AS --> SVC
    SVC --> PG
    AS --> SEC
```

## Pub/Sub Topic System

```mermaid
flowchart TB
    subgraph Topics["Topic Types"]
        HT["hub:{hub_id}<br/>Hub-level events"]
        CT["hub:{hub_id}:channel:{channel_id}<br/>Channel messages"]
        UT["user:{user_id}<br/>Direct messages"]
    end

    subgraph Events["Event Types"]
        HUB_EVT[Hub renamed<br/>Hub deleted<br/>Member joined/left<br/>Channel created/deleted]
        CHAN_EVT[New message<br/>Typing indicator<br/>Message edited]
        USER_EVT[Direct message<br/>Friend request]
    end

    subgraph Subscribers["Who Subscribes?"]
        HUB_SUB[All hub members]
        CHAN_SUB[Users viewing channel]
        USER_SUB[The specific user]
    end

    HT --> HUB_EVT
    CT --> CHAN_EVT
    UT --> USER_EVT

    HUB_EVT --> HUB_SUB
    CHAN_EVT --> CHAN_SUB
    USER_EVT --> USER_SUB
```

## Voice Channel Flow

```mermaid
sequenceDiagram
    participant User
    participant Server
    participant LiveKit as LiveKit Server

    User->>Server: JoinVoiceChannel(channel_id)
    Server->>Server: Verify user is hub member
    Server->>Server: Update session (voice_channel = channel_id)
    Server->>Server: Mint LiveKit token (identity, room, permissions)
    Server-->>User: VoiceJoinResponse (token, server_url)
    
    User->>LiveKit: Connect with token
    LiveKit-->>User: Joined room
    
    Note over User,LiveKit: User is now in voice chat
    
    User->>LiveKit: Publish audio track
    LiveKit->>LiveKit: Forward to other participants
    
    User->>Server: LeaveVoiceChannel
    Server->>Server: Clear voice_channel from session
    Server->>Server: Notify channel participants
    User->>LiveKit: Disconnect
```

## Key Concepts Summary

| Concept | What It Does | Where It Lives |
|---------|--------------|----------------|
| **Server** | Orchestrates all components | `server/` |
| **NetworkStack** | Handles one WebSocket listener + connections | `net/` |
| **NetworkRouter** | Routes messages to correct NetworkStack | `net/` |
| **AppStack** | Business logic orchestrator | `app/` |
| **EventQueue** | Buffers inbound events with priority | `app/queue/` |
| **WorkerPool** | Processes events in parallel | `app/worker/` |
| **Dispatcher** | Routes events to command handlers | `app/dispatcher/` |
| **Commands** | Execute business logic | `app/commands/` |
| **SessionManager** | Tracks logged-in users & state | `app/managers/` |
| **SubscriptionManager** | Pub/sub for real-time broadcasts | `app/managers/` |
| **Services** | Domain operations (Hub, Channel, User) | `app/services/` |
| **PersistenceGateway** | Database access | `infra/persistence/` |

## Data Flow Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                         CLIENT                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ WebSocket
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  NETWORK LAYER                                                  │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐    │
│  │  Transport  │──│ Connections  │──│   Outbound Queue    │    │
│  └─────────────┘  └──────────────┘  └─────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Events
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  APPLICATION LAYER                                              │
│  ┌───────────┐  ┌─────────────┐  ┌────────────┐  ┌──────────┐  │
│  │  Queue    │──│   Workers   │──│ Dispatcher │──│ Commands │  │
│  └───────────┘  └─────────────┘  └────────────┘  └──────────┘  │
│                                                       │         │
│                         ┌─────────────────────────────┘         │
│                         ▼                                       │
│  ┌────────────────────────────────────────────────────────┐    │
│  │  Managers (Session, Subscriptions)                      │    │
│  │  Services (Auth, Hub, Channel, User, Presence, etc.)   │    │
│  └────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ SQL
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  DATA LAYER                                                     │
│  ┌─────────────────────┐  ┌─────────────────────────────────┐  │
│  │  Persistence        │──│  PostgreSQL                     │  │
│  │  Gateway + Caches   │  │                                 │  │
│  └─────────────────────┘  └─────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```
