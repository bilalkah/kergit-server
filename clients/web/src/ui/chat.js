import { qs, el } from './dom.js';
}


function addMessage(sender, text, timestamp) {
    const row = el('div', 'message');
    const av = el('div', 'message-avatar');
    av.textContent = (sender || '?').charAt(0).toUpperCase();
    const content = el('div', 'message-content');
    const header = el('div', 'message-header');
    const s = el('span', 'message-sender');
    s.textContent = sender || 'unknown';
    const t = el('span', 'message-time');
    const dt = timestamp ? new Date(timestamp) : new Date();
    t.textContent = dt.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    header.appendChild(s); header.appendChild(t);
    const body = el('div', 'message-text');
    body.textContent = text;
    content.appendChild(header); content.appendChild(body);
    row.appendChild(av); row.appendChild(content);
    messages.appendChild(row);
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
}


function setChannels(channels, active) {
    channelsList.innerHTML = '';
    (channels || []).forEach((ch) => {
        const n = el('div', 'channel');
        if (ch === active) n.classList.add('active');
        n.textContent = `# ${ch}`;
        n.addEventListener('click', () => onJoin(ch));
        channelsList.appendChild(n);
    });
}


function setUsers(users) {
    usersList.innerHTML = '';
    userCount.textContent = (users || []).length;
    (users || []).forEach((u) => {
        const n = el('div', 'user');
        n.textContent = u;
        usersList.appendChild(n);
    });
}


messageInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); onSend(messageInput.value.trim()); }
});
sendBtn.addEventListener('click', () => onSend(messageInput.value.trim()));
emojiBtn.addEventListener('click', () => {/* stub for later */ });
createChannelBtn.addEventListener('click', () => {
    const name = prompt('Channel name');
    if (name) onJoin(name.trim());
});
refreshChannelsBtn.addEventListener('click', () => onRefreshChannels());
refreshUsersBtn.addEventListener('click', () => onRefreshUsers());
disconnectBtn.addEventListener('click', () => onDisconnect());


function enableComposer(enabled) {
    messageInput.disabled = !enabled;
    sendBtn.disabled = !enabled;
    emojiBtn.disabled = !enabled;
    if (enabled) messageInput.focus();
}


function clearComposer() { messageInput.value = ''; }


return {
    show, hide,
    setCurrentUser, setCurrentChannel,
    clearMessages, addSystem, addMessage,
    setChannels, setUsers,
    enableComposer, clearComposer,
    // callbacks
    onSend: (fn) => { onSend = fn; },
    onJoin: (fn) => { onJoin = fn; },
    onRefreshChannels: (fn) => { onRefreshChannels = fn; },
    onRefreshUsers: (fn) => { onRefreshUsers = fn; },
    onDisconnect: (fn) => { onDisconnect = fn; },
};
}