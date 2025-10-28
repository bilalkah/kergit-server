// src/views/status.js
export function renderStatus(rootDot, rootText, connection) {
  if (rootDot)  rootDot.className = `status-indicator ${connection}`;
  if (rootText) rootText.textContent =
    connection === 'connected' ? 'Connected' :
    connection === 'connecting' ? 'Connecting…' : 'Disconnected';
}
