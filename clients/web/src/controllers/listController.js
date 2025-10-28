// src/controllers/listController.js
import { reqList } from '../api/chatApi.js';
import { actions } from '../store/state.js';
import { renderChannels } from '../views/sidebar.js';
import { sel } from '../store/selectors.js';

export function wireList({ ws, els }) {
  const { channelsList } = els;

  ws.on('auth_ok', () => reqList(ws));

  ws.on('list_ok', (msg) => {
    actions.setList(msg);
    const hubs = sel.hubs();
    const firstHub = hubs[0];
    if (firstHub) {
      renderChannels(channelsList, sel.channels(firstHub.id), sel.currentChannelId());
    } else {
      channelsList.innerHTML = '<div class="empty">No channels yet</div>';
    }
  });
}
