// src/store/selectors.js
import { state } from './state.js';

export const sel = {
  hubs: () => state.hubs,
  channels: (hubId) => state.channelsByHub[hubId] || [],
  currentChannelId: () => state.current.channelId,
  currentChannelName: () => state.current.channelName,
  usersInChannel: (cid) => state.usersByChannel[cid] || [],
  messagesInChannel: (cid) => state.messagesByChannel[cid] || []
};
