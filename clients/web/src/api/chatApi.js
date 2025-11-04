// src/api/chatApi.js
export function sendAuth(wsClient, token, username) {
  return new Promise((resolve, reject) => {
    const timeoutErr = new Error('Authentication timed out');
    timeoutErr.code = 'AUTH_TIMEOUT';
    const timeout = setTimeout(() => { off(); reject(timeoutErr); }, 5000);
    const off = wsClient.on('auth_response', (msg) => {
      clearTimeout(timeout); off();
      if (msg?.success) {
        resolve(msg);
        return;
      }
      const err = new Error(msg?.error || 'Authentication failed');
      if (msg?.code) err.code = msg.code;
      err.payload = msg;
      reject(err);
    });
    wsClient.send({ type: 'auth', token, username, 'ts': Date.now() });
  });
}


export function reqList(ws) {
  ws.send({ type: 'list', include: ['hubs', 'channels'], ts: Date.now() });
}
export function joinChannel(ws, channel_id) {
  ws.send({ type: 'join_channel', channel_id, ts: Date.now() });
}
export function reqUsersForChannel(ws, channel_id) {
  ws.send({ type: 'users', scope: 'channel', channel_id, ts: Date.now() });
}
export function sendMessage(ws, channel_id, content) {
  ws.send({ type: 'send_message', channel_id, content, ts: Date.now() });
}

export function createChannel(ws, hub_id, name) {
  ws.send({ type: 'create_channel', hub_id, name, ts: Date.now() });
}
