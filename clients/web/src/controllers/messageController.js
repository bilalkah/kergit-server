// src/controllers/messageController.js
import { sendMessage } from '../api/chatApi.js';
import { renderHistory, appendMessage } from '../views/chat.js';
import { sel } from '../store/selectors.js';

export function wireMessaging({ ws, els }) {
  const { sendBtn, messageInput, messages, messagesWrap } = els;

  const doSend = () => {
    const cid = sel.currentChannelId(); if (!cid) return;
    const text = (messageInput.value || '').trim(); if (!text) return;
    sendMessage(ws, cid, text);
    messageInput.value = '';
  };

  sendBtn.addEventListener('click', doSend);
  messageInput.addEventListener('keydown', (e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); doSend(); } });

  const renderCurrentChannelMessages = () => {
    if (!messages || !messagesWrap) return;
    const cid = sel.currentChannelId();
    if (!cid) return;
    renderHistory(messagesWrap, messages, sel.messagesInChannel(cid));
  };

  document.addEventListener('messages:updated', (ev) => {
    const detail = ev.detail || {};
    if (!detail.channel_id) return;
    if (detail.channel_id !== sel.currentChannelId()) return;
    renderCurrentChannelMessages();
  });

  ws.on('error', (m = {}) => {
    if (!messages) return;
    appendMessage(messages, 'system', `[Error] ${m.code || ''} ${m.detail || m.message || ''}`);
  });
}
