// clients/web/src/ui/login.js
import { qs } from './dom.js';

export function mountLoginUI() {
  const root = qs('#login-screen');
  const form = qs('#login-form');
  const email = qs('#email');
  const password = qs('#password');
  const username = qs('#username');
  const serverUrl = qs('#server-url');
  const submitBtn = qs('#auth-submit-btn');
  const submitText = qs('#auth-submit-text');
  const errorBanner = qs('#auth-error');

  function show() { root.classList.remove('hidden'); }
  function hide() { root.classList.add('hidden'); }

  function setError(msg) {
    errorBanner.textContent = msg || '';
    errorBanner.style.display = msg ? 'block' : 'none';
  }

  function setBusy(busy) {
    submitBtn.disabled = !!busy;
    submitText.textContent = busy ? 'Signing In…' : 'Sign In';
  }

  function getValues() {
    return {
      email: email.value.trim(),
      password: password.value.trim(),
      username: username.value.trim(), // optional display name
      serverUrl: serverUrl.value.trim(),
    };
  }

  // Prevent HTML form submit
  form.addEventListener('submit', (e) => e.preventDefault());

  function onSubmit(handler) {
    submitBtn.addEventListener('click', () => handler(getValues()));
  }

  return { show, hide, onSubmit, setError, setBusy };
}
