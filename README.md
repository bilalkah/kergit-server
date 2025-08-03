```
+------------+            +------------+            +------------+
|   Client A | <--------> | Signaling  | <--------> |   Client B |
| (WebRTC +  |  WebSocket |  Server    |  WebSocket | (WebRTC +  |
| Chat/Media)|            | (C++)      |            | Chat/Media)|
+------------+            +------------+            +------------+
        |                                              |
        |             +------------------+            |
        +------------> STUN / TURN Server <------------+
                      +------------------+
                             (coturn)
        |                                              |
        +--------->     SFU Server     <---------------+
                      (e.g. mediasoup)
```

# Serverless Communication Platform

A modular, cross-platform real-time communication system supporting chat, voice calls, video calls, and screen sharing.

## Current Status: Security Foundation Phase 🔐

**Current Priority: Establishing Secure Foundation**
- 🔄 Implementing user authentication and session management
- 🔄 Adding WebSocket Secure (WSS) transport layer
- 🔄 Creating message validation and integrity checks
- 🔄 Adding rate limiting and DoS protection
- 🔄 Implementing access control and authorization

**Previous Implemented Features:**
- ✅ Multi-user chat with channels (basic functionality)
- ✅ Real-time messaging via WebSocket
- ✅ User presence and room management  
- ✅ Extensible command system
- ✅ Clean C++ architecture with PIMPL pattern

**Security Enhancements (In Progress):**
- 🔄 User authentication and secure sessions
- 🔄 Message validation and HMAC signatures
- 🔄 Rate limiting and connection security
- 🔄 WebSocket Secure (WSS) implementation
- 🔄 Access control and authorization system

## Project Iteration Table
|Iteration|	Goal|	Features|	Tools|	Status|
|---|---|---|---|---|
|0.5	|**Security Foundation**	|Authentication, message validation, encryption, rate limiting	|JWT, HMAC, WSS, bcrypt	|🔄 **IN PROGRESS**|
|1	|Secure Messaging Platform	|Secure text chat, user authentication, message integrity	|C++ (uWebSockets), WebSocket Secure	|🔄 **REFACTORING**|
|2	|Add Voice Calling (1:1)	|Peer-to-peer voice calls with secure signaling	|WebRTC (C++), secure signaling	|� **READY**|
|3	|Add Video Calling (1:1)	|Video stream via WebRTC	|Same as above	|� **READY**|
|4	|Add Screen Sharing	|Share display during calls	|WebRTC + OS-specific API	|� **READY**|
|5	|Add TURN Server	|Relay if P2P fails	|coturn	|📋 **PLANNED**|
|6	|Add SFU (Optional)	|Efficient multi-user calls	|mediasoup, Janus	|📋 **PLANNED**|
|7	|Make Mobile-Ready	|Push notifications, TURN priority	|Platform SDKs (iOS/Android)	|📋 **PLANNED**|
|8	|Deploy UI App	|Cross-platform client	|Qt (C++), Flutter, Electron	|📋 **PLANNED**|

## Architecture Overview

### 1. **Signaling Server (C++)**
- **Technology:** uWebSockets for WebSocket server
- **Responsibilities:**
  - Chat message routing and room management
  - WebRTC signaling (call requests, SDP exchange, ICE candidates)
  - User presence and state management
  - Call session tracking
- **Extensible:** Command pattern for easy feature addition

### 2. **Client (C++)**
- **Technology:** websocketpp for WebSocket client
- **Responsibilities:**
  - Chat interface and message handling
  - WebRTC peer connection management (stubs ready)
  - Media stream handling (voice/video/screen)
  - Call state management

### 3. **Shared Components**
- **Data Models:** User, Channel, Message, Call
- **State Management:** Connection states, call states
- **Message Types:** Chat, call signaling, WebRTC data

## Connection Architecture

### Single WebSocket + WebRTC P2P
```
┌─────────────┐    WebSocket     ┌─────────────┐
│   Client A  │ <──────────────> │   Server    │
│             │                  │ (Signaling) │
│ WebRTC P2P  │                  │             │
│ <─────────> │                  └─────────────┘
│   Client B  │
└─────────────┘
```

**Benefits:**
- ✅ Unified signaling for all features
- ✅ Direct P2P media (lower latency)
- ✅ Scalable (server only handles signaling)
- ✅ Industry standard approach

## Message Flow Examples

### Chat Message
```
Client A -> Server: {"type": "chat", "text": "Hello"}
Server -> Client B: {"type": "chat", "sender": "Alice", "text": "Hello"}
```

### Voice Call Request
```
Client A -> Server: {"type": "call_request", "target_user": "Bob", "media_type": "voice"}
Server -> Client B: {"type": "call_incoming", "from_user": "Alice", "media_type": "voice"}
```

### WebRTC Signaling
```
Client A -> Server: {"type": "webrtc_signal", "signal_type": "offer", "sdp": "..."}
Server -> Client B: {"type": "webrtc_signal", "signal_type": "offer", "sdp": "..."}
Client A <--WebRTC--> Client B (Direct P2P media)
```

## 🔐 Security Foundation

### Critical Security Issues Addressed

Your request to establish a solid foundation highlighted several critical security gaps:

1. **No Authentication**: Anyone could connect without verification
2. **No Message Validation**: Messages accepted without security checks
3. **Insecure Transport**: No WSS (WebSocket Secure) implementation
4. **No Rate Limiting**: Vulnerable to DoS attacks
5. **No Session Management**: No proper user session tracking
6. **No Message Integrity**: Missing HMAC/checksums for authentication

### Security Implementation Plan

**Phase 0.5: Security Foundation (CURRENT)**
- 🔄 User authentication and registration system
- 🔄 WebSocket Secure (WSS) transport layer
- 🔄 Message validation and schema checking
- 🔄 HMAC signatures for message integrity
- 🔄 Rate limiting and connection controls
- 🔄 Session management and access control

**Security Features Being Added:**
- ✅ Password hashing with salt (bcrypt-style)
- 🔄 JWT-based session tokens
- 🔄 Origin validation for CSRF protection
- 🔄 Message sequence numbering (replay protection)
- 🔄 Content filtering and input sanitization
- 🔄 Security audit logging

For detailed security implementation, see [SECURITY_FOUNDATION.md](SECURITY_FOUNDATION.md).

## Key Technologies

- **C++17:** High performance, cross-platform
- **uWebSockets/websocketpp:** Efficient WebSocket libraries
- **JSON (nlohmann):** Data serialization
- **WebRTC (planned):** Peer-to-peer media
- **PIMPL Pattern:** Clean APIs, binary compatibility

## Building and Running

### Prerequisites
- Bazel build system
- C++17 compiler
- Linux/macOS/Windows

### Build Commands
```bash
# Build server
bazel build //server:server

# Build client
bazel build //client:client

# Run server
bazel run //server:server

# Run client
bazel run //client:client
```

## Future Implementation Roadmap

### Phase 2: Voice Calls
1. **WebRTC Integration:** Add actual WebRTC peer connections
2. **Media Streams:** Implement audio capture and playback
3. **Call UI:** Add call controls and status indicators
4. **Testing:** Voice call functionality testing

### Phase 3: Video Calls
1. **Video Streams:** Add video capture and rendering
2. **Codec Support:** Implement VP8/VP9/H.264
3. **Bandwidth Management:** Adaptive bitrate
4. **UI Enhancement:** Video display components

### Phase 4: Screen Sharing
1. **Screen Capture:** OS-specific screen capture APIs
2. **Stream Management:** Handle screen share streams
3. **Permissions:** Handle screen share permissions
4. **UI Controls:** Screen share start/stop controls

## Technical Strengths

- **Modular Design:** Easy to add new features
- **Clean Architecture:** Separation of concerns
- **Extensible:** Command pattern for server features
- **Future-Ready:** WebRTC stubs already in place
- **Cross-Platform:** C++ core works everywhere
- **Scalable:** Single signaling, P2P media

## Development Guidelines

### Adding New Features
1. **Server Side:** Add new command class in `server/commands/`
2. **Client Side:** Extend `ChatClientApp` or `WebRTCManager`
3. **Data Models:** Update shared models in `common/`
4. **Testing:** Add tests for new functionality

### Code Style
- Use PIMPL pattern for public APIs
- Follow RAII principles
- Use smart pointers for memory management
- Add comprehensive error handling
- Document public interfaces

## 📚 Documentation

### **Architecture & Implementation**
- [architecture/](architecture/) - All architecture documentation and guides
  - [ARCHITECTURE_EXTENSION.md](architecture/ARCHITECTURE_EXTENSION.md) - Architecture extension for WebRTC features
  - [VOICE_CALL_IMPLEMENTATION.md](architecture/VOICE_CALL_IMPLEMENTATION.md) - Voice call implementation details
  - [RUNNING_VOICE_CALLS.md](architecture/RUNNING_VOICE_CALLS.md) - How to run voice calls
  - [TEST_VOICE_CALLS.md](architecture/TEST_VOICE_CALLS.md) - Testing voice call functionality

## Contributing

1. Fork the repository
2. Create feature branch
3. Implement with stubs for future features
4. Add tests
5. Submit pull request

---

**Current Focus:** Complete chat functionality while maintaining WebRTC-ready architecture for future iterations.

