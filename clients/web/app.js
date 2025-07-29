// Web-based Chat Client Application
class ChatApp {
    constructor() {
        this.ws = null;
        this.username = '';
        this.currentChannel = '';
        this.isConnected = false;
        this.channels = [];
        this.users = [];
        this.currentCall = null;
        
        this.initializeElements();
        this.bindEvents();
        this.showLoginScreen();
    }

    initializeElements() {
        // Login elements
        this.loginScreen = document.getElementById('login-screen');
        this.loginForm = document.getElementById('login-form');
        this.usernameInput = document.getElementById('username');
        this.serverUrlInput = document.getElementById('server-url');

        // Chat interface elements
        this.chatInterface = document.getElementById('chat-interface');
        this.currentChannelElement = document.getElementById('current-channel');
        this.connectionStatus = document.getElementById('connection-status');
        this.statusText = document.getElementById('status-text');
        this.currentUserElement = document.getElementById('current-user');
        this.disconnectBtn = document.getElementById('disconnect-btn');

        // Sidebar elements
        this.channelsList = document.getElementById('channels-list');
        this.usersList = document.getElementById('users-list');
        this.userCount = document.getElementById('user-count');
        this.createChannelBtn = document.getElementById('create-channel-btn');
        this.refreshChannelsBtn = document.getElementById('refresh-channels-btn');
        this.refreshUsersBtn = document.getElementById('refresh-users-btn');

        // Chat area elements
        this.messagesContainer = document.getElementById('messages-container');
        this.messages = document.getElementById('messages');
        this.messageInput = document.getElementById('message-input');
        this.sendBtn = document.getElementById('send-btn');
        this.emojiBtn = document.getElementById('emoji-btn');

        // Call elements
        this.callBtn = document.getElementById('call-btn');
        this.callStatus = document.getElementById('call-status');
        this.callState = document.getElementById('call-state');
        this.endCallBtn = document.getElementById('end-call-btn');

        // Call interface
        this.callInterface = document.getElementById('call-interface');
        this.callTitle = document.getElementById('call-title');
        this.callDuration = document.getElementById('call-duration');
        this.callerName = document.getElementById('caller-name');
        this.muteBtn = document.getElementById('mute-btn');
        this.videoBtn = document.getElementById('video-btn');
        this.screenShareBtn = document.getElementById('screen-share-btn');
        this.endCallBtnLarge = document.getElementById('end-call-btn-large');

        // Modals
        this.createChannelModal = document.getElementById('create-channel-modal');
        this.newChannelNameInput = document.getElementById('new-channel-name');
        this.createChannelConfirm = document.getElementById('create-channel-confirm');
        this.createChannelCancel = document.getElementById('create-channel-cancel');

        this.callModal = document.getElementById('call-modal');
        this.callModalTitle = document.getElementById('call-modal-title');
        this.callModalMessage = document.getElementById('call-modal-message');
        this.acceptCallBtn = document.getElementById('accept-call-btn');
        this.rejectCallBtn = document.getElementById('reject-call-btn');
    }

    bindEvents() {
        // Login events
        this.loginForm.addEventListener('submit', (e) => this.handleLogin(e));

        // Chat events
        this.disconnectBtn.addEventListener('click', () => this.disconnect());
        this.messageInput.addEventListener('keypress', (e) => this.handleMessageKeypress(e));
        this.sendBtn.addEventListener('click', () => {
            console.log('Send button clicked');
            this.sendMessage();
        });
        this.createChannelBtn.addEventListener('click', () => this.showCreateChannelModal());
        this.refreshChannelsBtn.addEventListener('click', () => this.requestChannels());
        this.refreshUsersBtn.addEventListener('click', () => this.requestUsers());

        // Call events
        this.callBtn.addEventListener('click', () => this.startCall());
        this.endCallBtn.addEventListener('click', () => this.endCall());
        this.endCallBtnLarge.addEventListener('click', () => this.endCall());
        this.muteBtn.addEventListener('click', () => this.toggleMute());
        this.videoBtn.addEventListener('click', () => this.toggleVideo());
        this.screenShareBtn.addEventListener('click', () => this.toggleScreenShare());

        // Modal events
        this.createChannelConfirm.addEventListener('click', () => {
            console.log('Create channel button clicked');
            this.createChannel();
        });
        this.createChannelCancel.addEventListener('click', () => this.hideCreateChannelModal());
        this.acceptCallBtn.addEventListener('click', () => this.acceptCall());
        this.rejectCallBtn.addEventListener('click', () => this.rejectCall());

        // Close modal events
        document.querySelectorAll('.close-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.target.closest('.modal').classList.add('hidden');
            });
        });

        // Click outside modal to close
        document.querySelectorAll('.modal').forEach(modal => {
            modal.addEventListener('click', (e) => {
                if (e.target === modal) {
                    modal.classList.add('hidden');
                }
            });
        });
    }

    showLoginScreen() {
        this.loginScreen.classList.remove('hidden');
        this.chatInterface.classList.add('hidden');
        this.callInterface.classList.add('hidden');
    }

    showChatInterface() {
        this.loginScreen.classList.add('hidden');
        this.chatInterface.classList.remove('hidden');
        this.callInterface.classList.add('hidden');
    }

    showCallInterface() {
        this.callInterface.classList.remove('hidden');
    }

    hideCallInterface() {
        this.callInterface.classList.add('hidden');
    }

    async handleLogin(e) {
        e.preventDefault();
        
        this.username = this.usernameInput.value.trim();
        const serverUrl = this.serverUrlInput.value.trim();
        
        if (!this.username) {
            alert('Please enter a username');
            return;
        }

        if (!serverUrl) {
            alert('Please enter a server URL');
            return;
        }

        this.updateConnectionStatus('connecting');
        await this.connect(serverUrl);
    }

    async connect(serverUrl) {
        try {
            this.ws = new WebSocket(serverUrl);
            
            this.ws.onopen = () => {
                console.log('Connected to server');
                this.isConnected = true;
                this.updateConnectionStatus('connected');
                this.showChatInterface();
                this.currentUserElement.textContent = this.username;
                this.messageInput.disabled = false;
                this.sendBtn.disabled = false;
                this.callBtn.disabled = false;
                
                // Request initial data
                this.requestChannels();
                
                // Join default channel
                this.joinChannel('general');
                
                // Set up periodic refresh of channels and users
                this.setupPeriodicRefresh();
            };

            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    console.log('📨 WebSocket message received:', data);
                    this.handleMessage(data);
                } catch (error) {
                    console.error('❌ Failed to parse WebSocket message:', error);
                    console.log('📄 Raw message:', event.data);
                    console.log('📄 Message type:', typeof event.data);
                    console.log('📄 Message length:', event.data.length);
                }
            };

            this.ws.onclose = () => {
                console.log('Disconnected from server');
                this.isConnected = false;
                this.updateConnectionStatus('disconnected');
                this.messageInput.disabled = true;
                this.sendBtn.disabled = true;
                this.callBtn.disabled = true;
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.updateConnectionStatus('disconnected');
                alert('Failed to connect to server. Please check the server URL and try again.');
            };

        } catch (error) {
            console.error('Connection error:', error);
            this.updateConnectionStatus('disconnected');
            alert('Failed to connect to server');
        }
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }
        
        // Clear refresh intervals
        if (this.channelsRefreshInterval) {
            clearInterval(this.channelsRefreshInterval);
            this.channelsRefreshInterval = null;
        }
        if (this.usersRefreshInterval) {
            clearInterval(this.usersRefreshInterval);
            this.usersRefreshInterval = null;
        }
        
        this.showLoginScreen();
        this.clearMessages();
        this.clearChannels();
        this.clearUsers();
    }

    updateConnectionStatus(status) {
        this.connectionStatus.className = `status-indicator ${status}`;
        
        switch (status) {
            case 'connected':
                this.statusText.textContent = 'Connected';
                break;
            case 'connecting':
                this.statusText.textContent = 'Connecting...';
                break;
            case 'disconnected':
                this.statusText.textContent = 'Disconnected';
                break;
        }
    }

    handleMessage(data) {
        console.log('Received message:', data);
        
        switch (data.type) {
            case 'joined':
                this.handleJoinedMessage(data);
                break;
            case 'chat':
                this.handleChatMessage(data);
                break;
            case 'channels':
                this.handleChannelsMessage(data);
                break;
            case 'users':
                this.handleUsersMessage(data);
                break;
            case 'user_joined':
                this.handleUserJoinedMessage(data);
                break;
            case 'user_left':
                this.handleUserLeftMessage(data);
                break;
            case 'user_disconnected':
                this.handleUserDisconnectedMessage(data);
                break;
            case 'call_incoming':
                this.handleCallIncoming(data);
                break;
            case 'call_accepted':
                this.handleCallAccepted(data);
                break;
            case 'call_rejected':
                this.handleCallRejected(data);
                break;
            case 'call_ended':
                this.handleCallEnded(data);
                break;
            case 'webrtc_signal':
                this.handleWebRTCSignal(data);
                break;
            case 'error':
                this.handleErrorMessage(data);
                break;
            default:
                console.log('Unknown message type:', data.type);
        }
    }

    handleJoinedMessage(data) {
        this.currentChannel = data.channel;
        this.currentChannelElement.textContent = `# ${data.channel}`;
        
        if (data.channel) {
            // Clear previous messages when joining a new channel
            this.clearMessages();
            this.addSystemMessage(`Joined channel #${data.channel}`);
            // Request users in the channel
            this.requestUsers();
            // Refresh channels list to show any new channels
            this.requestChannels();
        } else {
            this.addSystemMessage('Left the channel');
        }
    }

    handleChatMessage(data) {
        console.log('💬 Handling chat message:', data);
        this.addMessage(data.sender, data.text, data.timestamp);
    }

    handleChannelsMessage(data) {
        console.log('📋 Channels message received:', data);
        this.channels = data.channels || [];
        console.log('📋 Updated channels list:', this.channels);
        this.updateChannelsList();
    }

    handleUsersMessage(data) {
        console.log('👥 Users message received:', data);
        this.users = data.users || [];
        console.log('👥 Updated users list:', this.users);
        this.updateUsersList();
    }

    handleUserJoinedMessage(data) {
        console.log('👤 User joined message:', data);
        this.addSystemMessage(`${data.username} joined the channel`);
    }

    handleUserLeftMessage(data) {
        console.log('👤 User left message:', data);
        this.addSystemMessage(`${data.username} left the channel`);
    }

    handleUserDisconnectedMessage(data) {
        console.log('👤 User disconnected message:', data);
        this.addSystemMessage(`${data.username} disconnected`);
    }

    handleCallIncoming(data) {
        this.currentCall = {
            id: data.call_id,
            caller: data.from_user,
            type: data.media_type || 'voice'
        };
        
        this.callModalTitle.textContent = 'Incoming Call';
        this.callModalMessage.textContent = `${data.from_user} is calling you...`;
        this.callModal.classList.remove('hidden');
    }

    handleCallAccepted(data) {
        this.currentCall = {
            id: data.call_id,
            state: 'active'
        };
        
        this.callTitle.textContent = 'Voice Call';
        this.callerName.textContent = data.target_user || 'Unknown';
        this.showCallInterface();
        this.callModal.classList.add('hidden');
        
        this.addSystemMessage(`Call with ${data.target_user} started`);
    }

    handleCallRejected(data) {
        this.currentCall = null;
        this.callModal.classList.add('hidden');
        this.hideCallInterface();
        
        this.addSystemMessage(`Call with ${data.target_user} was rejected`);
    }

    handleCallEnded(data) {
        this.currentCall = null;
        this.hideCallInterface();
        
        this.addSystemMessage('Call ended');
    }

    handleWebRTCSignal(data) {
        // Handle WebRTC signaling messages
        console.log('WebRTC signal:', data);
    }

    handleErrorMessage(data) {
        this.addSystemMessage(`Error: ${data.message}`);
    }

    joinChannel(channelName) {
        console.log('joinChannel called:', { channelName, isConnected: this.isConnected, username: this.username });
        
        if (!this.isConnected) {
            console.log('joinChannel blocked: not connected');
            return;
        }
        
        // Clear messages immediately when switching channels
        if (this.currentChannel && this.currentChannel !== channelName) {
            this.clearMessages();
            this.addSystemMessage(`Switching to channel #${channelName}...`);
        }
        
        const message = {
            type: 'join',
            channel: channelName,
            username: this.username
        };
        
        this.ws.send(JSON.stringify(message));
        console.log('Joining channel:', message);
    }

    requestChannels() {
        if (!this.isConnected) return;
        
        const message = {
            type: 'list'
        };
        
        this.ws.send(JSON.stringify(message));
        console.log('Requesting channels:', message);
    }

    requestUsers() {
        if (!this.isConnected || !this.currentChannel) return;
        
        const message = {
            type: 'users'
        };
        
        this.ws.send(JSON.stringify(message));
        console.log('Requesting users:', message);
    }

    setupPeriodicRefresh() {
        // Refresh channels every 10 seconds
        this.channelsRefreshInterval = setInterval(() => {
            if (this.isConnected) {
                console.log('🔄 Periodic channels refresh');
                this.requestChannels();
            }
        }, 10000);

        // Refresh users every 5 seconds
        this.usersRefreshInterval = setInterval(() => {
            if (this.isConnected && this.currentChannel) {
                console.log('🔄 Periodic users refresh');
                this.requestUsers();
            }
        }, 5000);
    }

    sendMessage() {
        const text = this.messageInput.value.trim();
        console.log('sendMessage called:', { text, isConnected: this.isConnected, currentChannel: this.currentChannel });
        
        if (!text || !this.isConnected) {
            console.log('sendMessage blocked:', { hasText: !!text, isConnected: this.isConnected });
            return;
        }
        
        const message = {
            type: 'chat',
            text: text
        };
        
        this.ws.send(JSON.stringify(message));
        this.messageInput.value = '';
        console.log('Sent message:', message);
    }

    handleMessageKeypress(e) {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            this.sendMessage();
        }
    }

    startCall() {
        // For now, we'll just show a prompt to enter a username
        const targetUser = prompt('Enter username to call:');
        if (!targetUser) return;
        
        const message = {
            type: 'call_request',
            target_user: targetUser,
            media_type: 'voice'
        };
        
        this.ws.send(JSON.stringify(message));
        this.addSystemMessage(`Calling ${targetUser}...`);
    }

    acceptCall() {
        if (!this.currentCall) return;
        
        const message = {
            type: 'call_accept',
            call_id: this.currentCall.id
        };
        
        this.ws.send(JSON.stringify(message));
        this.callModal.classList.add('hidden');
    }

    rejectCall() {
        if (!this.currentCall) return;
        
        const message = {
            type: 'call_reject',
            call_id: this.currentCall.id
        };
        
        this.ws.send(JSON.stringify(message));
        this.callModal.classList.add('hidden');
        this.currentCall = null;
    }

    endCall() {
        if (!this.currentCall) return;
        
        const message = {
            type: 'call_end',
            call_id: this.currentCall.id
        };
        
        this.ws.send(JSON.stringify(message));
        this.hideCallInterface();
        this.currentCall = null;
    }

    toggleMute() {
        // Implement mute functionality
        console.log('Toggle mute');
    }

    toggleVideo() {
        // Implement video toggle functionality
        console.log('Toggle video');
    }

    toggleScreenShare() {
        // Implement screen share functionality
        console.log('Toggle screen share');
    }

    showCreateChannelModal() {
        this.createChannelModal.classList.remove('hidden');
        this.newChannelNameInput.focus();
    }

    hideCreateChannelModal() {
        this.createChannelModal.classList.add('hidden');
        this.newChannelNameInput.value = '';
    }

    createChannel() {
        const channelName = this.newChannelNameInput.value.trim();
        if (!channelName) return;
        
        this.joinChannel(channelName);
        this.hideCreateChannelModal();
    }

    addMessage(sender, text, timestamp) {
        const messageElement = document.createElement('div');
        messageElement.className = 'message';
        
        const avatar = document.createElement('div');
        avatar.className = 'message-avatar';
        avatar.textContent = sender.charAt(0).toUpperCase();
        
        const content = document.createElement('div');
        content.className = 'message-content';
        
        const header = document.createElement('div');
        header.className = 'message-header';
        
        const senderElement = document.createElement('span');
        senderElement.className = 'message-sender';
        senderElement.textContent = sender;
        
        const timeElement = document.createElement('span');
        timeElement.className = 'message-time';
        timeElement.textContent = timestamp ? this.formatTime(timestamp) : this.formatTime(Date.now());
        
        header.appendChild(senderElement);
        header.appendChild(timeElement);
        
        const textElement = document.createElement('div');
        textElement.className = 'message-text';
        textElement.textContent = text;
        
        content.appendChild(header);
        content.appendChild(textElement);
        
        messageElement.appendChild(avatar);
        messageElement.appendChild(content);
        
        this.messages.appendChild(messageElement);
        this.scrollToBottom();
    }

    addSystemMessage(text) {
        const messageElement = document.createElement('div');
        messageElement.className = 'system-message';
        messageElement.textContent = text;
        
        this.messages.appendChild(messageElement);
        this.scrollToBottom();
    }

    updateChannelsList() {
        console.log('🔄 Updating channels list UI:', this.channels);
        this.channelsList.innerHTML = '';
        
        this.channels.forEach(channel => {
            const channelElement = document.createElement('div');
            channelElement.className = 'channel';
            if (channel === this.currentChannel) {
                channelElement.classList.add('active');
            }
            channelElement.textContent = `# ${channel}`;
            channelElement.addEventListener('click', () => this.joinChannel(channel));
            this.channelsList.appendChild(channelElement);
        });
        console.log('🔄 Channels list updated, total channels:', this.channels.length);
    }

    updateUsersList() {
        console.log('🔄 Updating users list UI:', this.users);
        this.usersList.innerHTML = '';
        this.userCount.textContent = this.users.length;
        
        this.users.forEach(user => {
            const userElement = document.createElement('div');
            userElement.className = 'user';
            userElement.textContent = user;
            userElement.addEventListener('click', () => this.startCallWithUser(user));
            this.usersList.appendChild(userElement);
        });
        console.log('🔄 Users list updated, total users:', this.users.length);
    }

    startCallWithUser(username) {
        if (username === this.username) return;
        
        const message = {
            type: 'call_request',
            target_user: username,
            media_type: 'voice'
        };
        
        this.ws.send(JSON.stringify(message));
        this.addSystemMessage(`Calling ${username}...`);
    }

    clearMessages() {
        this.messages.innerHTML = '';
    }

    clearChannels() {
        this.channelsList.innerHTML = '';
        this.channels = [];
    }

    clearUsers() {
        this.usersList.innerHTML = '';
        this.users = [];
        this.userCount.textContent = '0';
    }

    scrollToBottom() {
        this.messagesContainer.scrollTop = this.messagesContainer.scrollHeight;
    }

    formatTime(timestamp) {
        const date = new Date(timestamp);
        return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
}

// Initialize the application when the page loads
document.addEventListener('DOMContentLoaded', () => {
    new ChatApp();
}); 