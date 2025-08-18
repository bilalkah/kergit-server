# Serverless Communication Platform - Communication Flow Diagram

## High-Level Architecture Overview

```mermaid
graph TB
    subgraph ClientLayer["Client Layer"]
        CLI[CLI Client<br/>websocketpp]
        WEB[Web Client<br/>JavaScript]
        DESKTOP[Desktop Client<br/>Qt/C++]
        MOBILE[Mobile Client<br/>iOS/Android]
    end
    
    subgraph NetworkLayer["Network Layer"]
        WS[WebSocket Connection<br/>ws://localhost:9001]
        WSS[Secure WebSocket<br/>wss:// planned]
    end
    
    subgraph ServerLayer["Server Layer"]
        subgraph ChatServerApp["ChatServerApp"]
            WS_SERVER[uWebSockets Server]
            COMMAND_HANDLER[Command Handler<br/>Pattern]
            AUTH[Authentication<br/>JWT + HMAC]
            RATE_LIMITER[Rate Limiter]
            SESSION_MGR[Session Manager]
        end
        
        subgraph CommandSystem["Command System"]
            LOGIN[LoginCommand]
            REGISTER[RegisterCommand]
            AUTH_CMD[AuthenticateCommand]
            CHAT[ChatCommand]
            JOIN[JoinCommand]
            USERS[UsersCommand]
            PING[PingCommand]
        end
    end
    
    subgraph DataLayer["Data Layer"]
        subgraph ChatDB["ChatDB PostgreSQL"]
            USERS_TABLE[Users Table]
            MESSAGES_TABLE[Messages Table]
            CHANNELS_TABLE[Channels Table]
            HUBS_TABLE[Hubs Table]
        end
        
        subgraph InMemoryState["In-Memory State"]
            CHAT_STATE[ChatServerState<br/>Active Users & Channels]
            WS_CONNECTIONS[WebSocket Connections<br/>ws_to_user Map]
        end
    end
    
    subgraph SecurityLayer["Security Layer"]
        JWT_MGR[JWT Manager]
        HMAC_VAL[HMAC Validator]
        MSG_ENCRYPT[Message Encryption]
        SECURITY_VAL[Security Validator]
    end
    
    %% Client connections
    CLI --> WS
    WEB --> WS
    DESKTOP --> WS
    MOBILE --> WS
    
    %% Network to server
    WS --> WS_SERVER
    WSS --> WS_SERVER
    
    %% Server internal flow
    WS_SERVER --> COMMAND_HANDLER
    COMMAND_HANDLER --> AUTH
    AUTH --> RATE_LIMITER
    RATE_LIMITER --> SESSION_MGR
    
    %% Command routing
    COMMAND_HANDLER --> LOGIN
    COMMAND_HANDLER --> REGISTER
    COMMAND_HANDLER --> AUTH_CMD
    COMMAND_HANDLER --> CHAT
    COMMAND_HANDLER --> JOIN
    COMMAND_HANDLER --> USERS
    COMMAND_HANDLER --> PING
    
    %% Database connections
    LOGIN --> USERS_TABLE
    REGISTER --> USERS_TABLE
    AUTH_CMD --> USERS_TABLE
    CHAT --> MESSAGES_TABLE
    JOIN --> CHANNELS_TABLE
    USERS --> USERS_TABLE
    
    %% In-memory state
    CHAT --> CHAT_STATE
    JOIN --> CHAT_STATE
    USERS --> CHAT_STATE
    WS_SERVER --> WS_CONNECTIONS
    
    %% Security integration
    AUTH --> JWT_MGR
    AUTH --> HMAC_VAL
    CHAT --> MSG_ENCRYPT
    COMMAND_HANDLER --> SECURITY_VAL
```

## Detailed Communication Flow

### 1. Connection Establishment

```mermaid
sequenceDiagram
    participant Client as Client (CLI/Web/Desktop)
    participant WS as WebSocket Server
    participant Auth as Authentication
    participant DB as PostgreSQL Database
    participant State as In-Memory State
    
    Client->>WS: WebSocket Connect (ws://localhost:9001)
    WS->>WS: Create PerSocketData
    WS->>State: Add to ws_to_user map
    WS->>Client: Connection Established
    
    Note over Client,State: Client is connected but not authenticated
```

### 2. Authentication Flow

```mermaid
sequenceDiagram
    participant Client as Client
    participant WS as WebSocket Server
    participant Auth as Authentication
    participant JWT as JWT Manager
    participant DB as PostgreSQL
    participant State as ChatServerState
    
    Client->>WS: {"type": "login", "username": "alice", "password": "***"}
    WS->>Auth: Validate credentials
    Auth->>DB: findUserIdByUsername("alice")
    Auth->>DB: findPasswordHashByUsername("alice")
    Auth->>Auth: Verify password hash
    Auth->>JWT: Generate JWT token
    Auth->>State: Add user to active users
    Auth->>WS: Update PerSocketData.user_id
    WS->>Client: {"type": "login_success", "token": "jwt_token", "user_id": "123"}
    
    Note over Client,State: Client is now authenticated and can send messages
```

### 3. Chat Message Flow

```mermaid
sequenceDiagram
    participant ClientA as Client A (Alice)
    participant WS as WebSocket Server
    participant Chat as ChatCommand
    participant DB as PostgreSQL
    participant State as ChatServerState
    participant ClientB as Client B (Bob)
    
    ClientA->>WS: {"type": "chat", "text": "Hello Bob!", "channel": "general"}
    WS->>Chat: Execute chat command
    Chat->>Chat: Validate message & user permissions
    Chat->>DB: sendMessage(channel_id, sender_id, "Hello Bob!")
    Chat->>State: Get users in channel "general"
    Chat->>WS: Broadcast to channel users
    WS->>ClientA: {"type": "chat", "sender": "alice", "text": "Hello Bob!", "status": "sent"}
    WS->>ClientB: {"type": "chat", "sender": "alice", "text": "Hello Bob!", "channel": "general"}
    
    Note over ClientA,ClientB: Message is delivered to all users in the channel
```

### 4. Channel Management Flow

```mermaid
sequenceDiagram
    participant Client as Client
    participant WS as WebSocket Server
    participant Join as JoinCommand
    participant DB as PostgreSQL
    participant State as ChatServerState
    
    Client->>WS: {"type": "join", "channel": "general"}
    WS->>Join: Execute join command
    Join->>DB: getHubChannels(hub_id)
    Join->>State: Add user to channel
    Join->>WS: Update user's current channel
    WS->>Client: {"type": "join_success", "channel": "general"}
    WS->>State: Broadcast user joined to channel
    
    Note over Client,State: User is now in the "general" channel
```

### 5. Database Connection Management

```mermaid
graph LR
    subgraph DatabaseConnectionPool["Database Connection Pool"]
        CONN1[Connection 1<br/>pqxx::connection]
        CONN2[Connection 2<br/>pqxx::connection]
        CONN3[Connection 3<br/>pqxx::connection]
    end
    
    subgraph ChatDBOperations["ChatDB Operations"]
        CREATE_HUB[createHub]
        CREATE_CHANNEL[createChannel]
        SEND_MESSAGE[sendMessage]
        FETCH_MESSAGES[fetchMessages]
        CREATE_USER[createUser]
        FIND_USER[findUserIdByUsername]
    end
    
    subgraph PostgreSQLDatabase["PostgreSQL Database"]
        USERS[(Users Table)]
        MESSAGES[(Messages Table)]
        CHANNELS[(Channels Table)]
        HUBS[(Hubs Table)]
    end
    
    CREATE_HUB --> CONN1
    CREATE_CHANNEL --> CONN2
    SEND_MESSAGE --> CONN3
    FETCH_MESSAGES --> CONN1
    CREATE_USER --> CONN2
    FIND_USER --> CONN3
    
    CONN1 --> USERS
    CONN2 --> MESSAGES
    CONN3 --> CHANNELS
    CONN1 --> HUBS
```

## Key Communication Patterns

### 1. **Command Pattern Implementation**
- Each client request is routed to a specific `ICommand` implementation
- Commands handle authentication, validation, and business logic
- Commands interact with both database and in-memory state

### 2. **WebSocket Connection Management**
- Server maintains `ws_to_user` map for connection tracking
- `PerSocketData` stores user context per connection
- Connection events trigger state updates

### 3. **Message Routing**
- Messages are routed based on `type` field in JSON
- Channel-based broadcasting for chat messages
- Direct messaging for system notifications

### 4. **Security Flow**
- JWT tokens for session management
- HMAC signatures for message integrity
- Rate limiting per connection
- Input validation and sanitization

### 5. **Database Operations**
- PostgreSQL for persistent storage
- Connection pooling via pqxx
- Transaction management for data consistency
- Prepared statements for security

## Current Implementation Status

✅ **Implemented:**
- Basic WebSocket communication
- Command pattern architecture
- PostgreSQL database integration
- User authentication system
- Chat message routing
- Channel management

🔄 **In Progress:**
- Security enhancements (JWT, HMAC, WSS)
- Rate limiting implementation
- Message encryption
- Session management

📋 **Planned:**
- WebRTC signaling for voice/video calls
- TURN server integration
- SFU for multi-user calls
- Mobile client support

This architecture provides a solid foundation for real-time communication with clear separation of concerns and extensible design patterns. 