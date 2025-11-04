// src/views/sidebar.js
export function renderHubs(root, hubs, currentId) {
  if (!root) return;
  root.innerHTML = '';
  hubs.forEach(h => {
    const el = document.createElement('button');
    el.className = 'hub-icon';
    el.dataset.hubId = h.id;
    el.title = h.name || 'Hub';
    const initials = (h?.name?.trim()?.charAt(0) || h?.id?.charAt(0) || '#').toUpperCase();
    el.textContent = initials;
    if (currentId && h.id === currentId) el.classList.add('active');
    root.appendChild(el);
  });
}

export function renderChannels(root, channels, currentId) {
  root.innerHTML = '';
  channels.forEach(c => {
    const el = document.createElement('div');
    el.className = `channel ${c.id === currentId ? 'active' : ''}`;
    el.textContent = `# ${c.name}`;
    el.dataset.channelId = c.id;
    el.dataset.channelName = c.name;
    root.appendChild(el);
  });
}
