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
    submitBtn, submitText, authError
  } = els;

  const setBusy  = (b) => { if (submitBtn) submitBtn.disabled = !!b; if (submitText) submitText.textContent = b ? 'Signing In…' : 'Sign In'; };
  const setError = (msg) => { if (authError) { authError.textContent = msg || ''; authError.style.display = msg ? 'block' : 'none'; } };

  // Make Enter key submit the same handler
  loginForm?.addEventListener('submit', (e) => { e.preventDefault(); submitBtn?.click(); });

  console.log('[WIRE] login handler attached');
  submitBtn?.addEventListener('click', async () => {
    console.log('[CLICK] login button clicked');
    setError('');

    const em  = email?.value?.trim() || '';
    const pw  = password?.value?.trim() || '';
    // serverUrl is OPTIONAL in your HTML; fall back gracefully
    const url = (serverUrl?.value?.trim()) || (window.CONFIG?.WS_URL) || 'ws://localhost:9001';
    const un  = (username?.value?.trim()) || (em.split('@')[0] || 'user');

    if (!em || !pw) return setError('Email and Password are required');

    try {
      setBusy(true);
      actions.setConnection('connecting');

      initSupabase(config.SUPABASE_URL, config.SUPABASE_ANON);
      const { token, user } = await loginWithPassword(em, pw);

      console.log('[WS] connecting →', url);
      await ws.connect(url);
      console.log('[WS] connected');

      sendAuth(ws, token, un);
      console.log('[WS=>] auth sent');

      loginScreen?.classList.add('hidden');
      chatScreen?.classList.remove('hidden');

      actions.setConnection('connected');
      actions.setAuth(true, { id: user?.id, email: em, username: un });
    } catch (e) {
      console.error('[AUTH] failed', e);
      actions.setConnection('disconnected');
      setError(e?.message || 'Auth failed');
    } finally {
      setBusy(false);
    }
  });
}
