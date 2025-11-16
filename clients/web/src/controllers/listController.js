// src/controllers/listController.js
import { state } from '../store/state.js';
import { renderChannels } from '../views/sidebar.js';
import { sel } from '../store/selectors.js';

export function wireList({ ws, els }) {
  const { channelsList } = els;

  ws.on('hubs_list', () => {
    if (!channelsList) return;
    const hubId = state.current.hubId;
    if (!hubId) return;
    const role = sel.currentHubRole();
    const canDelete = role === 'owner' || role === 'admin';
    renderChannels(channelsList, sel.channels(hubId), sel.currentChannelId(), { canDelete });
  });
}
