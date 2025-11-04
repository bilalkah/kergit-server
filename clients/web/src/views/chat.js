// src/views/chat.js
export function renderUsers(root, countEl, arr) {
  root.innerHTML = '';
  countEl.textContent = String(arr.length);
  arr.forEach(u => {
    const el = document.createElement('div');
    el.className = 'user';
    el.textContent = u.display_name || u.handle || 'Member';
    if (u.online) el.classList.add('online');
    root.appendChild(el);
  });
}

export function renderHistory(rootWrap, root, msgs) {
  root.innerHTML = '';
  msgs.forEach(m => appendMessage(root, m.sender, m.content, m.sent_at));
  rootWrap.scrollTop = rootWrap.scrollHeight;
}

export function appendMessage(root, sender, text, ts) {
  const row = document.createElement('div'); row.className = 'message';
  const av = document.createElement('div'); av.className = 'message-avatar';
  av.textContent = (sender || '?')[0]?.toUpperCase() || '?';
  const content = document.createElement('div'); content.className = 'message-content';
  const header = document.createElement('div'); header.className = 'message-header';
  const s = document.createElement('span'); s.className = 'message-sender'; s.textContent = sender || 'unknown';
  const t = document.createElement('span'); t.className = 'message-time';
  t.textContent = (ts ? new Date(ts) : new Date()).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  header.appendChild(s); header.appendChild(t);
  const body = document.createElement('div'); body.className = 'message-text'; body.textContent = text || '';
  content.appendChild(header); content.appendChild(body);
  row.appendChild(av); row.appendChild(content);
  root.appendChild(row);
}
