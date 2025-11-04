// src/controllers/realtime.js
import { sendAuth } from '../api/chatApi.js';
import { state, actions } from '../store/state.js';

export function wireRealtime({ ws, els }) {
  const { pingDisplay, connectionLostModal, connectionLostMessage, connectionLostOk, loginScreen, chatScreen } = els;
  const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const reconnectAttempts = [
    { waitBefore: 0 },
    { waitBefore: 5000 },
    { waitBefore: 5000 },
    { waitBefore: 15000 },
    { waitBefore: 15000 },
    { waitBefore: 15000 }
  ];
  const countdownWait = async (totalMs, render) => {
    let remaining = totalMs;
    const tick = 1000;
    while (remaining > 0) {
      if (cancelReconnect) return false;
      render(remaining);
      const slice = Math.min(tick, remaining);
      await wait(slice);
      remaining -= slice;
    }
    return !cancelReconnect;
  };

  const updatePingDisplay = (info) => {
    if (!pingDisplay) return;
    pingDisplay.classList.remove('is-good', 'is-warn', 'is-bad', 'is-idle');
    if (!info || info.latencyMs == null) {
      pingDisplay.textContent = 'Ping: --';
      pingDisplay.classList.add('is-idle');
      return;
    }
    const latency = Math.round(info.latencyMs);
    pingDisplay.textContent = `Ping: ${latency} ms`;
    if (latency <= 100) pingDisplay.classList.add('is-good');
    else if (latency <= 250) pingDisplay.classList.add('is-warn');
    else pingDisplay.classList.add('is-bad');
  };

  let stalled = false;
  let reconnectTask = null;
  let cancelReconnect = false;

  const updateReconnectMessage = (message) => {
    if (connectionLostMessage && typeof message === 'string') {
      connectionLostMessage.textContent = message;
    }
  };

  const showConnectionLost = (message) => {
    if (!connectionLostModal) return;
    stalled = true;
    updateReconnectMessage(message);
    connectionLostModal.classList.remove('hidden');
    actions.setConnection('connecting');
  };

  const hideConnectionLost = () => {
    if (!connectionLostModal) return;
    connectionLostModal.classList.add('hidden');
  };

  const resetReconnectState = () => {
    cancelReconnect = false;
    reconnectTask = null;
  };

  const goToLogin = () => {
    hideConnectionLost();
    stalled = false;
    actions.setAuth(false);
    actions.setConnection('disconnected');
    actions.setSession(null);
    if (chatScreen) chatScreen.classList.add('hidden');
    if (loginScreen) loginScreen.classList.remove('hidden');
    updatePingDisplay(null);
  };

  const attemptReconnect = () => {
    if (reconnectTask) return reconnectTask;
    cancelReconnect = false;
    reconnectTask = (async () => {
      if (!state.authed) {
        resetReconnectState();
        return false;
      }
      const { url, token, username } = state.session || {};
      if (!url || !token || !username) {
        updateReconnectMessage('Server connection lost. Please sign in again.');
        await wait(1500);
        goToLogin();
        resetReconnectState();
        return false;
      }

      for (let i = 0; i < reconnectAttempts.length; i += 1) {
        if (cancelReconnect) break;
        const attemptNo = i + 1;
        const waitBefore = reconnectAttempts[i].waitBefore || 0;
        if (waitBefore > 0 && attemptNo > 1) {
          const proceeded = await countdownWait(waitBefore, (msLeft) => {
            updateReconnectMessage(
              `Reconnect attempt ${attemptNo - 1} failed. Retrying in ${Math.ceil(msLeft / 1000)}s…`
            );
          });
          if (!proceeded) break;
        } else if (attemptNo === 1) {
          updateReconnectMessage('Connection lost. Attempting to reconnect…');
        }

        updateReconnectMessage(`Reconnecting… (attempt ${attemptNo}/${reconnectAttempts.length})`);
        try {
          await ws.connect(url);
          const authRes = await sendAuth(ws, token, username);
          if (!authRes?.success) throw new Error('auth failed');
          stalled = false;
          hideConnectionLost();
          actions.setConnection('connected');
          actions.setAuth(true, state.self);
          actions.setHeartbeat({});
          updatePingDisplay(null);
          resetReconnectState();
          return true;
        } catch (err) {
          console.warn('[WS] reconnect attempt failed', err);
          updateReconnectMessage(`Reconnect attempt ${attemptNo} failed.`);
          if (i === reconnectAttempts.length - 1) {
            break;
          }
        }
      }

      if (!cancelReconnect) {
        updateReconnectMessage('Server connection lost. Please sign in again.');
        await wait(2000);
        goToLogin();
      }
      resetReconnectState();
      return false;
    })();
    return reconnectTask;
  };

  connectionLostOk?.addEventListener('click', () => {
    cancelReconnect = true;
    ws.disconnect?.(1000, 'acknowledged connection loss');
    goToLogin();
  });

  // Connection → status dot
  ws.on('__open__', () => {
    stalled = false;
    hideConnectionLost();
    actions.setConnection('connected');
    actions.setHeartbeat({});
    updatePingDisplay(null);
    resetReconnectState();
  });
  ws.on('__close__', (info) => {
    actions.setConnection('disconnected');
    actions.setHeartbeat({});
    updatePingDisplay(null);
    if (info?.manual) {
      cancelReconnect = true;
      return;
    }
    if (!state.authed) {
      return;
    }
    showConnectionLost('Connection to the server was lost. Attempting to reconnect…');
    attemptReconnect();
  });
  ws.on('__stalled__', ({ missFor } = {}) => {
    actions.setConnection('connecting'); // missed pong, warn UI
    actions.setHeartbeat({});
    updatePingDisplay(null);
    const seconds = missFor ? Math.round(missFor / 1000) : null;
    const message = seconds
      ? `No heartbeat received for ${seconds}s. Attempting to reconnect…`
      : 'Heartbeat timed out. Attempting to reconnect…';
    showConnectionLost(message);
    attemptReconnect();
  });

  ws.on('__ping__', (info) => {
    actions.setHeartbeat(info || {});
    updatePingDisplay(info);
  });

  // Server may also push explicit status updates
  ws.on('status', (msg) => {
    // expect: {type:'status', value:'connected'|'connecting'|'disconnected'}
    if (msg?.value) actions.setConnection(msg.value);
  });

  // Hubs & Channels list (after auth)
  ws.on('hubs_list', (msg) => {
    // expect: {type:'hubs_list', hubs:[{id,name,created_at}], channels_by_hub:{[hubId]:[...]}}
    const hubs = Array.isArray(msg?.hubs) ? msg.hubs : [];
    const channels_by_hub = msg?.channels_by_hub || {};
    actions.setList({ hubs, channels_by_hub });

    // if no hub selected, UI can show the hub grid
    if (!state.current.hubId && hubs.length) {
      // let UI decide; no direct DOM here
      document.dispatchEvent(new CustomEvent('hubs:ready', { detail: { count: hubs.length } }));
    }
  });

  // Presence snapshot for a channel
  ws.on('presence_snapshot', (msg) => {
    // expect: {type:'presence_snapshot', channel_id, members:[{user_id,email,role,online}]}
    const { channel_id, members } = msg || {};
    if (!channel_id || !Array.isArray(members)) return;
    actions.setJoinedChannel({ channel_id, channel_name: state.current.channelName, members, history: [] });
    // notify UI to render users sidebar if needed
    document.dispatchEvent(new CustomEvent('presence:updated', { detail: { channel_id } }));
  });

  // Presence incremental updates
  ws.on('presence_update', (msg) => {
    // expect: {type:'presence_update', channel_id, user_id, email, role, online:true|false}
    const { channel_id, user_id, email, role, online } = msg || {};
    if (!channel_id || !user_id) return;
    actions.upsertPresence({ channel_id, user_id, email, role, online });
    document.dispatchEvent(new CustomEvent('presence:updated', { detail: { channel_id } }));
  });

  // Messages from server (just pass to your existing messageController via store)
  ws.on('message', (msg) => {
    // expect: {type:'message', channel_id, sender_name|sender_id, content, sent_at}
    const ch = msg?.channel_id;
    if (!ch) return;
    actions.pushMessage({
      channel_id: ch,
      sender: msg.sender_name || msg.sender_id || 'unknown',
      content: msg.content ?? '',
      sent_at: msg.sent_at || new Date().toISOString()
    });
    document.dispatchEvent(new CustomEvent('messages:updated', { detail: { channel_id: ch } }));
  });
}
