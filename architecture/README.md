# 🏗️ Architecture Documentation

This folder contains all architecture-related documentation and design decisions for the serverless communication system.

## 📋 Contents

### **Core Architecture**
- **[ARCHITECTURE_EXTENSION.md](./ARCHITECTURE_EXTENSION.md)** - Original architecture extension for WebRTC features
- **[VOICE_CALL_IMPLEMENTATION.md](./VOICE_CALL_IMPLEMENTATION.md)** - Detailed voice call implementation guide

### **User Guides**
- **[RUNNING_VOICE_CALLS.md](./RUNNING_VOICE_CALLS.md)** - How to run voice calls with the chat app
- **[TEST_VOICE_CALLS.md](./TEST_VOICE_CALLS.md)** - Testing guide for voice call functionality

## 🌐 WebRTC Networking Concepts

### **What is NAT? (Network Address Translation)**

**NAT** is like a "mail forwarding service" for the internet:

```
Your Home Network:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Your PC   │    │   Router    │    │   Internet  │
│ 192.168.1.5 │───▶│ 192.168.1.1 │───▶│ 203.0.113.1 │
└─────────────┘    │ (NAT Box)   │    └─────────────┘
                   └─────────────┘
```

**Why NAT exists:**
- **Limited IP addresses**: There aren't enough public IP addresses for every device
- **Security**: Hides your internal network from the internet
- **Cost**: Internet providers only need to give you one public IP

**The Problem for WebRTC:**
- Your PC has private IP `192.168.1.5` (only visible inside your home)
- The internet only sees your router's public IP `203.0.113.1`
- When someone tries to connect directly to your PC, they can't reach it!

### **What is ICE? (Interactive Connectivity Establishment)**

**ICE** is like a "smart detective" that finds all possible ways to connect two devices:

```
ICE Process:
1. Find all possible addresses for both devices
2. Test each combination to see which works
3. Choose the best working connection
```

**ICE Candidates (Connection Options):**
```
Device A Candidates:          Device B Candidates:
├── 192.168.1.5:12345        ├── 10.0.0.10:54321
├── 203.0.113.1:12345        ├── 198.51.100.5:54321
└── 2001:db8::1:12345        └── 2001:db8::2:54321
```

**ICE Testing Process:**
```
Step 1: Exchange all candidates
Step 2: Test each combination
Step 3: Find the best working path
Step 4: Establish connection
```

### **What is STUN? (Session Traversal Utilities for NAT)**

**STUN** is like a "mirror" that tells you how the internet sees you:

```
STUN Process:
┌─────────────┐    "What's my public IP?"    ┌─────────────┐
│   Your PC   │─────────────────────────────▶│  STUN Server│
│ 192.168.1.5 │                              │             │
└─────────────┘                              └─────────────┘
       │                                             │
       │ "Your public IP is 203.0.113.1:12345"      │
       │◀────────────────────────────────────────────│
```

**What STUN does:**
- **Discovers your public IP**: "Hey STUN server, what IP do you see me coming from?"
- **Tests connectivity**: "Can I receive data on this IP/port?"
- **Helps with NAT traversal**: Provides the public address for direct connections

**STUN Message Example:**
```json
{
  "type": "STUN_REQUEST",
  "from": "192.168.1.5:12345",
  "to": "stun.l.google.com:19302"
}

Response:
{
  "type": "STUN_RESPONSE", 
  "your_public_ip": "203.0.113.1:12345",
  "nat_type": "SYMMETRIC"
}
```

### **What is TURN? (Traversal Using Relays around NAT)**

**TURN** is like a "post office" that forwards messages when direct delivery fails:

```
TURN Relay Process:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Device A  │───▶│ TURN Server │───▶│  Device B   │
│ (Behind NAT)│    │ (Relay)     │    │ (Behind NAT)│
└─────────────┘    └─────────────┘    └─────────────┘
```

**When TURN is needed:**
- **Symmetric NAT**: Your router changes the port for each connection
- **Firewall blocking**: Direct connections are blocked
- **Different networks**: Corporate networks, mobile networks

**TURN Message Flow:**
```
1. Device A → TURN: "Please relay my messages to Device B"
2. Device B → TURN: "Please relay my messages to Device A"  
3. TURN Server: "I'll forward messages between you two"
4. All audio/video goes through TURN server
```

### **NAT Types and Their Impact**

| NAT Type | Description | Direct Connection | STUN Works | TURN Needed |
|----------|-------------|-------------------|------------|-------------|
| **Open** | No NAT | ✅ Yes | ✅ Yes | ❌ No |
| **Full Cone** | Accepts any external connection | ✅ Yes | ✅ Yes | ❌ No |
| **Restricted Cone** | Only accepts from known IPs | ✅ Yes | ✅ Yes | ❌ No |
| **Port Restricted** | Only accepts from known IP:port | ✅ Yes | ✅ Yes | ❌ No |
| **Symmetric** | Different port for each connection | ❌ No | ❌ No | ✅ Yes |

### **Real-World Example**

**Scenario: Two friends want to video call**

```
Friend A (Home WiFi)           Friend B (Mobile Data)
┌─────────────┐                ┌─────────────┐
│ 192.168.1.5 │                │ 10.0.0.10   │
│ (Private)   │                │ (Private)   │
└─────────────┘                └─────────────┘
       │                              │
       │ Router: 203.0.113.1          │ Mobile: 198.51.100.5
       └──────────────┐               └──────────────┐
                      │                              │
                      ▼                              ▼
                 ┌─────────────┐                ┌─────────────┐
                 │   Internet  │                │   Internet  │
                 └─────────────┘                └─────────────┘
```

**ICE Process:**
1. **STUN Discovery**: Both friends ask STUN servers "What's my public IP?"
2. **Candidate Exchange**: They share all their possible addresses
3. **Connectivity Testing**: Try to connect directly using public IPs
4. **Success**: Direct P2P connection established!

**If Direct Connection Fails:**
1. **TURN Fallback**: Use TURN server as relay
2. **Relay Connection**: All data goes through TURN server
3. **Higher Latency**: But connection works!

### **Why This Matters for Our Project**

**Current Implementation (Simulated):**
```cpp
// We simulate ICE candidates
std::string candidate = "candidate:1 1 UDP 2122252543 192.168.1.5 12345 typ host";
```

**Real Implementation Would Need:**
```cpp
// Real STUN server queries
webrtc::PeerConnectionInterface::RTCConfiguration config;
config.ice_servers.push_back({
    "stun:stun.l.google.com:19302"  // Google's free STUN server
});

// Real TURN server for fallback
config.ice_servers.push_back({
    "turn:your-turn-server.com:3478",
    "username", "password"
});
```

### **Common STUN/TURN Servers**

**Free STUN Servers:**
- `stun:stun.l.google.com:19302` (Google)
- `stun:stun1.l.google.com:19302` (Google)
- `stun:stun.stunprotocol.org:3478` (STUN Protocol)

**TURN Servers (Usually Paid):**
- `turn:your-turn-provider.com:3478`
- `turn:your-turn-provider.com:5349` (TLS)

### **Security Considerations**

**STUN (Generally Safe):**
- Only reveals your public IP
- No data relay
- Can be used without authentication

**TURN (Needs Security):**
- Relays all your data
- Requires authentication
- Can be expensive (bandwidth costs)
- Should use TLS encryption

## 🎯 Architecture Overview

### **Current System**
```
┌─────────────┐    WebSocket     ┌─────────────┐
│   Client    │◄────────────────►│   Server    │
│  (C++ App)  │   (Signaling)    │ (C++ uWS)   │
└─────────────┘                  └─────────────┘
       │                                │
       │ WebRTC P2P                     │
       │ (Simulated)                    │
       │                                │
       ▼                                ▼
┌─────────────┐                  ┌─────────────┐
│   Client    │◄────────────────►│   Server    │
│  (C++ App)  │   (Signaling)    │ (C++ uWS)   │
└─────────────┘                  └─────────────┘
```

### **Key Components**
- **Signaling Server**: WebSocket-based message routing
- **Client Application**: Chat + Voice call interface
- **WebRTC Manager**: Call lifecycle and media handling
- **Common Library**: Shared data structures and utilities

### **Message Flow**
1. **Chat Messages**: Direct WebSocket routing
2. **Call Signaling**: WebSocket for SDP/ICE exchange
3. **Media Streams**: Direct P2P WebRTC connections

## 🚀 Future Extensions

### **Real WebRTC Integration**
- Replace `webrtc_simple` with Google WebRTC
- Add real audio capture and playback
- Implement STUN/TURN server infrastructure

### **Additional Features**
- Video calls
- Screen sharing
- Group calls
- Call recording

## 📚 Related Documentation

- **[../README.md](../README.md)** - Main project overview
- **[../tests/](../tests/)** - Integration tests
- **[../client/](../client/)** - Client implementation
- **[../server/](../server/)** - Server implementation 