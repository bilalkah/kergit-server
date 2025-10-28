// src/api/chatApi.js
export function sendAuth(wsClient, token, username) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => { off(); reject(new Error('Auth timeout')); }, 5000);
    const off = wsClient.on('auth_response', (msg) => {
      clearTimeout(timeout); off();
      msg?.success ? resolve(msg) : reject(new Error(msg?.error || 'Authentication failed'));
    });
    wsClient.send({ type: 'auth', token, username });
  });
}


export function reqList(ws) {
  ws.send({ type: 'list', include: ['hubs', 'channels'] });
}
export function joinChannel(ws, channel_id) {
  ws.send({ type: 'join_channel', channel_id });
}
export function reqUsersForChannel(ws, channel_id) {
  ws.send({ type: 'users', scope: 'channel', channel_id });
}
export function sendMessage(ws, channel_id, content) {
  ws.send({ type: 'send_message', channel_id, content });
}
