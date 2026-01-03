// src/controllers/realtime.js
import { sendAuth } from '../api/chatApi.js';
import { state, actions } from '../store/state.js';
import { sel } from '../store/selectors.js';
import { renderChannels } from '../views/sidebar.js';
import { renderUsers } from '../views/chat.js';

export function wireRealtime({ ws, els }) {
  const {
    pingDisplay,
    connectionLostModal,
    connectionLostMessage,
    connectionLostOk,
    loginScreen,
    chatScreen,
    channelsList,
    channelsSection,
    channelEmptyState,
    chatEmptyState,
    usersList,
    userCount,
    membersSidebar,
    currentHubName,
    messagesWrap,
    inputArea
  } = els;
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

  const formatCloseReason = (info) => {
    if (!info) {
      return 'Connection to the server was lost. Attempting to reconnect…';
    }
    if (info.reason) {
      return `Disconnected: ${info.reason}. Attempting to reconnect…`;
    }
    if (info.code) {
      return `Disconnected (code ${info.code}). Attempting to reconnect…`;
    }
    return 'Connection to the server was lost. Attempting to reconnect…';
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
    cancelReconnect = true;
    hideConnectionLost();
    stalled = false;
    actions.setAuth(false);
    actions.setConnection('disconnected');
    actions.setSession(null);
    actions.setHubPresenceMap({});
    if (chatScreen) chatScreen.classList.add('hidden');
    if (loginScreen) loginScreen.classList.remove('hidden');
    updatePingDisplay(null);
  };

  const attemptReconnect = () => {
    if (reconnectTask) return reconnectTask;
    cancelReconnect = false;
    reconnectTask = (async () => {
      for (let i = 0; i < reconnectAttempts.length; i += 1) {
        if (cancelReconnect) break;
        if (!state.authed) {
          break;
        }
        const { url, token, username } = state.session || {};
        if (!url || !token || !username) {
          goToLogin();
          break;
        }
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
          actions.setHubCount(typeof authRes?.hub_count === 'number' ? authRes.hub_count : 0);
          if (Array.isArray(authRes?.hubs)) {
            actions.setList({
              hubs: authRes.hubs,
              channels_by_hub: authRes.channels_by_hub || {}
            });
            actions.setHubPresenceMap(authRes.members_by_hub || {});
          }
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
        if (state.authed) {
          updateReconnectMessage('Server connection lost. Please sign in again.');
          await wait(2000);
        }
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
      resetReconnectState();
      return;
    }
    if (!state.authed) {
      resetReconnectState();
      return;
    }
    if (!state.session || !state.session.token) {
      goToLogin();
      return;
    }
    showConnectionLost(formatCloseReason(info));
    attemptReconnect();
  });
  ws.on('__stalled__', ({ missFor } = {}) => {
    const seconds = missFor ? Math.round(missFor / 1000) : null;
    console.warn('[WS] heartbeat stalled', seconds != null ? `${seconds}s` : '');
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
    const online_by_hub = msg?.online_by_hub || {};

    actions.setHubCount(
      typeof msg?.hub_count === 'number' ? msg.hub_count : (Array.isArray(hubs) ? hubs.length : 0)
    );
    actions.setHubPresenceMap(online_by_hub);
    actions.setList({ hubs, channels_by_hub });

    const currentHubId = state.current.hubId;
    if (currentHubId && channelsList) {
      const role = sel.currentHubRole();
      const canDelete = role === 'owner' || role === 'admin';
      renderChannels(channelsList, sel.channels(currentHubId), sel.currentChannelId(), { canDelete });
    }
    if (currentHubId && usersList && userCount) {
      const roster = state.current.channelId
        ? sel.usersInChannel(state.current.channelId)
        : sel.membersInHub(currentHubId);
      renderUsers(usersList, userCount, roster);
      membersSidebar?.classList.remove('hidden');
    }

    if (!currentHubId && hubs.length) {
      document.dispatchEvent(new CustomEvent('hubs:ready', { detail: { count: hubs.length } }));
    }

    if (currentHubName) {
      const activeHub = hubs.find((h) => h.id === state.current.hubId);
      currentHubName.textContent = activeHub?.name || 'Select Hub';
    }
  });

  ws.on('hub_snapshot', (msg) => {
    const { hub_id, channels, online } = msg || {};
    if (!hub_id) return;
    actions.updateHubChannels(hub_id, Array.isArray(channels) ? channels : []);
    actions.setHubMembers(hub_id, Array.isArray(online) ? online : []);

    if (state.current.hubId === hub_id) {
      const list = sel.channels(hub_id);
      if (channelsList) {
        const role = sel.currentHubRole();
        const canDelete = role === 'owner' || role === 'admin';
        renderChannels(channelsList, list, sel.currentChannelId(), { canDelete });
        if (list.length) channelsSection?.classList.remove('hidden');
      }
      if (channelEmptyState) {
        if (list.length) channelEmptyState.classList.add('hidden');
        else {
          channelsSection?.classList.add('hidden');
          const text = channelEmptyState.querySelector('p');
          if (text) text.textContent = 'No channels yet. Create one to start the conversation.';
          channelEmptyState.classList.remove('hidden');
        }
      }
      if (usersList && userCount) {
        const roster = state.current.channelId
          ? sel.usersInChannel(state.current.channelId)
          : sel.membersInHub(hub_id);
        renderUsers(usersList, userCount, roster);
        membersSidebar?.classList.remove('hidden');
      }
      if (!state.current.channelId && chatEmptyState) {
        const text = chatEmptyState.querySelector('p');
        if (text) text.textContent = list.length
          ? 'Select a channel to start chatting.'
          : 'Create a channel to start chatting.';
        chatEmptyState.classList.remove('hidden');
      }
    }

    if (currentHubName) {
      const activeHub = state.hubs.find((h) => h.id === state.current.hubId);
      currentHubName.textContent = activeHub?.name || 'Select Hub';
    }
  });

  ws.on('channel_created', (msg) => {
    const { hub_id, channel } = msg || {};
    if (!hub_id || !channel) return;
    actions.appendChannel(hub_id, channel);
    if (state.current.hubId === hub_id) {
      const list = sel.channels(hub_id);
      if (channelsList) {
        const role = sel.currentHubRole();
        const canDelete = role === 'owner' || role === 'admin';
        renderChannels(channelsList, list, sel.currentChannelId(), { canDelete });
      }
      if (channelEmptyState && list.length) {
        channelEmptyState.classList.add('hidden');
        channelsSection?.classList.remove('hidden');
      }
    }
  });

  // Presence snapshot for a channel
  ws.on('presence_snapshot', (msg) => {
    // expect: {type:'presence_snapshot', channel_id, members:[{handle,display_name,online}]}
    const { channel_id, members } = msg || {};
    if (!channel_id || !Array.isArray(members)) return;
    actions.setChannelPresence(channel_id, members);
    // notify UI to render users sidebar if needed
    document.dispatchEvent(new CustomEvent('presence:updated', { detail: { channel_id } }));
  });

  // Presence incremental updates
  ws.on('presence_update', (msg) => {
    // expect: {type:'presence_update', channel_id, handle, display_name, online:true|false}
    const { channel_id, handle, display_name, online } = msg || {};
    if (!channel_id) return;
    actions.upsertPresence({ channel_id, handle, display_name, online });
    document.dispatchEvent(new CustomEvent('presence:updated', { detail: { channel_id } }));
  });

  ws.on('member_online', (msg = {}) => {
    const hubId = msg.hub_id;
    const userId = msg.user_id;
    const displayName = msg.display_name || msg.handle;
    if (!hubId || !userId) return;
    actions.updateHubMemberPresence(hubId, userId, true, displayName);
    if (state.current.hubId === hubId && !state.current.channelId && usersList && userCount) {
      const roster = sel.membersInHub(hubId);
      renderUsers(usersList, userCount, roster);
      membersSidebar?.classList.remove('hidden');
    }
  });

  ws.on('member_offline', (msg = {}) => {
    const hubId = msg.hub_id;
    const userId = msg.user_id;
    const displayName = msg.display_name || msg.handle;
    if (!hubId || !userId) return;
    actions.updateHubMemberPresence(hubId, userId, false, displayName);
    if (state.current.hubId === hubId && !state.current.channelId && usersList && userCount) {
      const roster = sel.membersInHub(hubId);
      renderUsers(usersList, userCount, roster);
    }
  });

  ws.on('member_left', (msg = {}) => {
    const hubId = msg.hub_id;
    const userId = msg.user_id;
    if (!hubId || !userId) return;
    actions.updateHubMemberPresence(hubId, userId, false);
    if (state.current.hubId === hubId && usersList && userCount) {
      const roster = sel.membersInHub(hubId);
      renderUsers(usersList, userCount, roster);
    }
  });

  // Messages from server (just pass to your existing messageController via store)
  ws.on('message', (msg) => {
    // expect: {type:'message', channel_id, sender, content, sent_at}
    const ch = msg?.channel_id;
    if (!ch) return;
    actions.pushMessage({
      channel_id: ch,
      sender: msg.sender || msg.sender_display || 'Member',
      content: msg.content ?? '',
      sent_at: msg.sent_at || new Date().toISOString()
    });
    document.dispatchEvent(new CustomEvent('messages:updated', { detail: { channel_id: ch } }));
  });

  ws.on('profile_updated', (msg = {}) => {
    // If the updated user is self, update self profile and local rosters
    if (msg.user_id === state.self.publicId) {
      const prev = state.self.displayName || state.self.username || state.self.fullName || '';
      actions.updateSelfProfile({
        username: typeof msg.username === 'string' ? msg.username : undefined,
        full_name: typeof msg.full_name === 'string' ? msg.full_name : undefined,
        display_name: msg.display_name
      });
      actions.updateUserDisplay(msg.user_id, state.self.displayName, prev);
      document.dispatchEvent(new CustomEvent('profile:update:success', { detail: msg }));
      if (state.current.hubId && usersList && userCount) {
        const roster = sel.membersInHub(state.current.hubId);
        renderUsers(usersList, userCount, roster);
      }
      if (state.current.channelId) {
        document.dispatchEvent(new CustomEvent('messages:updated', { detail: { channel_id: state.current.channelId } }));
      }
    } else if (msg.user_id) {
      // Update hub rosters for other users (scope to hub if provided)
      const displayName = msg.display_name || msg.username || msg.full_name || '';
      const findExisting = () => {
        if (msg.hub_id) {
          const arr = state.membersByHub[msg.hub_id] || [];
          const m = arr.find((x) => x.user_id === msg.user_id);
          if (m) return m.display_name || m.handle || '';
        }
        for (const members of Object.values(state.membersByHub || {})) {
          const m = members.find((x) => x.user_id === msg.user_id);
          if (m) return m.display_name || m.handle || '';
        }
        return '';
      };
      const oldDisplay = findExisting();
      if (msg.hub_id) {
        actions.updateHubMemberPresence(msg.hub_id, msg.user_id, true, displayName);
      } else {
        Object.keys(state.membersByHub || {}).forEach((hubId) => {
          actions.updateHubMemberPresence(hubId, msg.user_id, true, displayName);
        });
      }
      // Update channel rosters
      Object.keys(state.usersByChannel || {}).forEach((channelId) => {
        actions.upsertPresence({
          channel_id: channelId,
          handle: displayName,
          display_name: displayName,
          online: true,
          user_id: msg.user_id
        });
      });
      actions.updateUserDisplay(msg.user_id, displayName, oldDisplay);
      if (state.current.hubId && usersList && userCount) {
        const roster = sel.membersInHub(state.current.hubId);
        renderUsers(usersList, userCount, roster);
      }
      if (state.current.channelId) {
        document.dispatchEvent(new CustomEvent('messages:updated', { detail: { channel_id: state.current.channelId } }));
      }
    }
  });

  ws.on('hub_created', (msg = {}) => {
    const hub = msg.hub;
    if (!hub || !hub.id) return;
    actions.addHub(hub);
    const channels = Array.isArray(msg.channels) ? msg.channels : [];
    const members = Array.isArray(msg.members) ? msg.members : [];
    actions.updateHubChannels(hub.id, channels);
    actions.setHubMembers(hub.id, members);
    actions.setCurrentHub(hub.id);
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hub } }));
  });

  ws.on('channel_deleted', (msg = {}) => {
    const hubId = msg.hub_id;
    const channelId = msg.channel_id;
    if (!hubId || !channelId) return;
    actions.removeChannel(hubId, channelId);
    document.dispatchEvent(new CustomEvent('channel:deleted', { detail: { hubId, channelId } }));
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId } }));
  });

  ws.on('hub_invite', (msg = {}) => {
    const hubId = msg.hub_id || msg.hubId;
    const code = msg.invite_code || msg.code || msg.inviteCode;
    if (!hubId || !code) return;
    document.dispatchEvent(new CustomEvent('hub:invite', { detail: { hubId, inviteCode: code } }));
  });

  ws.on('hub_joined', (msg = {}) => {
    const hub = msg.hub;
    if (!hub || !hub.id) return;
    const channels = Array.isArray(msg.channels) ? msg.channels : [];
    const members = Array.isArray(msg.members) ? msg.members : [];
    actions.addHub(hub);
    actions.updateHubChannels(hub.id, channels);
    actions.setHubMembers(hub.id, members);
    actions.setCurrentHub(hub.id);
    document.dispatchEvent(new CustomEvent('hub:join:success', { detail: msg }));
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId: hub.id } }));
  });

  ws.on('channel_renamed', (msg = {}) => {
    const hubId = msg.hub_id;
    const channel = msg.channel;
    if (!hubId || !channel || !channel.id) return;
    actions.renameChannel(hubId, channel.id, channel.name || '');
    document.dispatchEvent(new CustomEvent('channel:renamed', { detail: { hubId, channel } }));
    if (state.current.hubId === hubId) {
      const list = sel.channels(hubId);
      if (channelsList) {
        const role = sel.currentHubRole();
        const canDelete = role === 'owner' || role === 'admin';
        renderChannels(channelsList, list, sel.currentChannelId(), { canDelete });
      }
      if (state.current.channelId === channel.id) {
        state.current.channelName = channel.name || state.current.channelName;
        if (currentHubName) currentHubName.textContent = (state.hubs.find((h) => h.id === hubId)?.name) || 'Select Hub';
      }
    }
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId } }));
  });

  ws.on('channel_closed', (msg = {}) => {
    const { channel_id } = msg;
    if (!channel_id) return;
    if (state.current.channelId === channel_id) {
      state.current.channelId = null;
      state.current.channelName = '';
      if (messagesWrap) messagesWrap.classList.add('hidden');
      if (inputArea) inputArea.classList.add('hidden');
      if (chatEmptyState) {
        const text = chatEmptyState.querySelector('p');
        if (text) text.textContent = 'This channel has been deleted.';
        chatEmptyState.classList.remove('hidden');
      }
    }
  });

  ws.on('member_role_updated', (msg = {}) => {
    const hubId = msg.hub_id;
    const role = msg.role;
    if (!hubId || !role) return;

    if (!msg.user_id) {
      actions.updateHubRole(hubId, role);
      document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId } }));
    }
  });

  ws.on('hub_renamed', (msg = {}) => {
    const hubId = msg.hub_id;
    const name = msg.name;
    if (!hubId || typeof name !== 'string') return;
    actions.renameHub(hubId, name);
    document.dispatchEvent(new CustomEvent('hub:renamed', { detail: { hubId, name } }));
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId } }));
    if (state.current.hubId === hubId && currentHubName) {
      currentHubName.textContent = name;
    }
  });

  ws.on('hub_deleted', (msg = {}) => {
    const hubId = msg.hub_id;
    if (!hubId) return;
    const nextHubId = actions.removeHub(hubId);
    if (nextHubId) {
      actions.setCurrentHub(nextHubId);
    } else {
      actions.setCurrentHub(null);
    }
    document.dispatchEvent(new CustomEvent('hub:deleted', { detail: { hubId } }));
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId: nextHubId } }));
  });

  ws.on('hub_left', (msg = {}) => {
    const hubId = msg.hub_id;
    if (!hubId) return;
    const nextHubId = actions.removeHub(hubId);
    if (nextHubId) {
      actions.setCurrentHub(nextHubId);
    } else {
      actions.setCurrentHub(null);
    }
    document.dispatchEvent(new CustomEvent('hub:left:success', { detail: { hubId } }));
    document.dispatchEvent(new CustomEvent('hubs:changed', { detail: { hubId: nextHubId } }));
  });

  ws.on('error', (msg = {}) => {
    if (msg.code === 'update_failed' || msg.code === 'invalid_profile') {
      document.dispatchEvent(new CustomEvent('profile:update:error', { detail: msg }));
    }
    document.dispatchEvent(new CustomEvent('ws:error', { detail: msg }));
    if (
      msg.code === 'invite_not_found' ||
      msg.code === 'invalid_invite' ||
      msg.code === 'hub_not_found' ||
      msg.code === 'join_hub_failed'
    ) {
      document.dispatchEvent(new CustomEvent('hub:join:error', { detail: msg }));
    }
    if (
      msg.code === 'insufficient_privilege' ||
      msg.code === 'not_in_hub' ||
      msg.code === 'missing_hub_id'
    ) {
      document.dispatchEvent(new CustomEvent('hub:invite:error', { detail: msg }));
    }
    if (
      msg.code === 'leave_hub_failed' ||
      msg.code === 'hub_owner_must_transfer' ||
      msg.code === 'not_in_hub'
    ) {
      document.dispatchEvent(new CustomEvent('hub:left:error', { detail: msg }));
    }
  });
}
