// src/services/ws.js
export class WSClient {
  // Add this helper once in the class
  once(type) {
    return new Promise((resolve) => {
      const off = this.on(type, (payload) => { off(); resolve(payload); });
    });
  }

  constructor() {
    this.ws = null;
    this.handlers = new Map(); // type -> Set<fn>
    this.anyHandlers = new Set(); // raw handler for all messages
    this.pingInterval = null;
    this.lastPongAt = 0;
    this.heartbeatMs = 2500;
    this.missToleranceMs = 60000; // ~2 missed pings
    this.outbox = []; // <— NEW (optional)
  }


  /**
   * Awaitable connect that preserves existing behavior:
   * - emits '__open__'
   * - starts heartbeat
   * - starts hub list
   * - routes messages & pong tracking
   * - emits '__close__'
   */
  connect(url, { headers, timeoutMs = 7000 } = {}) {
    return new Promise((resolve, reject) => {
      let settled = false;
      this.ws = new WebSocket(url);

      const onMessage = (ev) => {
        console.log('[WS<= RAW]', ev.data);
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        if (msg?.type === 'pong') this.lastPongAt = Date.now();
        this._emit('*', msg);
        if (msg?.type) this._emit(msg.type, msg);
      };

      const onOpen = () => {
        if (settled) return;
        settled = true;
        this._emit('__open__');
        this._startHeartbeat();
        this._startHubList();
        clearTimeout(timer);
        resolve();
      };

      const onError = (e) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        reject(e instanceof Error ? e : new Error('WebSocket error'));
      };

      const onClose = () => {
        this._stopHeartbeat();
        this._emit('__close__');
        if (!settled) {
          settled = true;
          clearTimeout(timer);
          reject(new Error('WebSocket closed before open'));
        }
      };

      const timer = setTimeout(() => {
        if (settled) return;
        settled = true;
        try { this.ws.close(); } catch { }
        reject(new Error('WS connect timeout'));
      }, timeoutMs);

      // Attach all listeners once, permanently for this connection
      this.ws.addEventListener('message', onMessage);
      this.ws.addEventListener('open', onOpen);
      this.ws.addEventListener('error', onError);
      this.ws.addEventListener('close', onClose);
    });
  }


  send(obj) {
    if (this.ws && this.ws.readyState === 1) {
      try { this.ws.send(JSON.stringify(obj)); } catch { }
    } else {
      // queue until open (optional safety)
      this.outbox.push(obj);
    }
  }

  on(type, fn) {
    if (type === '*') { this.anyHandlers.add(fn); return () => this.anyHandlers.delete(fn); }
    if (!this.handlers.has(type)) this.handlers.set(type, new Set());
    const set = this.handlers.get(type);
    set.add(fn);
    return () => set.delete(fn);
  }

  _emit(type, payload) {
    if (type === '*') { this.anyHandlers.forEach(fn => fn(payload)); return; }
    const set = this.handlers.get(type);
    if (set) set.forEach(fn => fn(payload));
  }

  _startHeartbeat() {
    this.lastPongAt = Date.now();
    if (this.pingInterval) clearInterval(this.pingInterval);
    this.pingInterval = setInterval(() => {
      // send ping
      this.send({ type: 'ping', ts: Date.now() });
      // check miss
      const missFor = Date.now() - this.lastPongAt;
      if (missFor > this.missToleranceMs) {
        this._emit('__stalled__', { missFor });
      }
    }, this.heartbeatMs);
  }

  _stopHeartbeat() {
    if (this.pingInterval) clearInterval(this.pingInterval);
    this.pingInterval = null;
  }

  _startHubList() {
    // send subscribe to hubs list
    this.send({ type: 'list', channel: 'hubs_list' });
  }
}
