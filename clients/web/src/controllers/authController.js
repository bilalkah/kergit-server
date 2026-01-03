// src/controllers/authController.js
import { initSupabase, loginWithPassword, signUpWithPassword } from '../services/supabase.js';
import { sendAuth } from '../api/chatApi.js';
import { actions } from '../store/state.js';

function resolveWebSocketUrl() {
  const defaultLocal = 'ws://localhost:9001';
  if (typeof window === 'undefined' || !window.location) return defaultLocal;

  const host = window.location.hostname || '';
  if (host.endsWith('.devtunnels.ms')) {
    const wsHost = host.includes('-8080') ? host.replace('-8080', '-9001') : host;
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${wsHost}`;
  }

  return defaultLocal;
}

export function wireAuth({ ws, els, config }) {
  const {
    // screens
    loginScreen, chatScreen,
    // form + inputs
    loginForm, email, password, username, fullName, serverUrl,
    signupFields = [],
    switchToSignup, switchToSignin, toggleToSignup, toggleToSignin,
    // button + status
    submitBtn, submitText, authError,
    // logout + header
    disconnectBtn, currentUser
  } = els;

  let authMode = 'signin';
  const idleLabel = () => (authMode === 'signup' ? 'Sign Up' : 'Sign In');
  const busyLabel = () => (authMode === 'signup' ? 'Signing Up…' : 'Signing In…');

  const setBusy = (b) => {
    if (submitBtn) submitBtn.disabled = !!b;
    if (submitText) submitText.textContent = b ? busyLabel() : idleLabel();
  };
  const setError = (msg) => { if (authError) { authError.textContent = msg || ''; authError.style.display = msg ? 'block' : 'none'; } };
  const refreshAuthMode = () => {
    signupFields.forEach((el) => el.classList.toggle('hidden', authMode !== 'signup'));
    toggleToSignup?.classList.toggle('hidden', authMode === 'signup');
    toggleToSignin?.classList.toggle('hidden', authMode === 'signin');
    setBusy(false);
  };
  const setAuthMode = (mode) => {
    if (!mode || mode === authMode) return;
    authMode = mode;
    setError('');
    refreshAuthMode();
  };

  refreshAuthMode();

  switchToSignup?.addEventListener('click', (e) => { e.preventDefault(); setAuthMode('signup'); });
  switchToSignin?.addEventListener('click', (e) => { e.preventDefault(); setAuthMode('signin'); });

  // Make Enter key submit the same handler
  loginForm?.addEventListener('submit', (e) => { e.preventDefault(); submitBtn?.click(); });

  submitBtn?.addEventListener('click', async () => {
    setError('');
    els.connectionLostModal?.classList.add('hidden');

    const mode = authMode;
    const em = email?.value?.trim() || '';
    const pw = password?.value?.trim() || '';
    const resolvedWsUrl = resolveWebSocketUrl();
    const url = (serverUrl?.value?.trim()) || window.CONFIG?.WS_URL || resolvedWsUrl;
    const usernameValue = username?.value?.trim();
    const rawName = mode === 'signup' ? usernameValue : '';
    const full = fullName?.value?.trim();

    if (!em || !pw) return setError('Email and Password are required');
    if (mode === 'signup' && !usernameValue) return setError('Username is required to Sign Up');

    try {
      setBusy(true);
      actions.setConnection('connecting');

      initSupabase(config.SUPABASE_URL, config.SUPABASE_ANON);
      let supaResult;
      if (mode === 'signup') {
        const metadata = {
          username: usernameValue,
          full_name: full || usernameValue || '',
          name: full || usernameValue || ''
        };
        supaResult = await signUpWithPassword(em, pw, metadata);
      } else {
        supaResult = await loginWithPassword(em, pw);
      }
      const { token, user } = supaResult;

      let connected = false;
      let connectErr = null;
      for (let attempt = 1; attempt <= 2 && !connected; attempt += 1) {
        try {
          await ws.connect(url);
          connected = true;
        } catch (err) {
          connectErr = err;
          if (attempt < 2) await new Promise((r) => setTimeout(r, 300));
        }
      }
      if (!connected) throw connectErr || new Error('WebSocket connection failed');

      const userMeta = user?.user_metadata || {};
      const autoName =
        rawName ||
        userMeta.username ||
        userMeta.full_name ||
        userMeta.name ||
        (user?.email ? user.email.split('@')[0] : '') ||
        `Member-${Math.random().toString(36).slice(2, 6)}`;

      const finalName = autoName.trim() || `Member-${Math.random().toString(36).slice(2, 6)}`;

      let res;
      let authError = null;
      for (let attempt = 1; attempt <= 2; attempt += 1) {
        try {
          res = await sendAuth(ws, token, finalName);
          break;
        } catch (err) {
          authError = err;
          if (err?.code !== 'AUTH_TIMEOUT' || attempt === 2) throw err;
          await new Promise((r) => setTimeout(r, 300));
        }
      }

      if (!res.success) {
        throw new Error('Authentication failed');
      }

      loginScreen?.classList.add('hidden');
      chatScreen?.classList.remove('hidden');
      if (disconnectBtn) disconnectBtn.disabled = false;
      if (currentUser) currentUser.textContent = finalName;

      actions.setConnection('connected');
      actions.setAuth(true, {
        id: user?.id,
        publicId: res?.user_id || null,
        email: em,
        username: finalName,
        fullName: userMeta.full_name || null,
        displayName: finalName
      });
      actions.setHubCount(typeof res?.hub_count === 'number' ? res.hub_count : 0);
      if (Array.isArray(res?.hubs)) {
        actions.setList({
          hubs: res.hubs,
          channels_by_hub: res.channels_by_hub || {}
        });
        actions.setHubPresenceMap(res.members_by_hub || {});
      }
      actions.setSession({ url, token, username: finalName });
    } catch (err) {
      ws.disconnect?.(4001, 'auth failed');
      actions.setConnection('disconnected');
      actions.setAuth(false);
      actions.setSession(null);
      const msg = err?.message || 'Authentication failed';
      const code = err?.code ? ` (${err.code})` : '';
      setError(`${msg}${code}`);
      if (disconnectBtn) disconnectBtn.disabled = true;
    } finally {
      setBusy(false);
    }
  });
}
