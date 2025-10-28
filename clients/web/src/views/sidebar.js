// src/views/sidebar.js
export function renderHubs(root, hubs) {
  root.innerHTML = '';
  hubs.forEach(h => {
    const el = document.createElement('div');
    el.className = 'hub';
    el.textContent = h.name;
    el.dataset.hubId = h.id;
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
