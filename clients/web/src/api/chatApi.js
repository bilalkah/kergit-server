// src/api/chatApi.js
export function sendAuth(wsClient, token, username) {
  return new Promise((resolve, reject) => {
    const timeoutErr = new Error('Authentication timed out');
    timeoutErr.code = 'AUTH_TIMEOUT';
    const timeout = setTimeout(() => { off(); reject(timeoutErr); }, 10000);
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

export function createHub(ws, name) {
  ws.send({ type: 'create_hub', name, ts: Date.now() });
}

export function deleteChannel(ws, channel_id) {
  ws.send({ type: 'delete_channel', channel_id, ts: Date.now() });
}

export function renameChannel(ws, channel_id, name) {
  ws.send({ type: 'rename_channel', channel_id, name, ts: Date.now() });
}

export function updateMemberRole(ws, { hub_id, user_id, role }) {
  ws.send({ type: 'update_member_role', hub_id, user_id, role, ts: Date.now() });
}

export function updateProfile(ws, { username, full_name } = {}) {
  const payload = { type: 'update_profile', ts: Date.now() };
  if (typeof username !== 'undefined') payload.username = username;
  if (typeof full_name !== 'undefined') payload.full_name = full_name;
  ws.send(payload);
}

export function renameHub(ws, hub_id, name) {
  ws.send({ type: 'rename_hub', hub_id, name, ts: Date.now() });
}

export function deleteHub(ws, hub_id) {
  ws.send({ type: 'delete_hub', hub_id, ts: Date.now() });
}

export function requestHubInvite(ws, hub_id) {
  ws.send({ type: 'generate_hub_invite', hub_id, ts: Date.now() });
}

export function joinHubByCode(ws, invite_code) {
  ws.send({ type: 'join_hub_by_code', invite_code, ts: Date.now() });
}

export function leaveHub(ws, hub_id) {
  ws.send({ type: 'leave_hub', hub_id, ts: Date.now() });
}
