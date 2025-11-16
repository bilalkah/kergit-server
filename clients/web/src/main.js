// src/main.js
import { WSClient } from './services/ws.js';
import { logout as supabaseLogout } from './services/supabase.js';
import { wireAuth } from './controllers/authController.js';
import { wireList } from './controllers/listController.js';
import { wireChannel } from './controllers/channelController.js';
import { wireMessaging } from './controllers/messageController.js';
import { renderStatus } from './views/status.js';
import { state, actions } from './store/state.js';
import { CONFIG } from './config.js';
import { wireRealtime } from './controllers/realtime.js';
import { createHubController } from './controllers/hubController.js';
import { createHubManagementController } from './controllers/hubManagementController.js';
import { createProfileController } from './controllers/profileController.js';

const qs = (s) => document.querySelector(s);
const qsa = (s) => Array.from(document.querySelectorAll(s));

const els = {
  // screens
  loginScreen: qs('#login-screen'),
  chatScreen: qs('#chat-interface'),

  // auth form
  loginForm: qs('#login-form'),
  email: qs('#email'),
  password: qs('#password'),
  username: qs('#username'),
  fullName: qs('#full-name'),
  serverUrl: qs('#server-url'),
  signupFields: qsa('.auth-signup-fields'),
  switchToSignup: qs('#switch-to-signup'),
  switchToSignin: qs('#switch-to-signin'),
  toggleToSignup: qs('#toggle-to-signup'),
  toggleToSignin: qs('#toggle-to-signin'),
  submitBtn: qs('#auth-submit-btn'),
  submitText: qs('#auth-submit-text'),
  authError: qs('#auth-error'),

  // status + user
  statusDot: qs('#connection-status'),
  statusText: qs('#status-text'),
  pingDisplay: qs('#ping-display'),
  currentUser: qs('#current-user'),
  userInfo: qs('.user-info'),
  profileModal: qs('#profile-modal'),
  profileModalClose: qs('#profile-modal .close-btn'),
  profileUsername: qs('#profile-username'),
  profileFullName: qs('#profile-full-name'),
  profileSave: qs('#profile-save'),
  profileCancel: qs('#profile-cancel'),
  profileError: qs('#profile-error'),

  // layout
  hubRailList: qs('#hub-rail-list'),
  createHubBtn: qs('#create-hub-btn'),
  channelSidebar: qs('.channel-sidebar'),
  channelsSection: qs('#channels-section'),
  membersSidebar: qs('#members-sidebar'),
  channelEmptyState: qs('#channel-empty-state'),
  chatEmptyState: qs('#chat-empty-state'),
  hubActions: qs('#hub-actions'),
  leaveHubBtn: qs('#leave-hub-btn'),
  leaveHubError: qs('#leave-hub-error'),

  // channels
  channelsList: qs('#channels-list'),
  createChannelBtn: qs('#create-channel-btn'),
  createChannelModal: qs('#create-channel-modal'),
  createChannelName: qs('#new-channel-name'),
  createChannelConfirm: qs('#create-channel-confirm'),
  createChannelCancel: qs('#create-channel-cancel'),
  createHubModal: qs('#create-hub-modal'),
  createHubName: qs('#new-hub-name'),
  createHubConfirm: qs('#create-hub-confirm'),
  createHubCancel: qs('#create-hub-cancel'),
  createHubError: qs('#hub-error'),

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
  inputArea: qs('.input-area'),

  // connection lost modal
  connectionLostModal: qs('#connection-lost-modal'),
  connectionLostMessage: qs('#connection-lost-message'),
  connectionLostOk: qs('#connection-lost-ok'),

  // generic confirm modal
  confirmModal: qs('#confirm-modal'),
  confirmModalTitle: qs('#confirm-modal-title'),
  confirmModalMessage: qs('#confirm-modal-message'),
  confirmModalPrimary: qs('#confirm-modal-primary'),
  confirmModalSecondary: qs('#confirm-modal-secondary'),
  channelSettingsModal: qs('#channel-settings-modal'),
  channelSettingsName: qs('#channel-settings-name'),
  channelSettingsSave: qs('#channel-settings-save'),
  channelSettingsDelete: qs('#channel-settings-delete'),
  channelSettingsError: qs('#channel-settings-error'),
  hubSettingsInviteCode: qs('#hub-settings-invite-code'),
  hubSettingsCopy: qs('#hub-settings-copy'),
  hubSettingsRefresh: qs('#hub-settings-refresh'),
  hubSettingsInviteError: qs('#hub-settings-invite-error'),

  // member role modal
  memberRoleModal: qs('#member-role-modal'),
  memberRoleTitle: qs('#member-role-title'),
  memberRoleMessage: qs('#member-role-message'),
  memberRoleMakeAdmin: qs('#member-role-make-admin'),
  memberRoleMakeMember: qs('#member-role-make-member'),
  memberRoleError: qs('#member-role-error'),

  // hub settings
  hubSettingsBtn: qs('#hub-settings-btn'),
  hubSettingsModal: qs('#hub-settings-modal'),
  hubSettingsName: qs('#hub-settings-name'),
  hubSettingsSave: qs('#hub-settings-save'),
  hubSettingsDelete: qs('#hub-settings-delete'),
  hubSettingsError: qs('#hub-settings-error'),

  // join hub
  openJoinHubBtn: qs('#open-join-hub-btn'),
  joinHubModal: qs('#join-hub-modal'),
  joinHubCodeInput: qs('#join-hub-code'),
  joinHubConfirm: qs('#join-hub-confirm'),
  joinHubCancel: qs('#join-hub-cancel'),
  joinHubError: qs('#join-hub-error')
};

const config = CONFIG || {};
const ws = new WSClient();
let hubController = null;
let hubManagementController = null;
let profileController = null;

function bindStatus() {
  const paint = () => renderStatus(els.statusDot, els.statusText, state.connection);
  const orig = actions.setConnection;
  actions.setConnection = (s) => { orig(s); paint(); };
  paint();
}


async function performLogout() {
  const btn = els.disconnectBtn;
  if (!btn) return;
  if (btn.disabled) return;
  btn.disabled = true;

  els.connectionLostModal?.classList.add('hidden');

  try {
    try {
      await supabaseLogout();
    } catch (err) {
      console.warn('[AUTH] logout warning:', err?.message || err);
    }

    try {
      ws.disconnect?.(1000, 'user logout');
    } catch (err) {
      console.warn('[WS] error during logout disconnect:', err);
    }

    actions.reset();
    actions.setAuth(false);
    actions.setSession(null);
    actions.setConnection('disconnected');
    actions.setHeartbeat({});
    actions.setHubPresenceMap({});
    profileController?.reset();
    hubManagementController?.reset();
    if (els.currentUser) els.currentUser.textContent = 'User';
    if (els.messages) els.messages.innerHTML = '';
    if (els.usersList) els.usersList.innerHTML = '';
    if (els.userCount) els.userCount.textContent = '0';
    if (els.messageInput) {
      els.messageInput.value = '';
      els.messageInput.disabled = true;
    }
    if (els.sendBtn) els.sendBtn.disabled = true;
    if (els.messagesWrap) els.messagesWrap.classList.add('hidden');
    if (els.inputArea) els.inputArea.classList.add('hidden');
    if (els.chatEmptyState) {
      const text = els.chatEmptyState.querySelector('p');
      if (text) text.textContent = 'Create or join a hub to start chatting.';
    }
    if (els.pingDisplay) {
      els.pingDisplay.textContent = 'Ping: --';
      els.pingDisplay.classList.remove('is-good', 'is-warn', 'is-bad');
      els.pingDisplay.classList.add('is-idle');
    }

    hubController?.reset();

    if (els.chatScreen) els.chatScreen.classList.add('hidden');
    if (els.loginScreen) els.loginScreen.classList.remove('hidden');
  } finally {
    // keep logout disabled until next successful login
  }
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

  if (els.disconnectBtn) {
    els.disconnectBtn.disabled = true;
    els.disconnectBtn.addEventListener('click', () => {
      performLogout().catch((err) => console.error('[LOGOUT] unexpected error:', err));
    });
  }

  hubManagementController = createHubManagementController({ ws, els });
  profileController = createProfileController({ ws, els });
  hubController = createHubController({
    els,
    onNoHubState: () => {
      hubManagementController?.reset();
      profileController?.reset();
    }
  });
  hubController.bind();

  // ---- Decorate setList so hubs from server update UI ----
  const origSetList = actions.setList;
  actions.setList = (payload) => {
    origSetList(payload);
    hubController?.handleHubListUpdate();
  };
  // --------------------------------------------------------

  hubController?.reset();

  console.log('[BOOT] ready');
}



document.addEventListener('DOMContentLoaded', start);
