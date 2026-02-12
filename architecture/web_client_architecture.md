# Web Client Architecture

## Module Overview

```mermaid
flowchart TB
    subgraph Pages["Pages (routes)"]
        INDEX["/index.vue<br/>Landing"]
        LOGIN["/login.vue<br/>Login form"]
        SIGNUP["/signup.vue<br/>Registration"]
        APP["/app/index.vue<br/>Main app"]
        SETTINGS["/app/settings.vue<br/>User settings"]
    end

    subgraph Layouts["Layouts"]
        DEFAULT["default.vue<br/>Landing layout"]
        APP_LAYOUT["app.vue<br/>Main app shell"]
    end

    subgraph AppComponents["App Layout Components"]
        TOPBAR[AppTopBar.vue]
        HUBLIST[HubList.vue]
        CHANLIST[ChannelList.vue]
        MEMBERLIST[MemberList.vue]
        MSGLIST[MessageList.vue]
        MSGINPUT[MessageInput.vue]
        VOICEDOCK[VoiceDock.vue]
    end

    subgraph Composables["Composables (Hooks)"]
        USE_WS["useWebSocket()<br/>Socket connection + commands"]
        USE_SUPA["useSupabase()<br/>Supabase client"]
    end

    subgraph Stores["Pinia Stores"]
        AUTH_STORE["auth.ts<br/>User auth state"]
        APP_STORE["app.ts<br/>App state + events"]
    end

    subgraph Services["Services"]
        PROTO["proto.ts<br/>Protobuf encode/decode"]
        LIVEKIT["livekit.ts<br/>Voice connection"]
        SUPA_ADAPTER["supabaseAdapter.ts<br/>Session mapping"]
    end

    subgraph Utils["Utils"]
        AVATAR["avatar.ts<br/>Avatar generation"]
    end

    %% Page → Layout
    INDEX --> DEFAULT
    LOGIN --> DEFAULT
    SIGNUP --> DEFAULT
    APP --> APP_LAYOUT
    SETTINGS --> APP_LAYOUT

    %% Layout → Components
    APP_LAYOUT --> TOPBAR
    APP_LAYOUT --> HUBLIST
    APP_LAYOUT --> CHANLIST
    APP_LAYOUT --> MEMBERLIST
    APP_LAYOUT --> MSGINPUT
    APP_LAYOUT --> VOICEDOCK
    APP --> MSGLIST

    %% Components → Composables
    APP_LAYOUT --> USE_WS
    TOPBAR --> USE_WS
    MSGINPUT --> USE_WS
    CHANLIST --> USE_WS
    VOICEDOCK --> USE_WS

    %% Composables → Stores
    USE_WS --> AUTH_STORE
    USE_WS --> APP_STORE

    %% Composables → Services
    USE_WS --> PROTO
    USE_WS --> LIVEKIT
    USE_WS --> SUPA_ADAPTER
    USE_WS --> USE_SUPA

    %% Services deps
    SUPA_ADAPTER --> AUTH_STORE
    LIVEKIT --> APP_STORE
```

## WebSocket State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE: Initial

    IDLE --> CONNECTING: connect()
    
    CONNECTING --> LOADING: onopen
    CONNECTING --> ERROR: onclose (auth error)
    CONNECTING --> IDLE: onclose (normal)
    
    LOADING --> READY: SESSION_BOOTSTRAP received
    LOADING --> ERROR: timeout / auth fail
    
    READY --> IDLE: onclose (normal)
    READY --> ERROR: onclose (auth error 4400+)
    
    ERROR --> IDLE: scheduleReconnect()
    IDLE --> CONNECTING: reconnect timer fires
    
    ERROR --> [*]: forceLogout()

    note right of CONNECTING
        Resolving token
        Opening WebSocket
    end note

    note right of LOADING
        Waiting for bootstrap
        Auth timeout = 8s
    end note

    note right of READY
        Ping loop active
        Commands available
    end note
```

## useWebSocket Composable - Function Map

```mermaid
flowchart TB
    subgraph Connection["Connection Management"]
        CONNECT["connect(options)"]
        DISCONNECT["disconnect()"]
        RESOLVE_TOKEN["resolveToken()"]
        WAIT_AUTH["waitForAuth(timeout)"]
        schedule_reconnect["scheduleReconnect()"]
        FORCE_LOGOUT["forceLogout(reason)"]
    end

    subgraph Auth["Authentication"]
        SEND_REAUTH["sendReauth(token)"]
        REFRESH_REAUTH["refreshAndReauth()"]
        SCHEDULE_REAUTH["scheduleReauth(session)"]
        ENSURE_LISTENER["ensureAuthRefreshListener()"]
    end

    subgraph Heartbeat["Heartbeat"]
        SEND_PING["sendPing()"]
        START_PING["startPingLoop()"]
        STOP_PING["stopPingLoop()"]
    end

    subgraph Messages["Message Commands"]
        SEND_MSG["sendChatMessage(hub, channel, content)"]
        FETCH_LATEST["fetchLatestMessages(hub, channel)"]
        FETCH_BEFORE["fetchMessagesBefore(hub, channel, beforeId)"]
    end

    subgraph Hubs["Hub Commands"]
        CREATE_HUB["createHub(name)"]
        JOIN_HUB["joinHub(code)"]
        LEAVE_HUB["leaveHub(hubId)"]
        REMOVE_HUB["removeHub(hubId)"]
        UPDATE_HUB["updateHubSettings(hubId, changes)"]
        HUB_CODE["createHubJoinCode(hubId)"]
    end

    subgraph Channels["Channel Commands"]
        CREATE_CHAN["createChannel(hub, name, type)"]
        UPDATE_CHAN["updateChannelSettings(hub, channel, changes)"]
        REMOVE_CHAN["removeChannel(hub, channel)"]
        ACTIVE_CHAN["sendActiveChannel(hub, channel)"]
    end

    subgraph Activity["Activity Commands"]
        TYPING_START["sendTypingStarted(hub, channel)"]
        TYPING_STOP["sendTypingStopped(hub, channel)"]
    end

    subgraph Voice["Voice Commands"]
        JOIN_VOICE["joinVoiceChannel(hub, channel)"]
        LEAVE_VOICE["leaveVoiceChannel(hub, channel)"]
        VOICE_ACTIVITY["sendVoiceActivity(hub, channel, activity)"]
        CONNECT_VOICE["connectVoice(token, sessionId, hub, channel)"]
        DISCONNECT_VOICE["disconnectVoice()"]
        SEND_MEMBERSHIP["sendVoiceMembership(hub, channel, state)"]
        FINALIZE_LEAVE["finalizeVoiceLeave(...)"]
    end

    subgraph User["User Commands"]
        UPDATE_USER["updateUserSettings(changes)"]
    end

    subgraph Handlers["Message Handlers"]
        HANDLE_MSG["handleMessage(event)"]
        HANDLE_BIN["handleBinaryMessage(data)"]
        HANDLE_TXT["handleTextMessage(data)"]
    end

    CONNECT --> RESOLVE_TOKEN
    CONNECT --> WAIT_AUTH
    RESOLVE_TOKEN --> SEND_REAUTH
    WAIT_AUTH --> START_PING

    HANDLE_MSG --> HANDLE_BIN
    HANDLE_MSG --> HANDLE_TXT
```

## Protobuf Message Flow

```mermaid
sequenceDiagram
    participant UI as UI Component
    participant WS as useWebSocket
    participant Proto as protoService
    participant Net as WebSocket

    rect rgb(230, 245, 255)
        Note over UI,Net: Outbound (Client → Server)
        UI->>WS: sendChatMessage(hub, channel, content)
        WS->>Proto: load() → ensure proto types loaded
        WS->>Proto: encodeSendMessage(hub, channel, content)
        Proto-->>WS: payload bytes
        WS->>Proto: encodeEnvelope(MESSAGE_SEND, payload)
        Proto-->>WS: envelope bytes
        WS->>Net: socket.send(envelopeBytes)
    end

    rect rgb(255, 245, 230)
        Note over UI,Net: Inbound (Server → Client)
        Net->>WS: onmessage(ArrayBuffer)
        WS->>Proto: decodeEnvelope(bytes)
        Proto-->>WS: {type, payload}
        WS->>WS: switch(type)
        alt MESSAGE_CREATED
            WS->>Proto: decodeMessageCreated(payload)
            Proto-->>WS: {hub_id, channel_id, message}
            WS->>UI: appStore.applyMessageCreated(data)
        else SESSION_BOOTSTRAP
            WS->>Proto: decodeSessionBootstrap(payload)
            WS->>UI: appStore.hydrateFromSessionBootstrap(data)
        else HUB_CREATED
            WS->>Proto: decodeHubCreated(payload)
            WS->>UI: appStore.applyHubCreated(data)
        end
    end
```

## App Store - State & Event Handlers

```mermaid
flowchart TB
    subgraph State["Reactive State"]
        direction LR
        HUBS["hubs[]"]
        CHANNELS["channelsByHub{}"]
        MEMBERS["membersByHub{}"]
        MESSAGES["messagesByChannel{}"]
        PRESENCE["presenceByUser{}"]
        TYPING["typingByChannel{}"]
        VOICE_PART["voiceParticipantsByChannel{}"]
        VOICE_STATE["voiceStatesByUser{}"]
    end

    subgraph Identity["User Identity"]
        USER_ID["userId"]
        USERNAME["username"]
        DISPLAY["displayName"]
        AVATAR["avatarSeed"]
    end

    subgraph Selection["UI Selection"]
        ACTIVE_HUB["activeHubId"]
        ACTIVE_CHAN["activeChannelId"]
        ACTIVE_VOICE_HUB["activeVoiceHubId"]
        ACTIVE_VOICE_CHAN["activeVoiceChannelId"]
        VIEWED_HUB["viewedHubId"]
    end

    subgraph Handlers["Event Handlers (apply*)"]
        H_BOOTSTRAP["hydrateFromSessionBootstrap()"]
        H_PRESENCE["applyPresenceEvent()"]
        H_MSG_CREATE["applyMessageCreated()"]
        H_MSG_BATCH["applyMessageBatch()"]
        H_HUB_CREATE["applyHubCreated()"]
        H_HUB_REMOVE["applyHubRemoved()"]
        H_HUB_RENAME["applyHubRenamed()"]
        H_MEMBER_JOIN["applyHubMemberJoined()"]
        H_MEMBER_LEFT["applyHubMemberLeft()"]
        H_CHAN_CREATE["applyChannelCreated()"]
        H_CHAN_REMOVE["applyChannelRemoved()"]
        H_CHAN_RENAME["applyChannelRenamed()"]
        H_VOICE_PART["applyVoiceChannelParticipants()"]
        H_VOICE_PRES["applyVoiceChannelPresence()"]
        H_USER_UPDATE["applyUserProfileUpdated()"]
    end

    H_BOOTSTRAP --> HUBS
    H_BOOTSTRAP --> CHANNELS
    H_BOOTSTRAP --> MEMBERS
    H_BOOTSTRAP --> PRESENCE

    H_PRESENCE --> PRESENCE
    H_PRESENCE --> TYPING

    H_MSG_CREATE --> MESSAGES
    H_MSG_BATCH --> MESSAGES

    H_HUB_CREATE --> HUBS
    H_HUB_CREATE --> CHANNELS
    H_HUB_REMOVE --> HUBS
    H_HUB_REMOVE --> ACTIVE_HUB
    H_HUB_RENAME --> HUBS

    H_MEMBER_JOIN --> MEMBERS
    H_MEMBER_JOIN --> PRESENCE
    H_MEMBER_LEFT --> MEMBERS

    H_CHAN_CREATE --> CHANNELS
    H_CHAN_REMOVE --> CHANNELS
    H_CHAN_REMOVE --> ACTIVE_CHAN
    H_CHAN_RENAME --> CHANNELS

    H_VOICE_PART --> VOICE_PART
    H_VOICE_PRES --> VOICE_PART
    H_VOICE_PRES --> VOICE_STATE
```

## Connection & Auth Flow

```mermaid
sequenceDiagram
    participant Page as app.vue layout
    participant WS as useWebSocket
    participant Auth as authStore
    participant Supa as Supabase
    participant Server

    Page->>Auth: await initPromise
    Auth-->>Page: auth ready
    
    Page->>Page: isAuthenticated?
    
    alt Not authenticated
        Page->>Page: middleware redirects to /login
    else Authenticated
        Page->>WS: connect()
        
        WS->>WS: Check localStorage auth_expires_at
        alt Expired
            WS->>WS: forceLogout('deadline_exceeded')
        else Valid
            WS->>Supa: getSession()
            Supa-->>WS: session
            
            alt Token expiring soon (< 10s)
                WS->>Supa: refreshSession()
                Supa-->>WS: new session
            end
            
            WS->>Auth: setAuthenticated(user)
            WS->>Auth: touchAuthExpiry()
            WS->>WS: scheduleReauth(session)
            
            WS->>Server: new WebSocket(wss://...)
            Note over WS,Server: Cookie: ws_auth={token}
            
            Server-->>WS: onopen
            WS->>WS: state = LOADING
            
            Server-->>WS: SESSION_BOOTSTRAP
            WS->>WS: hydrateFromSessionBootstrap()
            WS->>WS: startPingLoop()
            WS->>WS: state = READY
            
            WS-->>Page: connected = true
        end
    end
```

## Voice Channel Flow

```mermaid
sequenceDiagram
    participant UI as VoiceDock
    participant WS as useWebSocket
    participant Store as appStore
    participant Server
    participant LK as LiveKit Service
    participant LKServer as LiveKit Server

    UI->>WS: joinVoiceChannel(hubId, channelId)
    WS->>WS: voiceSession.id++, active=true
    WS->>Server: VOICE_JOIN (request_join)
    
    Server-->>WS: VOICE_TOKEN_ISSUED (token)
    WS->>LK: connectVoice(token, sessionId, hub, channel)
    
    LK->>LKServer: Connect with token
    LKServer-->>LK: Room joined
    LK->>LK: createLocalAudioTrack()
    LK->>LKServer: Publish audio
    
    LK->>WS: onJoined callback
    WS->>Server: VOICE_JOIN (join)
    
    Server-->>WS: VOICE_CHANNEL_PARTICIPANTS
    WS->>Store: applyVoiceChannelParticipants()
    
    Note over UI,LKServer: Voice active - audio streaming
    
    UI->>WS: leaveVoiceChannel(hubId, channelId)
    WS->>Server: VOICE_JOIN (request_leave)
    WS->>LK: leaveVoice()
    LK->>LKServer: Disconnect
    WS->>Server: VOICE_JOIN (leave)
    
    Server-->>WS: VOICE_CHANNEL_PRESENCE (left)
    WS->>Store: applyVoiceChannelPresence()
```

## Envelope Types (Commands & Events)

```mermaid
mindmap
  root((Envelope Types))
    Outbound Commands
      AUTH
      PING
      TYPING
      ACTIVE_CHANNEL
      MESSAGE_SEND
      MESSAGE_FETCH_LATEST
      MESSAGE_FETCH_BEFORE
      HUB_CREATE
      HUB_JOIN
      HUB_LEAVE
      HUB_REMOVE
      HUB_UPDATE
      HUB_CREATE_JOIN_CODE
      CHANNEL_CREATE
      CHANNEL_RENAME
      CHANNEL_REMOVE
      USER_UPDATE
      VOICE_JOIN
      VOICE_ACTIVITY
    Inbound Events
      PONG
      SESSION_BOOTSTRAP
      CommandError
      PRESENCE
      MESSAGE_CREATED
      MESSAGE_BATCH
      HUB_CREATED
      HUB_MEMBER_JOINED
      HUB_MEMBER_LEFT
      HUB_REMOVED
      HUB_RENAMED
      HUB_AVATAR_CHANGED
      HUB_ALREADY_MEMBER
      HUB_JOIN_CODE_CREATED
      CHANNEL_CREATED
      CHANNEL_RENAMED
      CHANNEL_REMOVED
      USER_PROFILE_UPDATED
      VOICE_TOKEN_ISSUED
      VOICE_CHANNEL_PARTICIPANTS
      VOICE_CHANNEL_PRESENCE
```

## Error Handling & Reconnection

```mermaid
flowchart TB
    subgraph Errors["Error Sources"]
        AUTH_ERR["Auth Error<br/>code >= 4400"]
        CLOSE_REASON["Server Close<br/>with reason"]
        TIMEOUT["Bootstrap Timeout<br/>8 seconds"]
        SOCKET_ERR["Socket Error"]
        TOKEN_FAIL["Token Refresh Failed"]
    end

    subgraph Actions["Actions Taken"]
        FORCE_LOGOUT["forceLogout()<br/>→ Clear state<br/>→ Redirect to /"]
        SCHEDULE_RECONNECT["scheduleReconnect()<br/>→ Retry in 5s<br/>→ Max 6 attempts"]
        ERROR_STATE["state = ERROR<br/>Show disconnect UI"]
    end

    AUTH_ERR --> FORCE_LOGOUT
    CLOSE_REASON --> ERROR_STATE
    TIMEOUT --> ERROR_STATE
    SOCKET_ERR --> ERROR_STATE
    TOKEN_FAIL --> FORCE_LOGOUT

    ERROR_STATE --> |No auth error| SCHEDULE_RECONNECT
    SCHEDULE_RECONNECT --> |Max attempts reached| ERROR_STATE
```

## Potential Issue Points

```mermaid
flowchart TB
    subgraph Issues["Potential Problem Areas"]
        direction TB
        
        subgraph Auth["Authentication Issues"]
            I1["Token not refreshed in time<br/>→ AUTH error from server"]
            I2["localStorage auth_expires_at<br/>not synced with JWT expiry"]
            I3["Multiple tabs: conflicting<br/>Supabase sessions"]
        end

        subgraph Voice["Voice Issues"]
            I4["voiceSession state<br/>out of sync with server"]
            I5["pendingVoiceMembership<br/>lost on reconnect"]
            I6["LiveKit token issued but<br/>connection fails"]
        end

        subgraph Connection["Connection Issues"]
            I7["connectionPromise not cleared<br/>on error path"]
            I8["authResolver timeout fires<br/>after socket closed"]
            I9["Reconnect loop never stops<br/>if server keeps closing"]
        end

        subgraph State["State Issues"]
            I10["messageIdsByChannel Map<br/>not cleared on clearAll()"]
            I11["voiceParticipantsByChannel<br/>stale after reconnect"]
            I12["activeChannelId invalid<br/>after channel deleted"]
        end
    end
```

## File Structure Summary

| Path | Purpose | Key Functions |
|------|---------|---------------|
| `composables/useWebSocket.ts` | WebSocket singleton, all commands | connect, disconnect, send*, fetch*, join*, leave* |
| `composables/useSupabase.ts` | Supabase client singleton | createClient |
| `stores/auth.ts` | Auth state | setAuthenticated, logout, touchAuthExpiry |
| `stores/app.ts` | App state + event handlers | hydrate*, apply*, activate*, view* |
| `src/services/proto.ts` | Protobuf encode/decode | load, encode*, decode* |
| `src/services/voice/livekit.ts` | LiveKit voice | connectVoice, leaveVoice |
| `src/services/auth/supabaseAdapter.ts` | Session mapping | mapSupabaseSession |
| `layouts/app.vue` | Main app shell | Boot screen, disconnect handling |
| `layouts/app/MessageList.vue` | Chat messages | Render, scroll, fetch more |
| `layouts/app/MessageInput.vue` | Message input | Typing indicators, send |
| `layouts/app/VoiceDock.vue` | Voice controls | Join, leave, mute, deafen |
| `layouts/app/HubList.vue` | Hub sidebar | Select, create, join hub |
| `layouts/app/ChannelList.vue` | Channel list | Select channel, create |
| `layouts/app/MemberList.vue` | Member list | Online status |

## Data Flow Summary

```
┌─────────────────────────────────────────────────────────────────────┐
│  UI LAYER (Vue Components)                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │
│  │  HubList    │  │ ChannelList │  │ MessageList │  ...             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                  │
│         │                │                │                          │
│         ▼                ▼                ▼                          │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ useWebSocket() + useAppStore()
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  STATE LAYER (Pinia Stores)                                         │
│  ┌───────────────────────┐  ┌───────────────────────────────────┐  │
│  │  authStore            │  │  appStore                         │  │
│  │  - authenticated      │  │  - hubs, channels, members        │  │
│  │  - user               │  │  - messages, presence, typing     │  │
│  │  - bootPhase          │  │  - voice participants/states      │  │
│  └───────────────────────┘  └───────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ Commands (send) / Events (receive)
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  COMPOSABLE LAYER                                                   │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  useWebSocket (singleton)                                      │  │
│  │  - state: IDLE → CONNECTING → LOADING → READY                 │  │
│  │  - ping loop, reauth scheduler, reconnect logic               │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ Binary (protobuf)
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  SERVICE LAYER                                                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐ │
│  │  protoService   │  │  livekit.ts     │  │  supabaseAdapter    │ │
│  │  encode/decode  │  │  voice connect  │  │  session mapping    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              │ WebSocket / WebRTC
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  NETWORK                                                            │
│  ┌─────────────────────┐  ┌─────────────────────────────────────┐  │
│  │  WebSocket → Server │  │  LiveKit Client → LiveKit Server    │  │
│  │  wss://.../ws       │  │  Voice/WebRTC                       │  │
│  └─────────────────────┘  └─────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```
