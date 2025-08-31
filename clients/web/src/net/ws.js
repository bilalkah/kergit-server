export class WSClient {
    constructor() {
        this.ws = null;
        this.onopen = null;
        this.onmessage = null;
        this.onclose = null;
    }


    connect(url) {
        // If already open/connecting, reuse.
        if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
            return Promise.resolve();
        }
        return new Promise((resolve, reject) => {
            try {
                this.ws = new WebSocket(url);
                this.ws.onopen = () => { this.onopen && this.onopen(); resolve(); };
                this.ws.onmessage = (ev) => {
                    try { const d = JSON.parse(ev.data); this.onmessage && this.onmessage(d); }
                    catch (e) { console.error('Bad JSON', e, ev.data); }
                };
                this.ws.onclose = () => { this.onclose && this.onclose(); };
                this.ws.onerror = (e) => { reject(e); };
            } catch (e) {
                reject(e);
            }
        });
    }


    send(obj) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(obj));
        }
    }


    close() { try { this.ws?.close(); } catch { } }
}


export function generateId() {
    if (crypto && crypto.getRandomValues) {
        const buf = new Uint8Array(16);
        crypto.getRandomValues(buf);
        buf[6] = (buf[6] & 0x0f) | 0x40; // v4
        buf[8] = (buf[8] & 0x3f) | 0x80; // variant
        const hex = [...buf].map(b => b.toString(16).padStart(2, '0'));
        return (
            hex[0] + hex[1] + hex[2] + hex[3] + '-' +
            hex[4] + hex[5] + '-' + hex[6] + hex[7] + '-' +
            hex[8] + hex[9] + '-' + hex[10] + hex[11] + hex[12] + hex[13] + hex[14] + hex[15]
        );
    }
    return (Date.now().toString(36) + Math.random().toString(36).slice(2, 10)).slice(0, 16);
}


export const nowSeconds = () => Math.floor(Date.now() / 1000);


export function sendSecure(wsClient, type, payload = {}) {
    const base = { type, id: generateId(), timestamp: nowSeconds() };
    wsClient.send({ ...base, ...payload });
}