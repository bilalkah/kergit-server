// src/store/state.js
const defaultSelf = () => ({ id: null, email: null, username: null });
const defaultCurrent = () => ({ hubId: null, channelId: null, channelName: '' });
const defaultHeartbeat = () => ({ latencyMs: null, serverTs: null, receivedAt: null });
const defaultSession = () => ({ url: null, token: null, username: null });

export const state = {
  connection: 'disconnected',  // 'connected' | 'connecting' | 'disconnected'
  authed: false,
  self: defaultSelf(),
  hubs: [],                    // [{id,name,created_at}]
  channelsByHub: {},           // hubId -> [{id,name,type,created_at}]
  current: defaultCurrent(),
  usersByChannel: {},          // channelId -> [{handle,display_name,online}]
  messagesByChannel: {},       // channelId -> [{sender,content,sent_at}]
  membersByHub: {},            // hubId -> [{handle,display_name,online?}]
  heartbeat: defaultHeartbeat(),
  session: defaultSession()
};

function resetState() {
  state.connection = 'disconnected';
  state.authed = false;
  state.self = defaultSelf();
  state.hubs = [];
  state.channelsByHub = {};
  state.current = defaultCurrent();
  state.usersByChannel = {};
  state.messagesByChannel = {};
  state.membersByHub = {};
  state.heartbeat = defaultHeartbeat();
  state.session = defaultSession();
}

const normalizeMember = (raw = {}) => {
  const handle = raw.handle || raw.username || raw.display_name || '';
  const display = raw.display_name || raw.username || handle || 'Member';
  return {
    handle: handle || display,
    display_name: display,
    online: raw.online !== false
  };
};

export const actions = {
  setConnection(s) { state.connection = s; },
  setAuth(ok, self) { state.authed = ok; if (self) state.self = self; },
  setList({ hubs, channels_by_hub }) {
    state.hubs = Array.isArray(hubs)
      ? hubs.map((hub) => ({ ...hub, role: hub.role || '' }))
      : [];
    state.channelsByHub = channels_by_hub || {};
  },
  setJoinedChannel({ channel_id, channel_name, members, history }) {
    state.current.channelId = channel_id;
    state.current.channelName = channel_name || '';
    state.usersByChannel[channel_id] = (members || []).map(normalizeMember);
    state.messagesByChannel[channel_id] = (history || []).map(h => ({
      sender: h.sender || 'Member', content: h.content, sent_at: h.sent_at
    }));
  },
  pushMessage({ channel_id, sender, content, sent_at }) {
    if (!state.messagesByChannel[channel_id]) state.messagesByChannel[channel_id] = [];
    state.messagesByChannel[channel_id].push({ sender: sender || 'Member', content, sent_at });
  },
  upsertPresence({ channel_id, handle, display_name, online }) {
    const arr = state.usersByChannel[channel_id] || [];
    const key = handle || display_name;
    if (!key) return;
    const i = arr.findIndex(u => u.handle === key);
    if (i >= 0) {
      arr[i].online = online;
      if (display_name) arr[i].display_name = display_name;
    } else {
      arr.push({ handle: key, display_name: display_name || key, online });
    }
    state.usersByChannel[channel_id] = arr;
  },

  // 🔽 NEW: switch hub (reset channel selection)
  setCurrentHub(hubId) {
    state.current.hubId = hubId || null;
    state.current.channelId = null;
    state.current.channelName = '';
  },

  setHeartbeat({ latencyMs = null, serverTs = null, receivedAt = null } = {}) {
    state.heartbeat.latencyMs = latencyMs;
    state.heartbeat.serverTs = serverTs;
    state.heartbeat.receivedAt = receivedAt;
  },

  setHubPresenceMap(map = {}) {
    state.membersByHub = {};
    if (map && typeof map === 'object') {
      Object.entries(map).forEach(([hubId, members]) => {
        state.membersByHub[hubId] = Array.isArray(members)
          ? members.map(normalizeMember)
          : [];
      });
    }
  },

  setHubMembers(hubId, members) {
    if (!hubId) return;
    state.membersByHub[hubId] = Array.isArray(members)
      ? members.map(normalizeMember)
      : [];
  },

  updateHubChannels(hubId, channels) {
    if (!hubId) return;
    state.channelsByHub[hubId] = Array.isArray(channels) ? channels : [];
  },

  appendChannel(hubId, channel) {
    if (!hubId || !channel) return;
    const list = state.channelsByHub[hubId] ? [...state.channelsByHub[hubId]] : [];
    const exists = list.some(c => c.id === channel.id);
    if (exists) return;
    list.push(channel);
    state.channelsByHub[hubId] = list;
  },

  setSession(info) {
    if (!info) {
      state.session = defaultSession();
      return;
    }
    state.session = {
      url: info.url ?? null,
      token: info.token ?? null,
      username: info.username ?? null
    };
  },

  reset() {
    resetState();
  }
};
