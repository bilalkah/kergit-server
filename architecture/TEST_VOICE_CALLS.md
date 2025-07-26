# 🧪 Testing Voice Call Notifications

## **Issue Fixed: Missing Call Notifications**

The problem was that the server was looking for users by **username** but storing them by **user ID**. This has been fixed!

## **How to Test the Fix**

### **Step 1: Start the Server**
```bash
bazel run //server:server
```

You should see server startup messages.

### **Step 2: Start Two Clients**

**Terminal 1 - Alice:**
```bash
bazel run //client:client
Enter user name: alice
/connect general
```

**Terminal 2 - Bob:**
```bash
bazel run //client:client
Enter user name: bob
/connect general
```

### **Step 3: Test Voice Call**

**From Alice's terminal:**
```bash
/call bob
```

**Expected Output:**

**Server logs should show:**
```
[CALL] User 'alice' (ID: 123456) requesting voice call to 'bob'
[CALL] Found target user 'bob' with ID: 789012
[CALL] Sending call request to target user 'bob'
```

**Bob's terminal should show:**
```
[VOICE] Incoming voice call from alice (ID: call_1234567890)
[VOICE] Type /accept call_1234567890 to accept or /reject call_1234567890 to reject
```

**Alice's terminal should show:**
```
[VOICE] Initiating voice call to bob
```

## **What Was Fixed**

### **Before (Broken):**
```cpp
// Server was looking for users by username in ws_to_user map
// But ws_to_user stores users by user ID (WebSocket pointer)
for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
    if (ws_uid == target_user) {  // ❌ This never matched!
        ws_ptr->send(call_request.dump());
        break;
    }
}
```

### **After (Fixed):**
```cpp
// First find the target user's ID by username
std::string target_user_id;
for (const auto& [user_id, user] : server.users) {
    if (user.username == target_user) {
        target_user_id = user_id;
        break;
    }
}

// Then find their WebSocket connection by user ID
for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
    if (ws_uid == target_user_id) {  // ✅ Now this matches!
        ws_ptr->send(call_request.dump());
        break;
    }
}
```

## **Debug Information**

The server now logs:
- ✅ Call request details
- ✅ Target user lookup results
- ✅ WebSocket connection status
- ✅ Call notification delivery

## **Test Scenarios**

### **Scenario 1: Successful Call**
1. Alice calls Bob
2. Bob receives notification
3. Bob accepts call
4. Both users see call established

### **Scenario 2: User Not Found**
1. Alice calls "nonexistent_user"
2. Server logs: `[CALL] Target user 'nonexistent_user' not found`
3. Alice gets error message

### **Scenario 3: User Not Connected**
1. Alice calls Bob (but Bob disconnected)
2. Server logs: `[CALL] Target user 'bob' not connected (no WebSocket found)`
3. Alice gets error message

## **Expected Call Flow**

```
Alice                    Server                    Bob
 |                         |                        |
 | /call bob              |                        |
 |------------------------>|                        |
 |                         | [CALL] User 'alice'... |
 |                         | [CALL] Found target... |
 |                         | [CALL] Sending call... |
 |                         |------------------------>|
 |                         |                        | [VOICE] Incoming call...
 |                         |                        |
 |                         |<------------------------|
 |                         | /accept call_123...     |
 |                         |------------------------>|
 |                         | [CALL] Call accepted... |
 |<------------------------|                        |
 | [VOICE] Call accepted   |                        |
```

## **Troubleshooting**

If notifications still don't work:

1. **Check server logs** for `[CALL]` messages
2. **Verify both users are connected** to the same server
3. **Check usernames match** exactly (case-sensitive)
4. **Ensure users are in the same channel**

## **Success Indicators**

✅ **Server logs show:**
```
[CALL] User 'alice' requesting voice call to 'bob'
[CALL] Found target user 'bob' with ID: 123456
[CALL] Sending call request to target user 'bob'
```

✅ **Bob receives:**
```
[VOICE] Incoming voice call from alice (ID: call_1234567890)
```

✅ **Alice receives:**
```
[VOICE] Initiating voice call to bob
```

**The voice call notifications should now work properly!** 🎉 