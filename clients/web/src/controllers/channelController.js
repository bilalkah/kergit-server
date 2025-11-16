// src/controllers/channelController.js
import {
  joinChannel,
  reqUsersForChannel,
  deleteChannel,
  updateMemberRole,
  renameChannel
} from '../api/chatApi.js';
import { state, actions } from '../store/state.js';
import { renderUsers, renderHistory } from '../views/chat.js';
import { renderChannels } from '../views/sidebar.js';
import { sel } from '../store/selectors.js';

export function wireChannel({ ws, els }) {
  const {
    channelsList,
    usersList,
    userCount,
    messagesWrap,
    messages,
    messageInput,
    sendBtn,
    chatEmptyState,
    inputArea,
    membersSidebar,
    confirmModal,
    confirmModalTitle,
    confirmModalMessage,
    confirmModalPrimary,
    confirmModalSecondary,
    channelSettingsModal,
    channelSettingsName,
    channelSettingsSave,
    channelSettingsDelete,
    channelSettingsError,
    memberRoleModal,
    memberRoleTitle,
    memberRoleMessage,
    memberRoleMakeAdmin,
    memberRoleMakeMember,
    memberRoleError
  } = els;

  if (!channelsList) return;

  let pendingChannelDelete = null;
  let pendingRoleUser = null;
  let channelSettingsCtx = { hubId: null, channelId: null, originalName: '' };
  let channelSettingsBusy = false;

  const clearChannelMenuState = () => {
    if (!channelsList) return;
    channelsList.querySelectorAll('.channel.channel-menu-open').forEach((node) => {
      node.classList.remove('channel-menu-open');
    });
  };

  const showChannelSettingsError = (message) => {
    if (!channelSettingsError) return;
    if (!message) {
      channelSettingsError.textContent = '';
      channelSettingsError.classList.add('hidden');
    } else {
      channelSettingsError.textContent = message;
      channelSettingsError.classList.remove('hidden');
    }
  };

  const setChannelSettingsBusy = (busy) => {
    channelSettingsBusy = busy;
    if (channelSettingsSave) channelSettingsSave.disabled = busy;
    if (channelSettingsDelete) channelSettingsDelete.disabled = busy;
  };

  const closeChannelSettingsModal = () => {
    if (!channelSettingsModal) return;
    channelSettingsModal.classList.add('hidden');
    channelSettingsCtx = { hubId: null, channelId: null, originalName: '' };
    showChannelSettingsError('');
    setChannelSettingsBusy(false);
    if (channelSettingsName) channelSettingsName.value = '';
    clearChannelMenuState();
  };

  const openChannelSettingsModal = (channelId, channelName) => {
    if (!channelSettingsModal) return;
    const role = sel.currentHubRole();
    if (!(role === 'owner' || role === 'admin')) return;
    const hubId = state.current.hubId;
    channelSettingsCtx = { hubId, channelId, originalName: channelName || '' };
    showChannelSettingsError('');
    setChannelSettingsBusy(false);
    if (channelSettingsName) channelSettingsName.value = channelName || '';
    channelSettingsModal.classList.remove('hidden');
    queueMicrotask(() => channelSettingsName?.focus());
    clearChannelMenuState();
    if (channelsList) {
      const node = channelsList.querySelector(`[data-channel-id="${channelId}"]`);
      if (node) node.classList.add('channel-menu-open');
    }
  };

  const closeConfirmModal = () => {
    if (!confirmModal) return;
    pendingChannelDelete = null;
    confirmModal.classList.add('hidden');
    if (confirmModalPrimary) {
      confirmModalPrimary.disabled = false;
      confirmModalPrimary.classList.remove('danger');
      confirmModalPrimary.textContent = 'Confirm';
      confirmModalPrimary.dataset.intent = '';
    }
  };

  const closeRoleModal = () => {
    if (!memberRoleModal) return;
    pendingRoleUser = null;
    memberRoleModal.classList.add('hidden');
    if (memberRoleError) {
      memberRoleError.textContent = '';
      memberRoleError.classList.add('hidden');
    }
  };

  const openRoleModal = (userId, displayName) => {
    if (!memberRoleModal || !memberRoleMakeAdmin || !memberRoleMakeMember) {
      updateMemberRole(ws, { hub_id: state.current.hubId, user_id: userId, role: 'admin' });
      return;
    }
    pendingRoleUser = userId;
    if (memberRoleTitle) memberRoleTitle.textContent = 'Update Member Role';
    if (memberRoleMessage) {
      memberRoleMessage.textContent =
        `Choose a role for ${displayName || 'this member'}.`;
    }
    memberRoleModal.classList.remove('hidden');
    queueMicrotask(() => memberRoleMakeAdmin?.focus());
  };

  confirmModalPrimary?.addEventListener('click', () => {
    if (confirmModalPrimary?.dataset?.intent === 'delete-channel') {
      if (!pendingChannelDelete) return;
      deleteChannel(ws, pendingChannelDelete.channelId);
      closeConfirmModal();
      return;
    }
  });

  confirmModalSecondary?.addEventListener('click', (e) => {
    e.preventDefault();
    closeConfirmModal();
  });

  confirmModal?.addEventListener('click', (e) => {
    if (e.target === confirmModal || e.target?.dataset?.close === 'confirm-modal') {
      closeConfirmModal();
    }
  });

  memberRoleMakeAdmin?.addEventListener('click', (e) => {
    e.preventDefault();
    if (!pendingRoleUser) return;
    updateMemberRole(ws, { hub_id: state.current.hubId, user_id: pendingRoleUser, role: 'admin' });
    closeRoleModal();
  });

  memberRoleMakeMember?.addEventListener('click', (e) => {
    e.preventDefault();
    if (!pendingRoleUser) return;
    updateMemberRole(ws, { hub_id: state.current.hubId, user_id: pendingRoleUser, role: 'member' });
    closeRoleModal();
  });

  memberRoleModal?.addEventListener('click', (e) => {
    if (e.target === memberRoleModal || e.target?.dataset?.close === 'member-role-modal') {
      closeRoleModal();
    }
  });

  channelSettingsSave?.addEventListener('click', (e) => {
    e.preventDefault();
    if (!channelSettingsModal || !channelSettingsCtx.channelId) return;
    if (channelSettingsBusy) return;
    const nameRaw = channelSettingsName?.value ?? '';
    const name = nameRaw.trim();
    if (!name) {
      showChannelSettingsError('Provide a channel name.');
      return;
    }
    if (name === channelSettingsCtx.originalName) {
      closeChannelSettingsModal();
      return;
    }
    showChannelSettingsError('');
    setChannelSettingsBusy(true);
    renameChannel(ws, channelSettingsCtx.channelId, name);
  });

  const requestChannelDeletion = () => {
    if (!channelSettingsCtx.channelId) return;
    if (!confirmModal || !confirmModalPrimary || !confirmModalMessage) {
      deleteChannel(ws, channelSettingsCtx.channelId);
      closeChannelSettingsModal();
      return;
    }
    const effectiveName = (channelSettingsName?.value?.trim() || channelSettingsCtx.originalName || '').trim();
    pendingChannelDelete = {
      channelId: channelSettingsCtx.channelId,
      hubId: channelSettingsCtx.hubId,
      channelName: effectiveName
    };
    if (confirmModalTitle) confirmModalTitle.textContent = 'Delete Channel';
    if (effectiveName) {
      confirmModalMessage.textContent =
        `Delete channel "${effectiveName}"? This action cannot be undone.`;
    } else {
      confirmModalMessage.textContent =
        'Delete this channel? This action cannot be undone.';
    }
    confirmModalPrimary.textContent = 'Delete';
    confirmModalPrimary.classList.add('danger');
    confirmModalPrimary.dataset.intent = 'delete-channel';
    confirmModalPrimary.disabled = false;
    confirmModal.classList.remove('hidden');
    queueMicrotask(() => confirmModalPrimary?.focus());
  };

  channelSettingsDelete?.addEventListener('click', (e) => {
    e.preventDefault();
    requestChannelDeletion();
  });

  channelSettingsModal?.addEventListener('click', (e) => {
    if (e.target === channelSettingsModal || e.target?.dataset?.close === 'channel-settings-modal') {
      closeChannelSettingsModal();
    }
  });

  channelSettingsName?.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      channelSettingsSave?.click();
    } else if (e.key === 'Escape') {
      closeChannelSettingsModal();
    }
  });

  if (channelsList) {
    channelsList.addEventListener('click', (e) => {
      const settingsBtn = e.target.closest('[data-action="channel-settings"]');
      if (settingsBtn) {
        e.preventDefault();
        e.stopPropagation();
        const channelId = settingsBtn.dataset.channelId;
        if (!channelId) return;
        const node = settingsBtn.closest('[data-channel-id]');
        const channelName = node?.dataset?.channelName || '';
        openChannelSettingsModal(channelId, channelName);
        return;
      }

      const node = e.target.closest('[data-channel-id]'); if (!node) return;
      const cid = node.dataset.channelId, cname = node.dataset.channelName || '';
      clearChannelMenuState();
      channelsList.querySelectorAll('.channel.active').forEach(el => el.classList.remove('active'));
      node.classList.add('active');
      if (messagesWrap) messagesWrap.classList.add('hidden');
      if (chatEmptyState) {
        const text = chatEmptyState.querySelector('p');
        if (text) text.textContent = 'Joining channel…';
        chatEmptyState.classList.remove('hidden');
      }
      inputArea?.classList.add('hidden');
      joinChannel(ws, cid);
      reqUsersForChannel(ws, cid);
    });
  }

  document.addEventListener('channel:renamed', (ev) => {
    const detail = ev.detail || {};
    const { channel } = detail;
    if (!channelSettingsCtx.channelId || !channel) return;
    if (channel.id !== channelSettingsCtx.channelId) return;
    channelSettingsCtx.originalName = channel.name || channelSettingsCtx.originalName;
    closeChannelSettingsModal();
  });

  document.addEventListener('channel:deleted', (ev) => {
    const detail = ev.detail || {};
    if (!channelSettingsCtx.channelId) return;
    if (detail.channelId === channelSettingsCtx.channelId) {
      closeChannelSettingsModal();
    }
  });

  ws.on('error', (msg = {}) => {
    if (!msg?.code) return;
    if (msg.code === 'rename_channel_failed' || msg.code === 'invalid_channel_name') {
      if (channelSettingsCtx.channelId) {
        const message =
          msg.message || msg.error_message || 'Unable to rename channel at this time.';
        showChannelSettingsError(message);
        setChannelSettingsBusy(false);
      }
    }
  });

  channelsList.addEventListener('contextmenu', (e) => {
    const node = e.target.closest('[data-channel-id]');
    if (!node) return;
    e.preventDefault();
    const channelId = node.dataset.channelId;
    const channelName = node.dataset.channelName || '';
    openChannelSettingsModal(channelId, channelName);
  });

  ws.on('joined_channel', (msg) => {
    actions.setJoinedChannel(msg);
    const roster = sel.usersInChannel(msg.channel_id) || [];
    renderUsers(usersList, userCount, roster);
    renderHistory(messagesWrap, messages, sel.messagesInChannel(msg.channel_id));
    const hubId = state.current.hubId;
    if (hubId) {
      const role = sel.currentHubRole();
      const canDelete = role === 'owner' || role === 'admin';
      renderChannels(channelsList, sel.channels(hubId), sel.currentChannelId(), { canDelete });
    }
    // Enable composer
    if (messageInput) messageInput.disabled = false;
    if (sendBtn) sendBtn.disabled = false;
    if (inputArea) inputArea.classList.remove('hidden');
    if (messagesWrap) messagesWrap.classList.remove('hidden');
    if (chatEmptyState) chatEmptyState.classList.add('hidden');
    membersSidebar?.classList.remove('hidden');
  });

  ws.on('user_joined', (m) => {
    actions.upsertPresence({ ...m, online: true });
  });
  ws.on('user_left', (m) => {
    actions.upsertPresence({ ...m, online: false });
  });

  if (usersList) {
    usersList.addEventListener('click', (e) => {
      const node = e.target.closest('.user');
      if (!node) return;
      const role = sel.currentHubRole();
      if (role !== 'owner') return;
      const userId = node.dataset.userId;
      if (!userId) return;
      if (userId === state.self.publicId) return;
      const hubId = state.current.hubId;
      if (!hubId) return;
      const displayName = node.textContent?.trim() || '';
      openRoleModal(userId, displayName);
    });
  }
}
