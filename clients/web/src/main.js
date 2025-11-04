// src/main.js
import { WSClient } from './services/ws.js';
import { wireAuth } from './controllers/authController.js';
import { wireList } from './controllers/listController.js';
import { wireChannel } from './controllers/channelController.js';
import { wireMessaging } from './controllers/messageController.js';
import { renderStatus } from './views/status.js';
import { renderHubs, renderChannels } from './views/sidebar.js';
import { state, actions } from './store/state.js';
import { CONFIG } from './config.js';
import { wireRealtime } from './controllers/realtime.js';
import { sel } from './store/selectors.js';

const qs = (s) => document.querySelector(s);

const els = {
  // screens
  loginScreen: qs('#login-screen'),
  chatScreen: qs('#chat-interface'),

  // auth form
  loginForm: qs('#login-form'),
  email: qs('#email'),
  password: qs('#password'),
  username: qs('#username'),
  serverUrl: qs('#server-url'),
  submitBtn: qs('#auth-submit-btn'),
  submitText: qs('#auth-submit-text'),
  authError: qs('#auth-error'),

  // status + user
  statusDot: qs('#connection-status'),
  statusText: qs('#status-text'),
  pingDisplay: qs('#ping-display'),
  currentUser: qs('#current-user'),

  // layout
  hubRailList: qs('#hub-rail-list'),
  channelSidebar: qs('.channel-sidebar'),
  channelsSection: qs('#channels-section'),
  membersSidebar: qs('#members-sidebar'),
  channelEmptyState: qs('#channel-empty-state'),
  chatEmptyState: qs('#chat-empty-state'),

  // channels
  channelsList: qs('#channels-list'),

  // users & chat
  usersList: qs('#users-list'),
  userCount: qs('#user-count'),
  messagesWrap: qs('#messages-container'),
  messages: qs('#messages'),
  messageInput: qs('#message-input'),
  sendBtn: qs('#send-btn'),

  // header
  currentHubName: qs('#current-hub-name'),
  disconnectBtn: qs('#disconnect-btn'),

  // misc
  refreshChannelsBtn: qs('#refresh-channels-btn'),
  refreshUsersBtn: qs('#refresh-users-btn'),
  inputArea: qs('.input-area'),

  // connection lost modal
  connectionLostModal: qs('#connection-lost-modal'),
  connectionLostMessage: qs('#connection-lost-message'),
  connectionLostOk: qs('#connection-lost-ok')
};

const config = CONFIG || {};
const ws = new WSClient();

function bindStatus() {
  const paint = () => renderStatus(els.statusDot, els.statusText, state.connection);
  const orig = actions.setConnection;
  actions.setConnection = (s) => { orig(s); paint(); };
  paint();
}

function renderHubRail() {
  const hubs = state.hubs || [];
  renderHubs(els.hubRailList, hubs, state.current.hubId);
  if (!els.hubRailList) return;
  els.hubRailList.querySelectorAll('.hub-icon').forEach(btn => {
    btn.addEventListener('click', () => handleHubSelection(btn.dataset.hubId));
  });
}

function showNoHubState() {
  if (els.channelEmptyState) {
    els.channelEmptyState.classList.remove('hidden');
    const text = els.channelEmptyState.querySelector('p');
    if (text) text.textContent = 'Create or join a hub to view channels.';
  }
  if (els.channelsList) els.channelsList.innerHTML = '';
  els.channelsSection?.classList.add('hidden');
  if (els.membersSidebar) els.membersSidebar.classList.add('hidden');
  if (els.usersList) els.usersList.innerHTML = '';
  if (els.userCount) els.userCount.textContent = '0';
  if (els.messagesWrap) els.messagesWrap.classList.add('hidden');
  if (els.chatEmptyState) {
    const text = els.chatEmptyState.querySelector('p');
    if (text) text.textContent = 'Create or join a hub to start chatting.';
    els.chatEmptyState.classList.remove('hidden');
  }
  els.inputArea?.classList.add('hidden');
  els.currentHubName.textContent = 'Select Hub';
}

function handleHubSelection(hubId) {
  if (!hubId) return;
  if (state.current.hubId === hubId) return;
  actions.setCurrentHub(hubId);
  updateHubUI();
}

function updateHubUI() {
  const hubs = state.hubs || [];
  const currentHub = hubs.find(h => h.id === state.current.hubId);

  renderHubRail();

  if (!currentHub) {
    if (hubs.length === 0) showNoHubState();
    return;
  }

  els.currentHubName.textContent = currentHub.name || 'Untitled Hub';

  const channels = sel.channels(currentHub.id);
  if (channels.length) {
    if (els.channelEmptyState) els.channelEmptyState.classList.add('hidden');
    els.channelsSection?.classList.remove('hidden');
    renderChannels(els.channelsList, channels, state.current.channelId);
  } else {
    els.channelsList.innerHTML = '';
    els.channelsSection?.classList.add('hidden');
    if (els.channelEmptyState) {
      const text = els.channelEmptyState.querySelector('p');
      if (text) text.textContent = 'No channels yet. Create one to start the conversation.';
      els.channelEmptyState.classList.remove('hidden');
    }
  }

  if (els.membersSidebar) els.membersSidebar.classList.remove('hidden');
  const members = sel.membersInHub(currentHub.id);
  if (els.usersList && els.userCount) {
    renderUsers(els.usersList, els.userCount, members);
  }

  if (!state.current.channelId) {
    if (els.messagesWrap) els.messagesWrap.classList.add('hidden');
    if (els.chatEmptyState) {
      const text = els.chatEmptyState.querySelector('p');
      if (text) text.textContent = 'Select a channel to start chatting.';
      els.chatEmptyState.classList.remove('hidden');
    }
    els.inputArea?.classList.add('hidden');
  }

  document.dispatchEvent(new CustomEvent('hub:selected', { detail: currentHub }));
}

function start() {
  bindStatus();
  if (els.pingDisplay) {
    els.pingDisplay.textContent = 'Ping: --';
    els.pingDisplay.classList.add('is-idle');
  }
  wireRealtime({ ws, els });
  wireAuth({ ws, els, config });
  wireList({ ws, els });
  wireChannel({ ws, els });
  wireMessaging({ ws, els });

  // ---- Decorate setList so hubs from server update UI ----
  const origSetList = actions.setList;
  actions.setList = (payload) => {
    origSetList(payload);
    const hubs = state.hubs || [];
    if (!hubs.length) {
      renderHubRail();
      showNoHubState();
      return;
    }
    const hasCurrent = hubs.some(h => h.id === state.current.hubId);
    if (!hasCurrent) {
      actions.setCurrentHub(hubs[0].id);
    }
    updateHubUI();
  };
  // --------------------------------------------------------

  renderHubRail();
  showNoHubState();

  console.log('[BOOT] ready');
}

document.addEventListener('DOMContentLoaded', start);
