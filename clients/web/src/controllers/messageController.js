// src/controllers/messageController.js
import { sendMessage } from '../api/chatApi.js';
import { actions } from '../store/state.js';
import { appendMessage } from '../views/chat.js';
import { sel } from '../store/selectors.js';

export function wireMessaging({ ws, els }) {
  const { sendBtn, messageInput, messages } = els;

  const doSend = () => {
    const cid = sel.currentChannelId(); if (!cid) return;
    const text = (messageInput.value || '').trim(); if (!text) return;
    sendMessage(ws, cid, text);
    messageInput.value = '';
  };

  sendBtn.addEventListener('click', doSend);
  messageInput.addEventListener('keydown', (e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); doSend(); } });

  ws.on('message', (m) => {
    if (m.channel_id !== sel.currentChannelId()) return;
    const display = m.sender || 'Member';
    actions.pushMessage({ channel_id: m.channel_id, sender: display, content: m.content, sent_at: m.sent_at });
    appendMessage(messages, display, m.content, m.sent_at);
  });

  ws.on('error', (m) => {
    appendMessage(messages, 'system', `[Error] ${m.code || ''} ${m.detail || m.message || ''}`);
  });
}
