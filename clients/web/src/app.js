import { initializeSupabase, loginWithPassword, signUp } from './auth/supabase.js';
import { CONFIG } from './config.js';

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

        this.hubs = [];
        this.hubChannels = {}; // hub_id -> [{id, name, type}]
        this.authMode = 'login';

        this.initializeElements();
        this.bindEvents();
        this.showLoginScreen();
        this.setAuthMode('login');
    }

    // --- Security helpers ---
    generateId() {
        if (crypto && crypto.getRandomValues) {
            const buf = new Uint8Array(16);
            crypto.getRandomValues(buf);
            // RFC4122 v4
            buf[6] = (buf[6] & 0x0f) | 0x40;
            buf[8] = (buf[8] & 0x3f) | 0x80;
            const hex = [...buf].map(b => b.toString(16).padStart(2, '0'));
            return (
                hex[0] + hex[1] + hex[2] + hex[3] + '-' +
                hex[4] + hex[5] + '-' + hex[6] + hex[7] + '-' +
                hex[8] + hex[9] + '-' + hex[10] + hex[11] + hex[12] + hex[13] + hex[14] + hex[15]
            );
        }
        // Fallback
        return (Date.now().toString(36) + Math.random().toString(36).slice(2, 10)).slice(0, 16);
    }

    nowSeconds() {
        return Math.floor(Date.now() / 1000);
    }

    sendSecure(type, payload = {}) {
        const base = { type, id: this.generateId(), timestamp: this.nowSeconds() };
        const message = { ...base, ...payload };
        this.ws.send(JSON.stringify(message));
        return message;
    }

    initializeElements() {
        // Login elements
        this.loginScreen = document.getElementById('login-screen');
        this.loginForm = document.getElementById('login-form');
        this.loginUsernameInput = document.getElementById('login-username');
        this.usernameInput = document.getElementById('username');
        this.fullNameInput = document.getElementById('full-name');
        this.emailInput = document.getElementById('email');
        this.passwordInput = document.getElementById('password');
        this.authSubmitBtn = document.getElementById('auth-submit-btn');
        this.authSubmitText = document.getElementById('auth-submit-text');
        this.switchToSignup = document.getElementById('switch-to-signup');
        this.switchToSignin = document.getElementById('switch-to-signin');
        this.toggleToSignupSpan = document.getElementById('toggle-to-signup');
        this.toggleToSigninSpan = document.getElementById('toggle-to-signin');
        this.authEmailGroup = document.querySelector('.auth-email');
        this.authSignupFields = document.querySelectorAll('.auth-signup-fields');
        this.authLoginFields = document.querySelectorAll('.auth-login-fields');
        this.authErrorBanner = document.getElementById('auth-error');

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
        // Suppress default form submit
        this.loginForm.addEventListener('submit', (e) => e.preventDefault());
        this.authSubmitBtn.addEventListener('click', () => this.handleAuth(this.authMode));
        this.switchToSignup.addEventListener('click', (e) => { e.preventDefault(); this.setAuthMode('register'); });
        this.switchToSignin.addEventListener('click', (e) => { e.preventDefault(); this.setAuthMode('login'); });

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

    setAuthMode(mode) {
        this.authMode = mode;
        const isSignup = mode === 'register';
        
        if (isSignup) {
            // Show signup fields
            this.authSignupFields.forEach(field => field.classList.remove('hidden'));
            this.authLoginFields.forEach(field => field.classList.add('hidden'));
            this.authEmailGroup.classList.remove('hidden');
            this.authSubmitText.textContent = 'Sign Up';
            this.toggleToSignupSpan.classList.add('hidden');
            this.toggleToSigninSpan.classList.remove('hidden');
        } else {
            // Show login fields (but keep username field hidden for login)
            this.authSignupFields.forEach(field => field.classList.add('hidden'));
            this.authLoginFields.forEach(field => field.classList.add('hidden')); // Keep hidden for login
            this.authEmailGroup.classList.remove('hidden');
            this.authSubmitText.textContent = 'Sign In';
            this.toggleToSignupSpan.classList.remove('hidden');
            this.toggleToSigninSpan.classList.add('hidden');
        }
        this.clearAuthError();
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

    showAuthError(message) {
        if (!this.authErrorBanner) return;
        this.authErrorBanner.textContent = message || 'An error occurred';
        this.authErrorBanner.style.display = 'block';
    }

    clearAuthError() {
        if (!this.authErrorBanner) return;
        this.authErrorBanner.textContent = '';
        this.authErrorBanner.style.display = 'none';
    }

    async handleAuth(mode) {
        const email = this.emailInput.value.trim();
        const password = this.passwordInput.value.trim();

        this.clearAuthError();

        // Validation
        if (!email) { this.showAuthError('Please enter an email'); return; }
        if (!password) { this.showAuthError('Please enter a password'); return; }
        
        // Check if Supabase config is available
        if (!CONFIG.SUPABASE_URL || !CONFIG.SUPABASE_ANON) {
            this.showAuthError('Supabase configuration not found. Please check your config.');
            return;
        }

        // Additional validation for signup
        if (mode === 'register') {
            const fullName = this.fullNameInput.value.trim();
            const username = this.usernameInput.value.trim();
            if (!fullName) { this.showAuthError('Please enter your full name'); return; }
            if (!username) { this.showAuthError('Please enter a username'); return; }
        }

        this.updateConnectionStatus('connecting');
        this.authSubmitBtn.disabled = true;
        this.authSubmitBtn.classList.add('loading');
        
        // Update button text based on mode
        const originalText = this.authSubmitText.textContent;
        this.authSubmitText.textContent = mode === 'register' ? 'Creating Account...' : 'Signing In...';

        try {
            // Initialize Supabase
            initializeSupabase(CONFIG.SUPABASE_URL, CONFIG.SUPABASE_ANON);

            // Perform authentication with Supabase
            let authResult;
            if (mode === 'register') {
                const fullName = this.fullNameInput.value.trim();
                const username = this.usernameInput.value.trim();
                console.log('Attempting Supabase signup with:', { email, fullName, username });
                authResult = await signUp(email, password, {
                    full_name: fullName,
                    username: username
                });
                this.username = username; // Set username for the app
                console.log('Supabase signup successful:', authResult);
            } else {
                console.log('Attempting Supabase login with:', { email });
                authResult = await loginWithPassword(email, password);
                // For login, we'll use the email as username or extract from user metadata
                this.username = authResult.user.user_metadata?.username || email.split('@')[0];
                console.log('Supabase login successful:', authResult);
            }

            // Connect to WebSocket server
            await this.connect(CONFIG.WS_URL);
            if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
                throw new Error('Failed to connect to WebSocket server');
            }

            // Send authentication token to server
            const authMessage = {
                type: 'auth', 
                token: authResult.token,
                username: this.username,
                auth_type: mode
            };
            console.log('Sending auth message to server:', authMessage);
            this.ws.send(JSON.stringify(authMessage));
            
            // Don't re-enable the button here - wait for server response
            // The auth_response handler will handle success/failure

        } catch (err) {
            console.error('Authentication error:', err);
            this.showAuthError(err.message || 'Failed to authenticate');
            this.authSubmitBtn.disabled = false;
            this.authSubmitBtn.classList.remove('loading');
            this.authSubmitText.textContent = this.authMode === 'register' ? 'Sign Up' : 'Sign In';
            this.updateConnectionStatus('disconnected');
        }
    }


    async connect(serverUrl) {
        // If already open, resolve immediately
        if (this.ws && this.ws.readyState === WebSocket.OPEN) return;
        // If connecting, wait until it opens or closes
        if (this.ws && this.ws.readyState === WebSocket.CONNECTING) {
            await new Promise((resolve, reject) => {
                const check = () => {
                    if (!this.ws) return reject(new Error('socket missing'));
                    if (this.ws.readyState === WebSocket.OPEN) return resolve();
                    if (this.ws.readyState === WebSocket.CLOSED) return reject(new Error('socket closed'));
                    setTimeout(check, 25);
                };
                check();
            });
            return;
        }

        return new Promise((resolve, reject) => {
            try {
                console.log('Opening WebSocket to', serverUrl);
                this.ws = new WebSocket(serverUrl);

                this.ws.onopen = () => {
                    console.log('Connected to server');
                    this.isConnected = true;
                    this.updateConnectionStatus('connected');
                    resolve();
                };

                this.ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        console.log('📨 WebSocket message received:', data);
                        this.handleMessage(data);
                    } catch (error) {
                        console.error('❌ Failed to parse WebSocket message:', error);
                        console.log('📄 Raw message:', event.data);
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
                    this.showAuthError('WebSocket error. Check server URL and try again.');
                    reject(error);
                };
            } catch (error) {
                reject(error);
            }
        });
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }

        // Clear refresh intervals
        if (this.channelsRefreshInterval) { clearInterval(this.channelsRefreshInterval); this.channelsRefreshInterval = null; }
        if (this.usersRefreshInterval) { clearInterval(this.usersRefreshInterval); this.usersRefreshInterval = null; }

        this.showLoginScreen();
        this.clearMessages();
        this.clearChannels();
        this.clearUsers();
        this.clearAuthError();
    }

    updateConnectionStatus(status) {
        this.connectionStatus.className = `status-indicator ${status}`;
        switch (status) {
            case 'connected': this.statusText.textContent = 'Connected'; break;
            case 'connecting': this.statusText.textContent = 'Connecting...'; break;
            case 'disconnected': this.statusText.textContent = 'Disconnected'; break;
        }
    }

    handleMessage(data) {
        console.log('📨 Received WebSocket message:', data);

        switch (data.type) {
            case 'auth_response':
                console.log('🔐 Auth response received:', data);
                // Re-enable auth button on response
                this.authSubmitBtn.disabled = false;
                this.authSubmitBtn.classList.remove('loading');
                this.authSubmitText.textContent = this.authMode === 'register' ? 'Sign Up' : 'Sign In';
                if (data.success) {
                    this.clearAuthError();
                    this.onAuthSuccess();
                } else {
                    this.showAuthError(data.error || 'Authentication failed');
                }
                break;
            case 'hubs':
                this.hubs = data.hubs || [];
                break;
            case 'channels_for_hub':
                this.hubChannels[data.hub_id] = data.channels || [];
                if (this.hubs && this.hubs.length > 0 && this.hubs[0].id === data.hub_id) {
                    const general = (data.channels || []).find(c => c.name === 'general' && c.type === 'text');
                    if (general) {
                        this.currentUserElement.textContent = this.username;
                        this.messageInput.disabled = false;
                        this.sendBtn.disabled = false;
                        this.callBtn.disabled = false;
                        this.showChatInterface();
                        this.updateConnectionStatus('connected');
                        this.requestChannels();
                        this.joinChannel('general');
                        this.setupPeriodicRefresh();
                    }
                }
                break;
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

    onAuthSuccess() { 
        this.addSystemMessage('Authenticated successfully. Fetching your hubs and channels...'); 
        this.clearAuthForm();
    }

    clearAuthForm() {
        this.emailInput.value = '';
        this.passwordInput.value = '';
        this.fullNameInput.value = '';
        this.usernameInput.value = '';
        this.loginUsernameInput.value = '';
        this.clearAuthError();
    }

    handleJoinedMessage(data) {
        this.currentChannel = data.channel;
        this.currentChannelElement.textContent = `# ${data.channel}`;
        this.updateConnectionStatus('connected');
        if (data.channel) {
            this.clearMessages();
            this.addSystemMessage(`Joined channel #${data.channel}`);
            this.requestUsers();
            this.requestChannels();
        } else {
            this.addSystemMessage('Left the channel');
        }
    }

    handleChatMessage(data) { console.log('💬 Handling chat message:', data); this.addMessage(data.sender, data.text, data.timestamp); }
    handleChannelsMessage(data) { console.log('📋 Channels message received:', data); this.channels = data.channels || []; console.log('📋 Updated channels list:', this.channels); this.updateChannelsList(); }
    handleUsersMessage(data) { console.log('👥 Users message received:', data); this.users = data.users || []; console.log('👥 Updated users list:', this.users); this.updateUsersList(); }
    handleUserJoinedMessage(data) { console.log('👤 User joined message:', data); this.addSystemMessage(`${data.username} joined the channel`); }
    handleUserLeftMessage(data) { console.log('👤 User left message:', data); this.addSystemMessage(`${data.username} left the channel`); }
    handleUserDisconnectedMessage(data) { console.log('👤 User disconnected message:', data); this.addSystemMessage(`${data.username} disconnected`); }

    handleCallIncoming(data) {
        this.currentCall = { id: data.call_id, caller: data.from_user, type: data.media_type || 'voice' };
        this.callModalTitle.textContent = 'Incoming Call';
        this.callModalMessage.textContent = `${data.from_user} is calling you...`;
        this.callModal.classList.remove('hidden');
    }

    handleCallAccepted(data) { this.currentCall = { id: data.call_id, state: 'active' }; this.callTitle.textContent = 'Voice Call'; this.callerName.textContent = data.target_user || 'Unknown'; this.showCallInterface(); this.callModal.classList.add('hidden'); this.addSystemMessage(`Call with ${data.target_user} started`); }
    handleCallRejected(data) { this.currentCall = null; this.callModal.classList.add('hidden'); this.hideCallInterface(); this.addSystemMessage(`Call with ${data.target_user} was rejected`); }
    handleCallEnded(data) { this.currentCall = null; this.hideCallInterface(); this.addSystemMessage('Call ended'); }
    handleWebRTCSignal(data) { console.log('WebRTC signal:', data); }

    handleErrorMessage(data) {
        if (this.loginScreen && !this.chatInterface.classList.contains('hidden')) { this.addSystemMessage(`Error: ${data.message}`); }
        else { this.showAuthError(data.message || 'An error occurred'); this.authSubmitBtn.disabled = false; }
    }

    joinChannel(channelName) {
        console.log('joinChannel called:', { channelName, isConnected: this.isConnected, username: this.username });
        if (!this.isConnected) { console.log('joinChannel blocked: not connected'); return; }
        if (this.currentChannel && this.currentChannel !== channelName) { this.clearMessages(); this.addSystemMessage(`Switching to channel #${channelName}...`); }
        const message = this.sendSecure('join', { channel: channelName, username: this.username });
        console.log('Joining channel:', message);
    }

    requestChannels() {
        if (!this.isConnected) return;
        const message = this.sendSecure('list');
        console.log('Requesting channels:', message);
    }

    requestUsers() {
        if (!this.isConnected || !this.currentChannel) return;
        const message = this.sendSecure('users');
        console.log('Requesting users:', message);
    }

    setupPeriodicRefresh() {
        this.channelsRefreshInterval = setInterval(() => { if (this.isConnected) { console.log('🔄 Periodic channels refresh'); this.requestChannels(); } }, 10000);
        this.usersRefreshInterval = setInterval(() => { if (this.isConnected && this.currentChannel) { console.log('🔄 Periodic users refresh'); this.requestUsers(); } }, 5000);
    }

    sendMessage() {
        const text = this.messageInput.value.trim();
        console.log('sendMessage called:', { text, isConnected: this.isConnected, currentChannel: this.currentChannel });
        if (!text || !this.isConnected) { console.log('sendMessage blocked:', { hasText: !!text, isConnected: this.isConnected }); return; }
        const payload = { text, username: this.username, channel: this.currentChannel };
        const message = this.sendSecure('chat', payload);
        this.messageInput.value = '';
        console.log('Sent message:', message);
    }

    handleMessageKeypress(e) { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); this.sendMessage(); } }

    startCall() { const targetUser = prompt('Enter username to call:'); if (!targetUser) return; const message = this.sendSecure('call_request', { target_user: targetUser, media_type: 'voice' }); this.addSystemMessage(`Calling ${targetUser}...`); }
    acceptCall() { if (!this.currentCall) return; const message = this.sendSecure('call_accept', { call_id: this.currentCall.id }); this.callModal.classList.add('hidden'); }
    rejectCall() { if (!this.currentCall) return; const message = this.sendSecure('call_reject', { call_id: this.currentCall.id }); this.callModal.classList.add('hidden'); this.currentCall = null; }
    endCall() { if (!this.currentCall) return; const message = this.sendSecure('call_end', { call_id: this.currentCall.id }); this.hideCallInterface(); this.currentCall = null; }

    toggleMute() { console.log('Toggle mute'); }
    toggleVideo() { console.log('Toggle video'); }
    toggleScreenShare() { console.log('Toggle screen share'); }

    showCreateChannelModal() { this.createChannelModal.classList.remove('hidden'); this.newChannelNameInput.focus(); }
    hideCreateChannelModal() { this.createChannelModal.classList.add('hidden'); this.newChannelNameInput.value = ''; }
    createChannel() { const channelName = this.newChannelNameInput.value.trim(); if (!channelName) return; this.joinChannel(channelName); this.hideCreateChannelModal(); }

    addMessage(sender, text, timestamp) {
        const messageElement = document.createElement('div'); messageElement.className = 'message';
        const avatar = document.createElement('div'); avatar.className = 'message-avatar'; avatar.textContent = sender.charAt(0).toUpperCase();
        const content = document.createElement('div'); content.className = 'message-content';
        const header = document.createElement('div'); header.className = 'message-header';
        const senderElement = document.createElement('span'); senderElement.className = 'message-sender'; senderElement.textContent = sender;
        const timeElement = document.createElement('span'); timeElement.className = 'message-time'; timeElement.textContent = timestamp ? this.formatTime(timestamp) : this.formatTime(Date.now());
        header.appendChild(senderElement); header.appendChild(timeElement);
        const textElement = document.createElement('div'); textElement.className = 'message-text'; textElement.textContent = text;
        content.appendChild(header); content.appendChild(textElement);
        messageElement.appendChild(avatar); messageElement.appendChild(content);
        this.messages.appendChild(messageElement); this.scrollToBottom();
    }

    addSystemMessage(text) { const messageElement = document.createElement('div'); messageElement.className = 'system-message'; messageElement.textContent = text; this.messages.appendChild(messageElement); this.scrollToBottom(); }

    updateChannelsList() {
        console.log('🔄 Updating channels list UI:', this.channels);
        this.channelsList.innerHTML = '';
        this.channels.forEach(channel => {
            const channelElement = document.createElement('div'); channelElement.className = 'channel';
            if (channel === this.currentChannel) { channelElement.classList.add('active'); }
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
            const userElement = document.createElement('div'); userElement.className = 'user'; userElement.textContent = user; userElement.addEventListener('click', () => this.startCallWithUser(user)); this.usersList.appendChild(userElement);
        });
        console.log('🔄 Users list updated, total users:', this.users.length);
    }

    startCallWithUser(username) { if (username === this.username) return; const message = this.sendSecure('call_request', { target_user: username, media_type: 'voice' }); this.addSystemMessage(`Calling ${username}...`); }

    clearMessages() { this.messages.innerHTML = ''; }
    clearChannels() { this.channelsList.innerHTML = ''; this.channels = []; }
    clearUsers() { this.usersList.innerHTML = ''; this.users = []; this.userCount.textContent = '0'; }
    scrollToBottom() { this.messagesContainer.scrollTop = this.messagesContainer.scrollHeight; }

    formatTime(timestamp) { const date = new Date(timestamp); return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }); }
}

export function startApp() { new ChatApp(); } 