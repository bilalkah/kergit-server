```
+------------+            +------------+            +------------+
|   Client A | <--------> | Signaling  | <--------> |   Client B |
| (WebRTC +  |  WebSocket |  Server    |  WebSocket | (WebRTC +  |
| Chat/Media)|            | (Python or |            | Chat/Media)|
+------------+            | Firebase)  |            +------------+
        |                 +------------+                    |
        |                                                  |
        |             +------------------+                |
        +------------> STUN / TURN Server <----------------+
                      +------------------+
                             (coturn)
        |                                                  |
        +--------->     SFU Server     <-------------------+
                      (e.g. mediasoup)
```

# Project Iteration Table
|Iteration|	Goal|	Features|	Tools|	Notes|
|---|---|---|---|---|
|1	|Prototype 1: Chat App	|Text chat, multi-user rooms	|Python (FastAPI or Firebase), WebSocket	|Start simple — chat-only app with rooms|
|2	|Add Voice Calling (1:1)	|Peer-to-peer voice	|WebRTC native (C++), Python signaling	|Minimal WebRTC setup: STUN + WebSocket|
|3	|Add Video Calling (1:1)	|Video stream via WebRTC	|Same as above	|Add webcam support, handle codecs|
|4	|Add Multi-user Voice/Video (Mesh)	|All users send to each other	|WebRTC mesh + signaling	|Works up to 3–4 users|
|5	|Add Screen Sharing	|Share display window	|WebRTC + OS-specific API	|C++ API for screen capture|
|6	|Add TURN Server	|Relay if P2P fails	|coturn	|Needed for mobile/NAT edge cases|
|7	|Add SFU (Optional)	|Efficient multi-user calls	|mediasoup, Janus	|Avoid mesh limitations|
|8	|Make Mobile-Ready	|Push notifications, TURN priority	|Platform SDKs (iOS/Android)	|Add mobile bindings later|
|9	|Deploy UI App	|Cross-platform client	|Qt (C++), Flutter, Electron	|GUI polish + user experience|

#  Key Technologies You’ll Use
Purpose	Tools/Tech
Signaling	Python + WebSocket, Firebase RTDB
Media (Voice/Video/Screen)	WebRTC (C++ API)
NAT Traversal	STUN/TURN (coturn)
Chat	WebSocket or Firebase
Multi-user Handling	SFU (mediasoup/Janus)
UI	Qt (C++), ImGui, Electron, Flutter

# Iteration 1: Multi-User Chat App (Signaling Foundation)

We'll build:

- A signaling server (in Python using WebSocket)
- A C++ client that connects to this server
- Basic features: join room, send/receive messages

This signaling layer will be reused for WebRTC setup later.

## Components
1. Signaling Server
- Language: Python
- Library: websockets (lightweight & async)
- Functionality:
    - Handle room creation
    - Broadcast messages to room participants
    - Track connected clients

2. C++ Client
- Connects via WebSocket (use websocketpp or uWebSockets)
- Sends/receives JSON messages
- Joins room and chats


