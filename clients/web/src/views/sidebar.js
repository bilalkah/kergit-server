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

export function renderChannels(root, channels, currentId, options = {}) {
  if (!root) return;
  const { canDelete = false } = options;
  root.innerHTML = '';
  channels.forEach((c) => {
    const el = document.createElement('div');
    el.className = `channel ${c.id === currentId ? 'active' : ''}`;
    el.dataset.channelId = c.id;
    el.dataset.channelName = c.name;

    const label = document.createElement('span');
    label.className = 'channel-label';
    label.textContent = `# ${c.name}`;
    el.appendChild(label);

    if (canDelete) {
      const actions = document.createElement('div');
      actions.className = 'channel-actions';

      const btn = document.createElement('button');
      btn.className = 'channel-menu-btn';
      btn.type = 'button';
      btn.dataset.action = 'channel-settings';
      btn.dataset.channelId = c.id;
      btn.title = 'Channel settings';
      btn.innerHTML = '<i class="fas fa-ellipsis-h"></i>';
      actions.appendChild(btn);

      el.appendChild(actions);
    }

    root.appendChild(el);
  });
}
