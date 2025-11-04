// src/controllers/channelController.js
import { joinChannel, reqUsersForChannel } from '../api/chatApi.js';
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
    membersSidebar
  } = els;

  if (!channelsList) return;

  channelsList.addEventListener('click', (e) => {
    const node = e.target.closest('[data-channel-id]'); if (!node) return;
    const cid = node.dataset.channelId, cname = node.dataset.channelName || '';
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

  ws.on('joined_channel', (msg) => {
    actions.setJoinedChannel(msg);
    renderUsers(usersList, userCount, sel.membersInHub(state.current.hubId));
    renderHistory(messagesWrap, messages, sel.messagesInChannel(msg.channel_id));
    const hubId = state.current.hubId;
    if (hubId) {
      renderChannels(channelsList, sel.channels(hubId), sel.currentChannelId());
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
}
