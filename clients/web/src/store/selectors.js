// src/store/selectors.js
import { state } from './state.js';

export const sel = {
  hubs: () => state.hubs,
  channels: (hubId) => state.channelsByHub[hubId] || [],
  currentChannelId: () => state.current.channelId,
  currentChannelName: () => state.current.channelName,
  usersInChannel: (cid) => state.usersByChannel[cid] || [],
  messagesInChannel: (cid) => state.messagesByChannel[cid] || [],
  membersInHub: (hubId) => state.membersByHub[hubId] || [],
  currentHubRole: () => {
    if (!state.current.hubId) return '';
    const hub = (state.hubs || []).find((h) => h.id === state.current.hubId);
    return hub?.role || '';
  }
};
