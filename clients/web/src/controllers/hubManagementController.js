// src/controllers/hubManagementController.js
import {
  createChannel,
  createHub,
  deleteHub,
  joinHubByCode,
  leaveHub,
  renameHub,
  requestHubInvite
} from '../api/chatApi.js';
import { state } from '../store/state.js';

export function createHubManagementController({ ws, els }) {
  if (!ws || !els) throw new Error('createHubManagementController requires ws and els');

  const closers = [];
  const registerCloser = (fn) => {
    if (typeof fn === 'function') closers.push(fn);
  };

  const reset = () => {
    closers.forEach((close) => {
      try {
        close();
      } catch (err) {
        console.warn('[HubManagement] reset close failed', err);
      }
    });
  };

  wireCreateHub();
  wireCreateChannel();
  wireHubSettings();
  wireJoinHubByCode();
  wireLeaveHub();

  return { reset };

  function wireCreateHub() {
    const {
      createHubBtn,
      createHubModal,
      createHubName,
      createHubConfirm,
      createHubCancel,
      createHubError
    } = els;

    if (!createHubModal || !createHubName) return;

    const closeBtn = createHubModal.querySelector('.close-btn');

    const showError = (message) => {
      if (!createHubError) return;
      if (!message) {
        createHubError.textContent = '';
        createHubError.classList.add('hidden');
      } else {
        createHubError.textContent = message;
        createHubError.classList.remove('hidden');
      }
    };

    const openModal = () => {
      showError('');
      createHubName.value = '';
      createHubModal.classList.remove('hidden');
      queueMicrotask(() => createHubName?.focus());
    };

    const hideModal = () => {
      createHubModal.classList.add('hidden');
      showError('');
      if (createHubConfirm) createHubConfirm.disabled = false;
    };

    const handleCreate = () => {
      const name = (createHubName.value || '').trim();
      if (!name) {
        createHubName.focus();
        showError('Provide a hub name.');
        return;
      }
      if (createHubConfirm) createHubConfirm.disabled = true;
      createHub(ws, name);
    };

    createHubBtn?.addEventListener('click', (e) => {
      e.preventDefault();
      openModal();
    });
    createHubConfirm?.addEventListener('click', (e) => {
      e.preventDefault();
      handleCreate();
    });
    createHubCancel?.addEventListener('click', (e) => {
      e.preventDefault();
      hideModal();
    });
    closeBtn?.addEventListener('click', hideModal);
    createHubModal.addEventListener('click', (e) => {
      if (e.target === createHubModal) hideModal();
    });
    createHubName?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        handleCreate();
      } else if (e.key === 'Escape') {
        hideModal();
      }
    });

    ws.on('error', (msg = {}) => {
      const code = msg.code;
      if (code !== 'create_hub_failed' && code !== 'hub_limit_reached' && code !== 'invalid_name') {
        return;
      }
      const message =
        msg.message || msg.error_message || (code === 'hub_limit_reached'
          ? 'Hub ownership limit reached.'
          : 'Unable to create hub.');
      showError(message);
      if (createHubConfirm) createHubConfirm.disabled = false;
      if (createHubModal) {
        createHubModal.classList.remove('hidden');
        queueMicrotask(() => createHubName?.focus());
      }
    });

    registerCloser(hideModal);
  }

  function wireCreateChannel() {
    const {
      createChannelBtn,
      createChannelModal,
      createChannelName,
      createChannelConfirm,
      createChannelCancel
    } = els;
    if (!createChannelModal || !createChannelName) return;

    const closeBtn = createChannelModal.querySelector('.close-btn');

    const openModal = () => {
      if (createChannelModal.classList.contains('hidden')) {
        createChannelName.value = '';
        createChannelModal.classList.remove('hidden');
        queueMicrotask(() => createChannelName.focus());
      }
    };

    const hideModal = () => {
      createChannelModal.classList.add('hidden');
    };

    const handleCreate = () => {
      const hubId = state.current.hubId;
      if (!hubId) {
        hideModal();
        return;
      }
      const name = (createChannelName.value || '').trim();
      if (!name) {
        createChannelName.focus();
        return;
      }
      if (createChannelConfirm) createChannelConfirm.disabled = true;
      createChannel(ws, hubId, name);
      hideModal();
      if (createChannelConfirm) createChannelConfirm.disabled = false;
    };

    createChannelBtn?.addEventListener('click', () => {
      if (createChannelBtn.disabled) return;
      openModal();
    });
    createChannelConfirm?.addEventListener('click', (e) => {
      e.preventDefault();
      handleCreate();
    });
    createChannelCancel?.addEventListener('click', (e) => {
      e.preventDefault();
      hideModal();
    });
    closeBtn?.addEventListener('click', hideModal);
    createChannelModal.addEventListener('click', (e) => {
      if (e.target === createChannelModal) hideModal();
    });
    createChannelName?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        handleCreate();
      } else if (e.key === 'Escape') {
        hideModal();
      }
    });

    registerCloser(hideModal);
  }

  function wireHubSettings() {
    const {
      hubSettingsBtn,
      hubSettingsModal,
      hubSettingsName,
      hubSettingsSave,
      hubSettingsDelete,
      hubSettingsError,
      confirmModal,
      confirmModalTitle,
      confirmModalMessage,
      confirmModalPrimary,
      confirmModalSecondary,
      hubSettingsInviteCode,
      hubSettingsCopy,
      hubSettingsRefresh,
      hubSettingsInviteError
    } = els;

    if (!hubSettingsModal || !hubSettingsBtn) return;

    let hubContext = { hubId: null, originalName: '' };
    let pendingHubDelete = null;
    let invitePending = false;
    let busy = false;

    const showError = (msg) => {
      if (!hubSettingsError) return;
      if (!msg) {
        hubSettingsError.textContent = '';
        hubSettingsError.classList.add('hidden');
      } else {
        hubSettingsError.textContent = msg;
        hubSettingsError.classList.remove('hidden');
      }
    };

    const showInviteError = (msg) => {
      if (!hubSettingsInviteError) return;
      if (!msg) {
        hubSettingsInviteError.textContent = '';
        hubSettingsInviteError.classList.add('hidden');
      } else {
        hubSettingsInviteError.textContent = msg;
        hubSettingsInviteError.classList.remove('hidden');
      }
    };

    const setInviteCode = (code) => {
      if (!hubSettingsInviteCode) return;
      hubSettingsInviteCode.value = code || '';
      if (code) {
        hubSettingsInviteCode.classList.remove('placeholder');
      } else {
        hubSettingsInviteCode.classList.add('placeholder');
      }
    };

    const setBusy = (nextBusy) => {
      busy = nextBusy;
      if (hubSettingsSave) hubSettingsSave.disabled = busy;
      if (hubSettingsDelete) hubSettingsDelete.disabled = busy;
    };

    const setInviteBusy = (nextBusy) => {
      invitePending = nextBusy;
      if (hubSettingsCopy) hubSettingsCopy.disabled = invitePending;
      if (hubSettingsRefresh) hubSettingsRefresh.disabled = invitePending;
    };

    const closeConfirm = () => {
      if (!confirmModal) return;
      confirmModal.classList.add('hidden');
      if (confirmModalPrimary) {
        confirmModalPrimary.disabled = false;
        confirmModalPrimary.classList.remove('danger');
        confirmModalPrimary.textContent = 'Confirm';
        confirmModalPrimary.dataset.intent = '';
      }
    };

    const closeHubSettingsModal = () => {
      if (!hubSettingsModal) return;
      hubSettingsModal.classList.add('hidden');
      hubContext = { hubId: null, originalName: '' };
      setBusy(false);
      showError('');
      showInviteError('');
      setInviteBusy(false);
      setInviteCode('');
      closeConfirm();
    };

    const requestInviteCode = () => {
      if (!hubContext.hubId || invitePending) return;
      setInviteBusy(true);
      showInviteError('');
      setInviteCode('Fetching code…');
      requestHubInvite(ws, hubContext.hubId);
    };

    const openHubSettings = () => {
      if (!hubSettingsModal) return;
      const hubId = state.current.hubId;
      if (!hubId) return;
      const hub = (state.hubs || []).find((h) => h.id === hubId);
      hubContext = { hubId, originalName: hub?.name || '' };
      showError('');
      showInviteError('');
      setBusy(false);
      setInviteBusy(false);
      setInviteCode(hub?.invite_code || '');
      if (hubSettingsName) hubSettingsName.value = hub?.name || '';
      hubSettingsModal.classList.remove('hidden');
      queueMicrotask(() => hubSettingsName?.focus());
    };

    hubSettingsBtn.addEventListener('click', (e) => {
      e.preventDefault();
      openHubSettings();
    });

    hubSettingsSave?.addEventListener('click', (e) => {
      e.preventDefault();
      if (!hubContext.hubId || busy) return;
      const nameRaw = hubSettingsName?.value ?? '';
      const name = nameRaw.trim();
      if (!name) {
        showError('Provide a hub name.');
        hubSettingsName?.focus();
        return;
      }
      if (name === hubContext.originalName) {
        closeHubSettingsModal();
        return;
      }
      setBusy(true);
      showError('');
      renameHub(ws, hubContext.hubId, name);
    });

    hubSettingsDelete?.addEventListener('click', (e) => {
      e.preventDefault();
      if (!hubContext.hubId || !confirmModal || !confirmModalPrimary || !confirmModalMessage) return;
      pendingHubDelete = hubContext.hubId;
      const effectiveName = (hubSettingsName?.value?.trim() || hubContext.originalName || '').trim();
      if (confirmModalTitle) confirmModalTitle.textContent = 'Delete Hub';
      if (effectiveName) {
        confirmModalMessage.textContent =
          `Delete hub \"${effectiveName}\"? This action cannot be undone.`;
      } else {
        confirmModalMessage.textContent =
          'Delete this hub? This action cannot be undone.';
      }
      confirmModalPrimary.textContent = 'Delete';
      confirmModalPrimary.classList.add('danger');
      confirmModalPrimary.dataset.intent = 'delete-hub';
      confirmModalPrimary.disabled = false;
      confirmModal.classList.remove('hidden');
      queueMicrotask(() => confirmModalPrimary?.focus());
    });

    hubSettingsModal.addEventListener('click', (e) => {
      if (e.target === hubSettingsModal || e.target?.dataset?.close === 'hub-settings-modal') {
        closeHubSettingsModal();
      }
    });

    hubSettingsName?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        hubSettingsSave?.click();
      } else if (e.key === 'Escape') {
        closeHubSettingsModal();
      }
    });

    confirmModalPrimary?.addEventListener('click', () => {
      if (confirmModalPrimary.dataset.intent === 'delete-hub' && pendingHubDelete) {
        setBusy(true);
        showError('');
        deleteHub(ws, pendingHubDelete);
        pendingHubDelete = null;
        closeConfirm();
      }
    });

    confirmModalSecondary?.addEventListener('click', () => {
      if (confirmModalPrimary?.dataset?.intent === 'delete-hub') {
        pendingHubDelete = null;
      }
    });

    confirmModal?.addEventListener('click', (e) => {
      if (e.target === confirmModal || e.target?.dataset?.close === 'confirm-modal') {
        if (confirmModalPrimary?.dataset?.intent === 'delete-hub') {
          pendingHubDelete = null;
        }
      }
    });

    let copyReset;
    const showCopyFeedback = (ok) => {
      if (!hubSettingsCopy) return;
      hubSettingsCopy.textContent = ok ? 'Copied!' : 'Copy';
      clearTimeout(copyReset);
      copyReset = setTimeout(() => {
        hubSettingsCopy.textContent = 'Copy';
      }, 1200);
    };

    hubSettingsCopy?.addEventListener('click', async (e) => {
      e.preventDefault();
      const val = hubSettingsInviteCode?.value || '';
      if (!val) return;
      const ok = await (async () => {
        try {
          if (navigator?.clipboard?.writeText) {
            await navigator.clipboard.writeText(val);
            return true;
          }
        } catch (_) {
          /* ignore and fall back */
        }
        // Fallback 1: use the visible input
        if (hubSettingsInviteCode) {
          const wasReadOnly = hubSettingsInviteCode.hasAttribute('readonly');
          if (wasReadOnly) hubSettingsInviteCode.removeAttribute('readonly');
          hubSettingsInviteCode.focus();
          hubSettingsInviteCode.select();
          hubSettingsInviteCode.setSelectionRange(0, val.length);
          const ok = document.execCommand('copy');
          if (wasReadOnly) hubSettingsInviteCode.setAttribute('readonly', 'readonly');
          hubSettingsInviteCode.blur();
          if (ok) return true;
        }
        // Fallback 2: hidden textarea
        let tmp;
        try {
          tmp = document.createElement('textarea');
          tmp.value = val;
          tmp.setAttribute('readonly', '');
          tmp.style.position = 'absolute';
          tmp.style.left = '-9999px';
          document.body.appendChild(tmp);
          tmp.select();
          tmp.setSelectionRange(0, tmp.value.length);
          const ok = document.execCommand('copy');
          return ok;
        } catch (err) {
          console.warn('[HubSettings] copy failed', err);
          return false;
        } finally {
          if (tmp && tmp.parentNode) tmp.parentNode.removeChild(tmp);
          if (hubSettingsInviteCode) hubSettingsInviteCode.blur();
        }
      })();
      showCopyFeedback(ok);
      // Keep focus on modal input for convenience
      queueMicrotask(() => hubSettingsInviteCode?.focus());
    });

    hubSettingsRefresh?.addEventListener('click', (e) => {
      e.preventDefault();
      requestInviteCode();
    });

    document.addEventListener('hub:renamed', (ev) => {
      const detail = ev.detail || {};
      if (detail?.hubId === hubContext.hubId) {
        setBusy(false);
        closeHubSettingsModal();
      }
    });

    document.addEventListener('hub:deleted', (ev) => {
      const detail = ev.detail || {};
      if (detail?.hubId === hubContext.hubId) {
        pendingHubDelete = null;
        setBusy(false);
        closeHubSettingsModal();
      }
    });

    ws.on('error', (msg = {}) => {
      if (!msg?.code) return;
      if (msg.code === 'rename_hub_failed' || msg.code === 'invalid_hub_name') {
        if (hubContext.hubId) {
          const message = msg.message || msg.error_message || 'Unable to rename hub.';
          showError(message);
          setBusy(false);
        }
      }
      if (msg.code === 'delete_hub_failed') {
        pendingHubDelete = null;
        showError(msg.message || msg.error_message || 'Unable to delete hub.');
        setBusy(false);
      }
    });

    document.addEventListener('hub:invite', (ev) => {
      const detail = ev.detail || {};
      if (!hubContext.hubId || detail.hubId !== hubContext.hubId) return;
      invitePending = false;
      showInviteError('');
      setInviteCode(detail.inviteCode || detail.invite_code || '');
    });

    document.addEventListener('hub:invite:error', (ev) => {
      if (!hubSettingsModal || hubSettingsModal.classList.contains('hidden')) return;
      const detail = ev.detail || {};
      invitePending = false;
      setInviteCode('');
      showInviteError(detail.message || detail.error_message || 'Unable to fetch invite code.');
    });

    document.addEventListener('ws:error', (ev) => {
      const detail = ev.detail || {};
      if (!hubSettingsModal || hubSettingsModal.classList.contains('hidden')) return;
      const code = detail?.code;
      if (code === 'rename_hub_failed') {
        showError(detail.message || detail.error_message || 'Unable to rename hub.');
        setBusy(false);
      }
      if (code === 'delete_hub_failed') {
        pendingHubDelete = null;
        showError(detail.message || detail.error_message || 'Unable to delete hub.');
        setBusy(false);
      }
      if (!invitePending) return;
      if (code === 'insufficient_privilege' || code === 'not_in_hub' || code === 'hub_not_found' ||
          code === 'missing_hub_id') {
        invitePending = false;
        setInviteCode('');
        showInviteError(detail.message || detail.error_message || 'Unable to fetch invite code.');
      }
    });

    registerCloser(closeHubSettingsModal);
  }

  function wireJoinHubByCode() {
    const {
      openJoinHubBtn,
      joinHubModal,
      joinHubCodeInput,
      joinHubConfirm,
      joinHubCancel,
      joinHubError
    } = els;
    if (!openJoinHubBtn || !joinHubModal || !joinHubCodeInput) return;

    let joinBusy = false;

    const showError = (msg) => {
      if (!joinHubError) return;
      if (!msg) {
        joinHubError.textContent = '';
        joinHubError.classList.add('hidden');
      } else {
        joinHubError.textContent = msg;
        joinHubError.classList.remove('hidden');
      }
    };

    const setBusy = (busy) => {
      joinBusy = busy;
      if (joinHubConfirm) joinHubConfirm.disabled = busy;
    };

    const closeModal = () => {
      joinHubModal.classList.add('hidden');
      showError('');
      setBusy(false);
      joinHubCodeInput.value = '';
    };

    const openModal = () => {
      showError('');
      setBusy(false);
      joinHubCodeInput.value = '';
      joinHubModal.classList.remove('hidden');
      queueMicrotask(() => joinHubCodeInput.focus());
    };

    const submit = () => {
      if (joinBusy) return;
      const code = joinHubCodeInput.value.trim();
      if (!code) {
        showError('Enter an invite code.');
        joinHubCodeInput.focus();
        return;
      }
      showError('');
      setBusy(true);
      joinHubByCode(ws, code);
    };

    openJoinHubBtn.addEventListener('click', (e) => {
      e.preventDefault();
      openModal();
    });

    joinHubConfirm?.addEventListener('click', (e) => {
      e.preventDefault();
      submit();
    });

    joinHubCancel?.addEventListener('click', (e) => {
      e.preventDefault();
      closeModal();
    });

    joinHubModal.addEventListener('click', (e) => {
      if (e.target === joinHubModal || e.target?.dataset?.close === 'join-hub-modal') {
        closeModal();
      }
    });

    joinHubCodeInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        submit();
      } else if (e.key === 'Escape') {
        closeModal();
      }
    });

    document.addEventListener('hub:join:success', (ev) => {
      if (joinHubModal.classList.contains('hidden')) return;
      const detail = ev.detail || {};
      const hub = detail.hub || {};
      if (!hub || !hub.id) return;
      closeModal();
    });

    document.addEventListener('hub:join:error', (ev) => {
      if (joinHubModal.classList.contains('hidden')) return;
      const detail = ev.detail || {};
      const code = detail?.code;
      const message =
        detail?.message || detail?.error_message ||
        (code === 'invite_not_found'
          ? 'Invite code not valid.'
          : code === 'invalid_invite'
              ? 'Invite code is required.'
              : code === 'hub_not_found'
                  ? 'Hub not found.'
                  : code === 'join_hub_failed'
                      ? 'Unable to join hub.'
                      : 'Unable to join hub.');
      showError(message);
      setBusy(false);
    });

    document.addEventListener('ws:error', (ev) => {
      if (!joinBusy || joinHubModal.classList.contains('hidden')) return;
      const detail = ev.detail || {};
      if (!detail?.code) return;
      if (
        detail.code === 'invite_not_found' ||
        detail.code === 'invalid_invite' ||
        detail.code === 'hub_not_found' ||
        detail.code === 'join_hub_failed'
      ) {
        const message =
          detail.message || detail.error_message || 'Unable to join hub.';
        showError(message);
        setBusy(false);
      }
    });

    registerCloser(closeModal);
  }

  function wireLeaveHub() {
    const {
      leaveHubBtn,
      confirmModal,
      confirmModalTitle,
      confirmModalMessage,
      confirmModalPrimary,
      confirmModalSecondary
    } = els;

    if (!leaveHubBtn || !confirmModal || !confirmModalPrimary || !confirmModalMessage || !confirmModalTitle) return;

    const closeLeaveConfirm = () => {
      if (confirmModalPrimary.dataset.intent !== 'leave-hub') return;
      confirmModal.classList.add('hidden');
      confirmModalPrimary.disabled = false;
      confirmModalPrimary.classList.remove('danger');
      confirmModalPrimary.textContent = 'Confirm';
      confirmModalPrimary.dataset.intent = '';
    };

    leaveHubBtn.addEventListener('click', (e) => {
      e.preventDefault();
      if (leaveHubBtn.disabled) return;
      const hubId = state.current.hubId;
      if (!hubId) return;
      const hub = (state.hubs || []).find((h) => h.id === hubId);
      if (confirmModalTitle) confirmModalTitle.textContent = 'Leave Hub';
      if (confirmModalMessage) {
        const name = hub?.name ? ` \"${hub.name}\"` : '';
        confirmModalMessage.textContent =
          `Leave hub${name}? You will lose access to its channels and history.`;
      }
      confirmModalPrimary.textContent = 'Leave';
      confirmModalPrimary.classList.add('danger');
      confirmModalPrimary.dataset.intent = 'leave-hub';
      confirmModalPrimary.disabled = false;
      confirmModal.classList.remove('hidden');
      queueMicrotask(() => confirmModalPrimary?.focus());
    });

    confirmModalPrimary.addEventListener('click', (e) => {
      if (confirmModalPrimary.dataset.intent !== 'leave-hub') return;
      e.preventDefault();
      const hubId = state.current.hubId;
      if (!hubId) {
        closeLeaveConfirm();
        return;
      }
      confirmModalPrimary.disabled = true;
      leaveHubBtn.disabled = true;
      leaveHub(ws, hubId);
      closeLeaveConfirm();
    });

    confirmModalSecondary?.addEventListener('click', (e) => {
      if (confirmModalPrimary.dataset.intent !== 'leave-hub') return;
      e.preventDefault();
      closeLeaveConfirm();
    });

    confirmModal.addEventListener('click', (e) => {
      if (confirmModalPrimary.dataset.intent !== 'leave-hub') return;
      if (e.target === confirmModal || e.target?.dataset?.close === 'confirm-modal') {
        closeLeaveConfirm();
      }
    });

    document.addEventListener('hub:left:error', (ev) => {
      leaveHubBtn.disabled = false;
      if (els.leaveHubError) {
        const detail = ev.detail || {};
        const message = detail.message || detail.error_message || 'Unable to leave hub.';
        els.leaveHubError.textContent = message;
        els.leaveHubError.classList.remove('hidden');
      }
    });

    document.addEventListener('hub:left:success', () => {
      leaveHubBtn.disabled = false;
      if (els.leaveHubError) {
        els.leaveHubError.textContent = '';
        els.leaveHubError.classList.add('hidden');
      }
    });

    registerCloser(closeLeaveConfirm);
  }
}
