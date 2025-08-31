import { qs } from './dom.js';


const statusDot = () => qs('#connection-status');
const statusText = () => qs('#status-text');


export function setStatus(state) {
    const dot = statusDot();
    const txt = statusText();
    dot.className = `status-indicator ${state}`;
    txt.textContent = state === 'connected' ? 'Connected' : state === 'connecting' ? 'Connecting...' : 'Disconnected';
}