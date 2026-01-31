```
┌────────────────────────────────────────────────────────────────────────────┐
│                                  INTERNET                                  │
│                                                                            │
│  Browsers / Mobile Clients (Web, iOS, Android)                             │
│                                                                            │
│  ┌────────────────────┐        ┌────────────────────────┐                │
│  │ App Socket (WSS)   │        │ LiveKit Media (WebRTC) │                │
│  │ Auth / Presence   │        │ Audio RTP / ICE        │                │
│  └─────────┬──────────┘        └──────────┬────────────┘                │
│            │                               │                               │
└────────────┼───────────────────────────────┼───────────────────────────────┘
             │                               │
             ▼                               ▼

┌────────────────────────────────────────────────────────────────────────────┐
│                        RENTED PUBLIC SERVER (EDGE)                          │
│                     (Static IP, 24/7, Trusted TLS)                          │
│                                                                            │
│  ┌──────────────────────────────────────────────────────────────────────┐ │
│  │                  REVERSE PROXY / LOAD BALANCER                        │ │
│  │                    (Nginx / Caddy / HAProxy)                          │ │
│  │                                                                      │ │
│  │  HTTPS / WSS :443                                                    │ │
│  │   - Terminates TLS                                                   │ │
│  │   - Routes signaling traffic                                         │ │
│  │                                                                      │ │
│  │  UDP/TCP Forwarding                                                  │ │
│  │   - 7881 TCP (WebRTC fallback)                                       │ │
│  │   - 7882 UDP (WebRTC audio RTP)                                      │ │
│  └───────────────┬───────────────────────────────┬──────────────────────┘ │
│                  │                               │                          │
└──────────────────┼───────────────────────────────┼──────────────────────────┘
                   │                               │
                   ▼                               ▼

┌────────────────────────────────────────────────────────────────────────────┐
│                YOUR PHYSICAL MACHINE (GIGABIT INTERNET)                    │
│             (Stable LAN, High CPU, 32GB RAM, NVMe SSD)                     │
│                                                                            │
│  ┌──────────────────────────────────────────────────────────────────────┐ │
│  │                      APP SERVER (C++)                                 │ │
│  │                  (Single Process, Multithreaded)                      │ │
│  │                                                                      │ │
│  │  Network Layer (2 threads)                                            │ │
│  │   - Accept WSS connections                                           │ │
│  │   - TLS already terminated at LB                                     │ │
│  │   - Frame IO, backpressure                                           │ │
│  │                                                                      │ │
│  │  App Layer (3–5 threads)                                             │ │
│  │   - Authentication                                                   │ │
│  │   - Presence                                                         │ │
│  │   - Hub / Channel routing                                            │ │
│  │   - Voice intent (join / leave / mute / deafen)                      │ │
│  │   - LiveKit token minting                                            │ │
│  │                                                                      │ │
│  │  Shared State                                                        │ │
│  │   - Connection registry                                              │ │
│  │   - Online users                                                     │ │
│  │   - Voice presence tables                                            │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌──────────────────────────────────────────────────────────────────────┐ │
│  │                 LIVEKIT SERVER #1 (PRIMARY SFU)                       │ │
│  │                                                                      │ │
│  │   - WebRTC signaling (via LB)                                        │ │
│  │   - ICE negotiation                                                  │ │
│  │   - Audio SFU routing (Opus)                                         │ │
│  │                                                                      │ │
│  │   UDP 7882 ← RTP audio                                               │ │
│  │   TCP 7881 ← fallback                                                │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌──────────────────────────────────────────────────────────────────────┐ │
│  │                 LIVEKIT SERVER #2 (OPTIONAL / SPLIT)                 │ │
│  │                                                                      │ │
│  │   - Second SFU instance                                              │ │
│  │   - Different port range or rooms                                    │ │
│  │   - Used for load isolation                                          │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│  ┌──────────────────────────────────────────────────────────────────────┐ │
│  │                       PostgreSQL                                     │ │
│  │   - Users                                                            │ │
│  │   - Hubs / Channels                                                  │ │
│  │   - Permissions                                                      │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────┘
```