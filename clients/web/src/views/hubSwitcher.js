// src/views/hubSwitcher.js
export function initHubSwitcher({ els, getHubs, onSelect }) {
  const { hubSelector, currentHubName, hubSwitcher, hubsListEl } = els;
  if (!hubSelector || !currentHubName || !hubSwitcher || !hubsListEl) {
    console.warn('[HubSwitcher] Missing DOM elements'); return;
  }

  const open = () => hubSwitcher.classList.remove('hidden');
  const close = () => hubSwitcher.classList.add('hidden');

  function render(hubs) {
    hubsListEl.innerHTML = '';
    hubs.forEach(h => {
      const item = document.createElement('div');
      item.className = 'hub-item';
      item.innerHTML = `
        <span>${h.name}</span>
        ${h.online != null ? `<small>${h.online} online</small>` : ''}
      `;
      item.addEventListener('click', () => {
        currentHubName.textContent = h.name;
        close();
        onSelect && onSelect(h);
        document.dispatchEvent(new CustomEvent('hub:selected', { detail: h }));
      });
      hubsListEl.appendChild(item);
    });
  }

  hubSelector.addEventListener('click', async () => {
    const willOpen = hubSwitcher.classList.contains('hidden');
    if (willOpen) {
      const hubs = await Promise.resolve(getHubs ? getHubs() : []);
      render(hubs || []);
      open();
    } else {
      close();
    }
  });

  // click-away close
  document.addEventListener('click', (e) => {
    if (!hubSwitcher.contains(e.target) && !hubSelector.contains(e.target)) close();
  });

  // ESC to close
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape') close(); });
}
