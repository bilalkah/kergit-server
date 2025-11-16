// src/controllers/profileController.js
import { updateProfile } from '../api/chatApi.js';
import { actions, state } from '../store/state.js';

export function createProfileController({ ws, els }) {
  if (!ws || !els) throw new Error('createProfileController requires ws and els');

  const {
    userInfo,
    profileModal,
    profileModalClose,
    profileUsername,
    profileFullName,
    profileSave,
    profileCancel,
    profileError
  } = els;

  if (!profileModal) {
    return { reset: () => {} };
  }

  let profileModalBusy = false;
  const defaultSaveText = profileSave?.textContent || 'Save';

  const showError = (msg) => {
    if (!profileError) return;
    if (!msg) {
      profileError.textContent = '';
      profileError.classList.add('hidden');
    } else {
      profileError.textContent = msg;
      profileError.classList.remove('hidden');
    }
  };

  const setBusy = (busy) => {
    profileModalBusy = busy;
    if (profileSave) {
      profileSave.disabled = busy;
      profileSave.textContent = busy ? 'Saving…' : defaultSaveText;
    }
  };

  const populateFields = () => {
    if (profileUsername) profileUsername.value = state.self.username || '';
    if (profileFullName) profileFullName.value = state.self.fullName || '';
  };

  const openModal = () => {
    if (!state.authed) return;
    populateFields();
    showError('');
    setBusy(false);
    profileModal.classList.remove('hidden');
    queueMicrotask(() => profileUsername?.focus());
  };

  const hideModal = () => {
    profileModal.classList.add('hidden');
    setBusy(false);
    showError('');
  };

  const submitUpdate = () => {
    if (profileModalBusy) return;
    const usernameRaw = profileUsername ? profileUsername.value.trim() : '';
    const fullNameRaw = profileFullName ? profileFullName.value.trim() : '';

    const payload = {};
    const currentUsername = state.self.username || '';
    const currentFullName = state.self.fullName || '';

    if (usernameRaw !== currentUsername) payload.username = usernameRaw;
    if (fullNameRaw !== currentFullName) payload.full_name = fullNameRaw;

    if (!('username' in payload) && !('full_name' in payload)) {
      showError('Update your username or full name to save changes.');
      return;
    }

    showError('');
    setBusy(true);
    updateProfile(ws, payload);
  };

  userInfo?.addEventListener('click', (e) => {
    e.preventDefault();
    openModal();
  });

  profileModalClose?.addEventListener('click', (e) => {
    e.preventDefault();
    hideModal();
  });
  profileCancel?.addEventListener('click', (e) => {
    e.preventDefault();
    hideModal();
  });
  profileModal.addEventListener('click', (e) => {
    if (e.target === profileModal) hideModal();
  });
  profileSave?.addEventListener('click', (e) => {
    e.preventDefault();
    submitUpdate();
  });

  const submitOnEnter = (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      submitUpdate();
    } else if (e.key === 'Escape') {
      hideModal();
    }
  };

  profileUsername?.addEventListener('keydown', submitOnEnter);
  profileFullName?.addEventListener('keydown', submitOnEnter);

  document.addEventListener('profile:update:success', (ev) => {
    setBusy(false);
    showError('');
    hideModal();
    const detail = ev.detail || {};
    const displayName =
      detail.display_name ||
      detail.username ||
      state.self.displayName ||
      state.self.username ||
      'Member';
    if (els.currentUser) els.currentUser.textContent = displayName;
    const currentSession = state.session || {};
    actions.setSession({
      url: currentSession.url ?? null,
      token: currentSession.token ?? null,
      username: displayName
    });
  });

  document.addEventListener('profile:update:error', (ev) => {
    setBusy(false);
    const detail = ev.detail || {};
    showError(detail.message || 'Profile update failed.');
  });

  return {
    reset: hideModal
  };
}
