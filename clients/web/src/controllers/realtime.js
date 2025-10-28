// src/controllers/realtime.js
import { state, actions } from '../store/state.js';

export function wireRealtime({ ws, els }) {
  // Connection → status dot
  ws.on('__open__', () => actions.setConnection('connected'));
  ws.on('__close__', () => actions.setConnection('disconnected'));
  ws.on('__stalled__', () => actions.setConnection('connecting')); // missed pong, warn UI

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
