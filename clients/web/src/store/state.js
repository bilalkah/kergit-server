// src/store/state.js
const defaultSelf = () => ({ id: null, publicId: null, email: null, username: null, fullName: null, displayName: null });
const defaultCurrent = () => ({ hubId: null, channelId: null, channelName: '' });
const defaultHeartbeat = () => ({ latencyMs: null, serverTs: null, receivedAt: null });
const defaultSession = () => ({ url: null, token: null, username: null });
const defaultHubCount = () => 0;

const STORAGE_KEYS = {
  hub: 'sc:lastHubId'
};

const safeStorage = {
  get(key) {
    if (typeof window === 'undefined' || !window.localStorage) return null;
    try {
      return window.localStorage.getItem(key);
    } catch {
      return null;
    }
  },
  set(key, value) {
    if (typeof window === 'undefined' || !window.localStorage) return;
    try {
      if (typeof value === 'string' && value.length > 0) {
        window.localStorage.setItem(key, value);
      } else {
        window.localStorage.removeItem(key);
      }
    } catch {
      // ignore
    }
  }
};

const rememberHubSelection = (hubId) => {
  safeStorage.set(STORAGE_KEYS.hub, hubId || '');
};

export const getPreferredHubId = () => safeStorage.get(STORAGE_KEYS.hub);

export const state = {
  connection: 'disconnected',  // 'connected' | 'connecting' | 'disconnected'
  authed: false,
  self: defaultSelf(),
  hubs: [],                    // [{id,name,created_at}]
  channelsByHub: {},           // hubId -> [{id,name,type,created_at}]
  hubCount: defaultHubCount(),
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
  state.hubCount = defaultHubCount();
  state.current = defaultCurrent();
  state.usersByChannel = {};
  state.messagesByChannel = {};
  state.membersByHub = {};
  state.heartbeat = defaultHeartbeat();
  state.session = defaultSession();
  rememberHubSelection(null);
}

const normalizeMember = (raw = {}) => {
  const handle = raw.handle || raw.username || raw.display_name || '';
  const display = raw.display_name || raw.username || handle || 'Member';
  const userId = raw.user_id || raw.id || null;
  const online = raw.online === true;
  return {
    handle: handle || display,
    display_name: display,
    online,
    user_id: userId
  };
};

export const actions = {
  setConnection(s) { state.connection = s; },
  setAuth(ok, self) {
    state.authed = ok;
    if (self) {
      const prev = state.self || defaultSelf();
      state.self = {
        ...prev,
        ...self,
      };
      if (!state.self.displayName) {
      state.self.displayName = state.self.username || state.self.fullName || prev.displayName;
      }
    } else if (!ok) {
      state.self = defaultSelf();
    }
  },
  setList({ hubs, channels_by_hub }) {
    state.hubs = Array.isArray(hubs)
      ? hubs.map((hub) => ({ ...hub, role: hub.role || '' }))
      : [];
    state.channelsByHub = channels_by_hub || {};
    state.hubCount = Array.isArray(hubs) ? hubs.length : state.hubCount;
  },
  setHubCount(count) {
    const next = Number.isFinite(count) ? Number(count) : defaultHubCount();
    state.hubCount = next < 0 ? 0 : next;
  },
  setJoinedChannel({ hub_id, channel_id, channel_name, members, history }) {
    state.current.channelId = channel_id;
    state.current.channelName = channel_name || '';
    const fallbackMembers = hub_id ? state.membersByHub[hub_id] : null;
    const sourceMembers = Array.isArray(members) && members.length
      ? members
      : (fallbackMembers || []);
    state.usersByChannel[channel_id] = sourceMembers.map(normalizeMember);
    state.messagesByChannel[channel_id] = (history || []).map(h => ({
      sender: h.sender || 'Member', content: h.content, sent_at: h.sent_at
    }));
  },
  setChannelPresence(channel_id, members) {
    if (!channel_id) return;
    state.usersByChannel[channel_id] = Array.isArray(members)
      ? members.map(normalizeMember)
      : [];
  },
  pushMessage({ channel_id, sender, content, sent_at }) {
    if (!state.messagesByChannel[channel_id]) state.messagesByChannel[channel_id] = [];
    state.messagesByChannel[channel_id].push({ sender: sender || 'Member', content, sent_at });
  },
  upsertPresence({ channel_id, handle, display_name, online, user_id }) {
    const arr = state.usersByChannel[channel_id] || [];
    const key = handle || display_name;
    if (!key) return;
    const i = arr.findIndex(u => u.handle === key);
    if (i >= 0) {
      arr[i].online = online;
      if (display_name) arr[i].display_name = display_name;
      if (user_id) arr[i].user_id = user_id;
    } else {
      arr.push({ handle: key, display_name: display_name || key, online, user_id: user_id || null });
    }
    state.usersByChannel[channel_id] = arr;
  },

  // 🔽 NEW: switch hub (reset channel selection)
  setCurrentHub(hubId) {
    state.current.hubId = hubId || null;
    state.current.channelId = null;
    state.current.channelName = '';
    rememberHubSelection(state.current.hubId);
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

  updateSelfProfile({ username, full_name, display_name }) {
    if (typeof username !== 'undefined') {
      state.self.username = username ? username : null;
    }
    if (typeof full_name !== 'undefined') {
      state.self.fullName = full_name ? full_name : null;
    }
    if (display_name) {
      state.self.displayName = display_name;
    }
    if (!state.self.displayName) {
      state.self.displayName = state.self.username || state.self.fullName;
    }
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

actions.addHub = function addHub(hub) {
  if (!hub || !hub.id) return;
  const hubs = state.hubs || [];
  const idx = hubs.findIndex((h) => h.id === hub.id);
  if (idx >= 0) {
    hubs[idx] = { ...hubs[idx], ...hub };
  } else {
    hubs.push({ ...hub });
  }
  state.hubs = hubs;
};

actions.removeChannel = function removeChannel(hubId, channelId) {
  if (!hubId || !channelId) return;
  const list = state.channelsByHub[hubId];
  if (!Array.isArray(list)) return;
  state.channelsByHub[hubId] = list.filter((c) => c.id !== channelId);
  if (state.current.channelId === channelId) {
    state.current.channelId = null;
    state.current.channelName = '';
  }
  if (state.usersByChannel[channelId]) delete state.usersByChannel[channelId];
};

actions.updateHubRole = function updateHubRole(hubId, role) {
  if (!hubId) return;
  const hubs = state.hubs || [];
  const idx = hubs.findIndex((h) => h.id === hubId);
  if (idx >= 0) {
    hubs[idx] = { ...hubs[idx], role: role || hubs[idx].role };
  }
};

actions.renameChannel = function renameChannel(hubId, channelId, name) {
  if (!hubId || !channelId || typeof name !== 'string') return;
  const list = state.channelsByHub[hubId];
  if (!Array.isArray(list)) return;
  const idx = list.findIndex((c) => c.id === channelId);
  if (idx >= 0) {
    const updated = [...list];
    updated[idx] = { ...updated[idx], name };
    state.channelsByHub[hubId] = updated;
  }
  if (state.current.channelId === channelId) {
    state.current.channelName = name;
  }
};

actions.renameHub = function renameHub(hubId, name) {
  if (!hubId || typeof name !== 'string') return;
  const hubs = state.hubs || [];
  const idx = hubs.findIndex((h) => h.id === hubId);
  if (idx >= 0) {
    const updated = [...hubs];
    updated[idx] = { ...updated[idx], name };
    state.hubs = updated;
  }
};

actions.removeHub = function removeHub(hubId) {
  if (!hubId) return null;
  const hubs = state.hubs || [];
  const idx = hubs.findIndex((h) => h.id === hubId);
  if (idx < 0) return null;
  const updated = [...hubs];
  updated.splice(idx, 1);
  state.hubs = updated;
  const channels = state.channelsByHub[hubId] || [];
  channels.forEach((channel) => {
    if (channel?.id && state.usersByChannel[channel.id]) {
      delete state.usersByChannel[channel.id];
    }
    if (channel?.id && state.messagesByChannel[channel.id]) {
      delete state.messagesByChannel[channel.id];
    }
  });
  delete state.channelsByHub[hubId];
  delete state.membersByHub[hubId];
  if (state.current.hubId === hubId) {
    state.current.hubId = null;
    state.current.channelId = null;
    state.current.channelName = '';
  }
  return updated.length ? updated[0].id : null;
};

actions.updateHubMemberPresence = function updateHubMemberPresence(hubId, userId, online, displayName) {
  if (!hubId || !userId) return;
  const members = state.membersByHub[hubId] ? [...state.membersByHub[hubId]] : [];
  const idx = members.findIndex((m) => m.user_id === userId);

  if (idx >= 0) {
    if (online) {
      const updated = { ...members[idx], online: true };
      if (displayName) {
        updated.display_name = displayName;
        updated.handle = members[idx].handle || displayName;
      }
      members[idx] = updated;
    } else {
      members.splice(idx, 1);  // remove member when offline/left
    }
  } else {
    if (!online) {
      state.membersByHub[hubId] = members;
      return;
    }
    const name = displayName || 'Member';
    members.push({
      handle: displayName || userId,
      display_name: name,
      online,
      user_id: userId
    });
  }
  state.membersByHub[hubId] = members;
};
