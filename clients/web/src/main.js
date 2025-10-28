// src/main.js
import { WSClient } from './services/ws.js';
import { wireAuth } from './controllers/authController.js';
import { wireList } from './controllers/listController.js';
import { wireChannel } from './controllers/channelController.js';
import { wireMessaging } from './controllers/messageController.js';
import { renderStatus } from './views/status.js';
import { state, actions } from './store/state.js';
import { CONFIG } from './config.js';
import { wireRealtime } from './controllers/realtime.js';

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
  currentUser: qs('#current-user'),

  // sidebars
  channelsSidebar: qs('.left-sidebar'),
  usersSidebar: qs('#users-sidebar'),

  // hub list inline
  hubListContainer: qs('#hub-list-container'),
  hubsListEl: qs('#hubs-list'),

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
  hubSelector: qs('#hub-selector'),
  currentHubName: qs('#current-hub-name'),
  disconnectBtn: qs('#disconnect-btn'),

  // misc
  refreshChannelsBtn: qs('#refresh-channels-btn'),
  refreshUsersBtn: qs('#refresh-users-btn'),
  inputArea: qs('.input-area')
};

const config = CONFIG || {};
const ws = new WSClient();

function bindStatus() {
  const paint = () => renderStatus(els.statusDot, els.statusText, state.connection);
  const orig = actions.setConnection;
  actions.setConnection = (s) => { orig(s); paint(); };
  paint();
}

// render hub list inline (center of page)
function renderHubList() {
  const hubs = state.hubs || [];
  els.hubsListEl.innerHTML = '';

  hubs.forEach(h => {
    const item = document.createElement('div');
    item.className = 'hub-item';
    item.textContent = h.name;
    item.addEventListener('click', () => {
      actions.setCurrentHub(h.id);
      els.currentHubName.textContent = h.name;
      els.hubListContainer.classList.add('hidden');
      els.channelsSidebar.classList.remove('hidden');
      els.usersSidebar.classList.remove('hidden');
      els.messagesWrap.classList.remove('hidden');
      els.inputArea.classList.remove('hidden');
      document.dispatchEvent(new CustomEvent('hub:selected', { detail: h }));
    });
    els.hubsListEl.appendChild(item);
  });

  // show if no hub selected
  if (!state.current.hubId) {
    els.hubListContainer.classList.remove('hidden');
    els.channelsSidebar.classList.add('hidden');
    els.usersSidebar.classList.add('hidden');
    els.messagesWrap.classList.add('hidden');
    els.inputArea.classList.add('hidden');
  }
}

function start() {
  bindStatus();
  wireRealtime({ ws, els });
  wireAuth({ ws, els, config });
  wireList({ ws, els });
  wireChannel({ ws, els });
  wireMessaging({ ws, els });

  // ---- Decorate setList so hubs from server update UI ----
  const origSetList = actions.setList;
  actions.setList = (payload) => {
    origSetList(payload);

    // if hubs received, render inline hub list
    if (state.hubs?.length > 0) {
      renderHubList();
    }

    // default hub selection
    if (!state.current.hubId && state.hubs.length > 0) {
      els.hubListContainer.classList.remove('hidden');
      els.channelsSidebar.classList.add('hidden');
      els.usersSidebar.classList.add('hidden');
      els.messagesWrap.classList.add('hidden');
      els.inputArea.classList.add('hidden');
    }
  };
  // --------------------------------------------------------

  console.log('[BOOT] ready');
}

document.addEventListener('DOMContentLoaded', start);
