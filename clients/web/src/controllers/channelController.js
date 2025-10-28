// src/controllers/channelController.js
import { joinChannel, reqUsersForChannel } from '../api/chatApi.js';
import { actions } from '../store/state.js';
import { renderUsers, renderHistory } from '../views/chat.js';
import { sel } from '../store/selectors.js';

export function wireChannel({ ws, els }) {
  const { channelsList, usersList, userCount, messagesWrap, messages, currentChannelEl, messageInput, sendBtn } = els;

  channelsList.addEventListener('click', (e) => {
    const node = e.target.closest('[data-channel-id]'); if (!node) return;
    const cid = node.dataset.channelId, cname = node.dataset.channelName || '';
    joinChannel(ws, cid);
    reqUsersForChannel(ws, cid);
    if (currentChannelEl) currentChannelEl.textContent = `# ${cname}`;
  });

  ws.on('joined_channel', (msg) => {
    actions.setJoinedChannel(msg);
    renderUsers(usersList, userCount, sel.usersInChannel(msg.channel_id));
    renderHistory(messagesWrap, messages, sel.messagesInChannel(msg.channel_id));
    // Enable composer
    if (messageInput) messageInput.disabled = false;
    if (sendBtn) sendBtn.disabled = false;
  });

  ws.on('user_joined', (m) => {
    actions.upsertPresence({ ...m, online: true });
    if (m.channel_id === sel.currentChannelId()) {
      renderUsers(usersList, userCount, sel.usersInChannel(m.channel_id));
    }
  });
  ws.on('user_left', (m) => {
    actions.upsertPresence({ ...m, online: false });
    if (m.channel_id === sel.currentChannelId()) {
      renderUsers(usersList, userCount, sel.usersInChannel(m.channel_id));
    }
  });
}
