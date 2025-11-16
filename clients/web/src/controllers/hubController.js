// src/controllers/hubController.js
import { actions, state, getPreferredHubId } from '../store/state.js';
import { sel } from '../store/selectors.js';
import { renderHubs, renderChannels } from '../views/sidebar.js';
import { renderUsers } from '../views/chat.js';

export function createHubController({ els, onNoHubState } = {}) {
  if (!els) throw new Error('createHubController requires els');

  const callbacks = {
    onNoHubState: typeof onNoHubState === 'function' ? onNoHubState : null
  };

  const handleHubSelection = (hubId) => {
    if (!hubId || state.current.hubId === hubId) return;
    actions.setCurrentHub(hubId);
    updateHubUI();
  };

  const renderHubRail = () => {
    const hubs = state.hubs || [];
    renderHubs(els.hubRailList, hubs, state.current.hubId);
    if (!els.hubRailList) return;
    els.hubRailList.querySelectorAll('.hub-icon').forEach((btn) => {
      btn.addEventListener('click', () => handleHubSelection(btn.dataset.hubId));
    });
  };

  const showNoHubState = () => {
    if (els.channelEmptyState) {
      els.channelEmptyState.classList.remove('hidden');
      const text = els.channelEmptyState.querySelector('p');
      if (text) text.textContent = 'Create or join a hub to view channels.';
    }
    els.hubActions?.classList.add('hidden');
    if (els.leaveHubBtn) {
      els.leaveHubBtn.classList.add('hidden');
      els.leaveHubBtn.disabled = true;
    }
    if (els.leaveHubError) {
      els.leaveHubError.textContent = '';
      els.leaveHubError.classList.add('hidden');
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
    if (els.currentHubName) els.currentHubName.textContent = 'Select Hub';
    if (els.hubSettingsBtn) {
      els.hubSettingsBtn.style.visibility = 'hidden';
      els.hubSettingsBtn.disabled = true;
    }
    if (els.createChannelBtn) {
      els.createChannelBtn.style.visibility = 'hidden';
      els.createChannelBtn.disabled = true;
    }
    callbacks.onNoHubState?.();
  };

  const updateHubUI = () => {
    const hubs = state.hubs || [];
    const currentHub = hubs.find((h) => h.id === state.current.hubId);

    renderHubRail();

    if (!currentHub) {
      if (hubs.length === 0) showNoHubState();
      return;
    }

    const role = currentHub.role || sel.currentHubRole() || '';
    const isOwner = role === 'owner';
    const canCreate = role === 'owner' || role === 'admin';
    if (els.hubSettingsBtn) {
      els.hubSettingsBtn.style.visibility = isOwner ? 'visible' : 'hidden';
      els.hubSettingsBtn.disabled = !isOwner;
    }
    if (els.leaveHubBtn) {
      if (isOwner) {
        els.leaveHubBtn.classList.add('hidden');
        els.leaveHubBtn.disabled = true;
        els.hubActions?.classList.add('hidden');
        if (els.leaveHubError) {
          els.leaveHubError.textContent = '';
          els.leaveHubError.classList.add('hidden');
        }
      } else {
        els.leaveHubBtn.classList.remove('hidden');
        els.leaveHubBtn.disabled = false;
        els.hubActions?.classList.remove('hidden');
        if (els.leaveHubError) {
          els.leaveHubError.textContent = '';
          els.leaveHubError.classList.add('hidden');
        }
      }
    } else if (els.hubActions) {
      els.hubActions.classList.add('hidden');
    }

    if (els.createChannelBtn) {
      if (canCreate) {
        els.createChannelBtn.style.visibility = 'visible';
        els.createChannelBtn.disabled = false;
      } else {
        els.createChannelBtn.style.visibility = 'hidden';
        els.createChannelBtn.disabled = true;
      }
    }

    if (els.currentHubName) {
      els.currentHubName.textContent = currentHub.name || 'Untitled Hub';
    }

    const channels = sel.channels(currentHub.id);
    const canManageChannels = role === 'owner' || role === 'admin';
    els.channelsSection?.classList.remove('hidden');
    if (channels.length) {
      if (els.channelEmptyState) els.channelEmptyState.classList.add('hidden');
      renderChannels(els.channelsList, channels, state.current.channelId, { canDelete: canManageChannels });
    } else {
      if (els.channelsList) els.channelsList.innerHTML = '';
      if (els.channelEmptyState) {
        const text = els.channelEmptyState.querySelector('p');
        if (text) text.textContent = canCreate
          ? 'No channels yet. Use the + button to create one.'
          : 'No channels available in this hub.';
        els.channelEmptyState.classList.remove('hidden');
      }
    }

    if (els.membersSidebar) els.membersSidebar.classList.remove('hidden');
    const members = state.current.channelId
      ? sel.usersInChannel(state.current.channelId)
      : sel.membersInHub(currentHub.id);
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
  };

  const bind = () => {
    document.addEventListener('hubs:changed', updateHubUI);
    document.addEventListener('presence:updated', (ev) => {
      const detail = ev.detail || {};
      const channelId = detail.channel_id;
      if (!channelId || channelId !== state.current.channelId) return;
      if (els.usersList && els.userCount) {
        renderUsers(els.usersList, els.userCount, sel.usersInChannel(channelId));
      }
    });
  };

  const handleHubListUpdate = () => {
    const hubs = state.hubs || [];
    if (!hubs.length) {
      renderHubRail();
      showNoHubState();
      return;
    }
    const hasCurrent = hubs.some((h) => h.id === state.current.hubId);
    if (!state.current.hubId || !hasCurrent) {
      const preferred = getPreferredHubId();
      const preferredExists = preferred && hubs.some((h) => h.id === preferred);
      if (preferredExists && preferred !== state.current.hubId) {
        actions.setCurrentHub(preferred);
      } else if (!hasCurrent && hubs[0]) {
        actions.setCurrentHub(hubs[0].id);
      } else if (!state.current.hubId && hubs[0]) {
        actions.setCurrentHub(hubs[0].id);
      }
    }
    updateHubUI();
  };

  return {
    bind,
    refresh: updateHubUI,
    renderHubRail,
    showNoHubState,
    handleHubListUpdate,
    reset: () => {
      renderHubRail();
      showNoHubState();
    }
  };
}
