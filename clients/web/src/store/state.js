// src/store/state.js
export const state = {
  connection: 'disconnected',  // 'connected' | 'connecting' | 'disconnected'
  authed: false,
  self: { id: null, email: null, username: null },
  hubs: [],                    // [{id,name,created_at}]
  channelsByHub: {},           // hubId -> [{id,name,type,created_at}]
  current: { hubId: null, channelId: null, channelName: '' },
  usersByChannel: {},          // channelId -> [{user_id,email,role,online}]
  messagesByChannel: {},       // channelId -> [{sender,content,sent_at}]
  membersByHub: {},            // hubId -> [{user_id,email,username}]
  heartbeat: {
    latencyMs: null,
    serverTs: null,
    receivedAt: null
  },
  session: {
    url: null,
    token: null,
    username: null
  }
};

export const actions = {
  setConnection(s) { state.connection = s; },
  setAuth(ok, self) { state.authed = ok; if (self) state.self = self; },
  setList({ hubs, channels_by_hub }) {
    state.hubs = hubs || [];
    state.channelsByHub = channels_by_hub || {};
  },
  setJoinedChannel({ channel_id, channel_name, members, history }) {
    state.current.channelId = channel_id;
    state.current.channelName = channel_name || '';
    state.usersByChannel[channel_id] = (members || []);
    state.messagesByChannel[channel_id] = (history || []).map(h => ({
      sender: h.sender_name || h.sender_id, content: h.content, sent_at: h.sent_at
    }));
  },
  pushMessage({ channel_id, sender, content, sent_at }) {
    if (!state.messagesByChannel[channel_id]) state.messagesByChannel[channel_id] = [];
    state.messagesByChannel[channel_id].push({ sender, content, sent_at });
  },
  upsertPresence({ channel_id, user_id, email, role, online }) {
    const arr = state.usersByChannel[channel_id] || [];
    const i = arr.findIndex(u => u.user_id === user_id);
    if (i >= 0) arr[i].online = online;
    else arr.push({ user_id, email, role, online });
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
        state.membersByHub[hubId] = Array.isArray(members) ? members : [];
      });
    }
  },

  setHubMembers(hubId, members) {
    if (!hubId) return;
    state.membersByHub[hubId] = Array.isArray(members) ? members : [];
  },

  updateHubChannels(hubId, channels) {
    if (!hubId) return;
    state.channelsByHub[hubId] = Array.isArray(channels) ? channels : [];
  },

  setSession(info) {
    if (!info) {
      state.session = { url: null, token: null, username: null };
      return;
    }
    state.session = {
      url: info.url ?? null,
      token: info.token ?? null,
      username: info.username ?? null
    };
  }
};
