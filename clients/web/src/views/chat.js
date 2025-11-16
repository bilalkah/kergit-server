// src/views/chat.js
export function renderUsers(root, countEl, arr) {
  if (!root || !countEl) return;
  const members = Array.isArray(arr) ? arr : [];
  const online = members.filter((m) => m.online);
  const offline = members.filter((m) => !m.online);

  root.innerHTML = '';
  countEl.textContent = String(members.length);

  const appendHeader = (label, amount, sectionClass) => {
    const header = document.createElement('div');
    header.className = `users-section ${sectionClass}`.trim();
    header.textContent = `${label} — ${amount}`;
    root.appendChild(header);
  };

  const appendUser = (user, isOnline) => {
    const el = document.createElement('div');
    el.className = `user ${isOnline ? 'online' : 'offline'}`.trim();
    el.textContent = user.display_name || user.handle || 'Member';
    if (user.user_id) el.dataset.userId = user.user_id;
    root.appendChild(el);
  };

  appendHeader('Online', online.length, 'online');
  online.forEach((member) => appendUser(member, true));

  appendHeader('Offline', offline.length, 'offline');
  offline.forEach((member) => appendUser(member, false));
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
