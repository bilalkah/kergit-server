// src/controllers/authController.js
import { initSupabase, loginWithPassword } from '../services/supabase.js';
import { sendAuth } from '../api/chatApi.js';
import { actions } from '../store/state.js';

export function wireAuth({ ws, els, config }) {
  const {
    // screens
    loginScreen, chatScreen,
    // form + inputs
    loginForm, email, password, username, serverUrl,
    // button + status
    submitBtn, submitText, authError,
    // logout + header
    disconnectBtn, currentUser
  } = els;

  const setBusy = (b) => { if (submitBtn) submitBtn.disabled = !!b; if (submitText) submitText.textContent = b ? 'Signing In…' : 'Sign In'; };
  const setError = (msg) => { if (authError) { authError.textContent = msg || ''; authError.style.display = msg ? 'block' : 'none'; } };

  // Make Enter key submit the same handler
  loginForm?.addEventListener('submit', (e) => { e.preventDefault(); submitBtn?.click(); });

  console.log('[WIRE] login handler attached');
  submitBtn?.addEventListener('click', async () => {
    console.log('[CLICK] login button clicked');
    setError('');
    els.connectionLostModal?.classList.add('hidden');

    const em = email?.value?.trim() || '';
    const pw = password?.value?.trim() || '';
    // serverUrl is OPTIONAL in your HTML; fall back gracefully
    const url = (serverUrl?.value?.trim()) || (window.CONFIG?.WS_URL) || 'ws://localhost:9001';
    const rawName = username?.value?.trim();

    if (!em || !pw) return setError('Email and Password are required');

    try {
      setBusy(true);
      actions.setConnection('connecting');

      initSupabase(config.SUPABASE_URL, config.SUPABASE_ANON);
      const { token, user } = await loginWithPassword(em, pw);

      console.log('[WS] connecting →', url);
      await ws.connect(url);
      console.log('[WS] connected');

      const userMeta = user?.user_metadata || {};
      const autoName =
        rawName ||
        userMeta.username ||
        userMeta.full_name ||
        userMeta.name ||
        (user?.email ? user.email.split('@')[0] : '') ||
        `Member-${Math.random().toString(36).slice(2, 6)}`;

      const finalName = autoName.trim() || `Member-${Math.random().toString(36).slice(2, 6)}`;

      const res = await sendAuth(ws, token, finalName);
      console.log('[WS=>] auth sent');

      if (!res.success) {
        throw new Error('Authentication failed');
      }

      loginScreen?.classList.add('hidden');
      chatScreen?.classList.remove('hidden');
      if (disconnectBtn) disconnectBtn.disabled = false;
      if (currentUser) currentUser.textContent = finalName;

      actions.setConnection('connected');
      actions.setAuth(true, { id: user?.id, email: em, username: finalName });
      actions.setSession({ url, token, username: finalName });
    } catch (err) {
      console.error('❌ Login/auth failed:', err);
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
