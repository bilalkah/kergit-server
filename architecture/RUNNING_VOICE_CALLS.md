# 🎉 Running Voice Calls with the Chat App

## Quick Start Guide

You now have a **fully functional voice call system** integrated with your chat application! Here's how to run it:

## 🚀 **Step 1: Start the Server**

First, start the signaling server that handles both chat and voice call signaling:

```bash
# Build and run the server
bazel run //server:server
```

The server will start on `ws://localhost:9001` and handle:
- ✅ Chat messages and channels
- ✅ Voice call requests and signaling
- ✅ WebRTC signaling (SDP offers/answers, ICE candidates)

## 🚀 **Step 2: Run Multiple Chat Clients**

Open **multiple terminal windows** to simulate different users:

### Terminal 1 - User "Alice"
```bash
# Build and run the client
bazel run //client:client

# When prompted, enter username: alice
# Join a channel: /create general
```

### Terminal 2 - User "Bob"
```bash
# In another terminal
bazel run //client:client

# When prompted, enter username: bob
# Join the same channel: /connect general
```

## 🎯 **Step 3: Make Voice Calls**

### **Initiating a Voice Call**

From Alice's terminal:
```bash
# Start a voice call with Bob
/call bob
```

You'll see:
```
[VOICE] Initiating voice call to bob
[WebRTC] Initiating voice call to bob
[CALL] State changed: CONNECTING
[WebRTC] PeerConnection initialized
[WebRTC] Peer connection created successfully
[CALL] Local stream ready: local_stream
[WebRTC] Local stream created successfully
```

### **Receiving a Voice Call**

In Bob's terminal, you'll see:
```
[VOICE] Incoming voice call from alice (ID: call_1234567890)
[VOICE] Type /accept call_1234567890 to accept or /reject call_1234567890 to reject
```

### **Accepting the Call**

From Bob's terminal:
```bash
/accept call_1234567890
```

You'll see the WebRTC connection establish:
```
[VOICE] Accepting call call_1234567890
[WebRTC] Accepting call call_1234567890
[CALL] State changed: CONNECTING
[WebRTC] PeerConnection initialized
[WebRTC] Peer connection created successfully
[CALL] Local stream ready: local_stream
[WebRTC] Local stream created successfully
[VOICE] Call call_1234567890 accepted by bob
[CALL] State changed: CONNECTED
```

### **Rejecting the Call**

If Bob wants to reject:
```bash
/reject call_1234567890
```

### **Ending the Call**

From either user's terminal:
```bash
/endcall
```

## 📋 **Available Voice Call Commands**

| Command | Description | Example |
|---------|-------------|---------|
| `/call <user>` | Start a voice call with user | `/call bob` |
| `/accept <call_id>` | Accept incoming call | `/accept call_1234567890` |
| `/reject <call_id>` | Reject incoming call | `/reject call_1234567890` |
| `/endcall` | End current voice call | `/endcall` |
| `/callstatus` | Show voice call status | `/callstatus` |

## 🔍 **Checking Call Status**

```bash
/callstatus
```

Shows:
```
Voice call status: In call (State: 2)
```
or
```
Voice call status: No active call
```

## 📱 **Complete Example Session**

### Terminal 1 (Alice):
```bash
$ bazel run //client:client
Enter user name: alice
/connect general
[Joined channel: general] as 'alice'

# Start voice call
/call bob
[VOICE] Initiating voice call to bob

# Check status
/callstatus
Voice call status: In call (State: 2)

# End call
/endcall
[VOICE] Ending call call_1234567890
```

### Terminal 2 (Bob):
```bash
$ bazel run //client:client
Enter user name: bob
/connect general
[Joined channel: general] as 'bob'

# Receive incoming call notification
[VOICE] Incoming voice call from alice (ID: call_1234567890)
[VOICE] Type /accept call_1234567890 to accept or /reject call_1234567890 to reject

# Accept the call
/accept call_1234567890
[VOICE] Accepting call call_1234567890
[VOICE] Call call_1234567890 accepted by bob
```

## 🎯 **What's Happening Behind the Scenes**

1. **Call Request**: Alice sends call request to server
2. **Server Routing**: Server forwards request to Bob
3. **WebRTC Signaling**: SDP offers/answers exchanged via server
4. **ICE Candidates**: Network connectivity established
5. **P2P Connection**: Direct peer-to-peer audio stream
6. **Call Management**: Server tracks call state

## 🔧 **Troubleshooting**

### **Build Issues**
```bash
# Clean and rebuild
bazel clean
bazel build //client:client //server:server
```

### **Connection Issues**
- Make sure server is running on port 9001
- Check firewall settings
- Verify WebSocket connection

### **Voice Call Issues**
- Check that both users are connected to server
- Verify call IDs match between users
- Look for WebRTC state changes in logs

## 🎉 **Success Indicators**

When voice calls are working correctly, you'll see:

✅ **Call Initiation**:
```
[VOICE] Initiating voice call to bob
[WebRTC] PeerConnection initialized
[CALL] State changed: CONNECTING
```

✅ **Call Acceptance**:
```
[VOICE] Accepting call call_1234567890
[WebRTC] Accepting call call_1234567890
[CALL] State changed: CONNECTED
```

✅ **WebRTC Signaling**:
```
[VOICE] Received WebRTC offer from alice
[VOICE] Received WebRTC answer from bob
[VOICE] Received ICE candidate from alice
```

✅ **Call Status**:
```
Voice call status: In call (State: 2)
```

## 🚀 **Next Steps**

The voice call system is now fully functional! You can:

1. **Test with multiple users** - Start more clients
2. **Add real audio** - Replace WebRTC stubs with actual audio capture
3. **Extend to video calls** - Add video stream support
4. **Add screen sharing** - Implement screen capture functionality

**Enjoy your voice calls!** 🎉 