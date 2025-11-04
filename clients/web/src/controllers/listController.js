// src/controllers/listController.js
import { reqList } from '../api/chatApi.js';
import { state } from '../store/state.js';
import { renderChannels } from '../views/sidebar.js';
import { sel } from '../store/selectors.js';

export function wireList({ ws, els }) {
  const { channelsList, refreshChannelsBtn } = els;

  refreshChannelsBtn?.addEventListener('click', () => reqList(ws));

  ws.on('hubs_list', () => {
    if (!channelsList) return;
    const hubId = state.current.hubId;
    if (!hubId) return;
    renderChannels(channelsList, sel.channels(hubId), sel.currentChannelId());
  });
}
