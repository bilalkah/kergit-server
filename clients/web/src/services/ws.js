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
    this.heartbeatTimer = null;
    this.lastPongAt = 0;
    this.lastPingInfo = null;
    this.heartbeatMs = 2500;
    this.missToleranceMs = 9000; // tolerate ~3 missed heartbeats
    this.outbox = []; // <— NEW (optional)
    this.stalled = false;
    this.manualClose = false;
    this.currentUrl = null;
    this.lastHeaders = undefined;
    this.lastTimeoutMs = 7000;
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
      this.manualClose = false;
      this.currentUrl = url;
      this.lastHeaders = headers;
      this.lastTimeoutMs = timeoutMs;
      const socket = new WebSocket(url);
      this.ws = socket;

      const onMessage = (ev) => {
        if (this.ws !== socket) return;
        let msg;
        console.log('[WS] recv', ev.data);
        try { msg = JSON.parse(ev.data); } catch { return; }

        if (msg.type === "conn_status") {
          this._onConnStatus(msg);
          return;
        }

        if (msg?.type === 'ping') {
          const receivedAt = Date.now();
          this.lastPongAt = receivedAt;
          this.stalled = false;
          const latencyMs = typeof msg.ts === 'number' ? Math.max(0, receivedAt - msg.ts) : null;
          this.lastPingInfo = { latencyMs, serverTs: msg?.ts ?? null, receivedAt };
          this.send({ type: 'pong', ts: msg?.ts ?? receivedAt });
          this._emit('__ping__', this.lastPingInfo);
        }
        this._emit('*', msg);
        if (msg?.type) this._emit(msg.type, msg);
      };

      const onOpen = () => {
        if (this.ws !== socket) return;
        if (settled) return;
        settled = true;
        this._emit('__open__');
        this.stalled = false;
        this.lastPingInfo = null;
        this._startHeartbeat();
        this._flushOutbox();
        this._emit('__ping__', null);
        clearTimeout(timer);
        resolve();
      };

      const onError = (e) => {
        if (this.ws !== socket) return;
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        reject(e instanceof Error ? e : new Error('WebSocket error'));
      };

      const onClose = (event) => {
        const isCurrent = this.ws === socket;
        const wasManual = this.manualClose && isCurrent;
        if (isCurrent) {
          this.manualClose = false;
          this._stopHeartbeat();
          this.stalled = false;
          this.lastPingInfo = null;
          this.lastPongAt = 0;
          this.ws = null;
        }
        this._emit('__close__', { manual: wasManual, code: event?.code, reason: event?.reason });
        if (isCurrent) this._emit('__ping__', null);
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
      socket.addEventListener('message', onMessage);
      socket.addEventListener('open', onOpen);
      socket.addEventListener('error', onError);
      socket.addEventListener('close', onClose);
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

  disconnect(code = 1000, reason = 'client disconnect') {
    if (!this.ws) return;
    this.manualClose = true;
    try {
      this.ws.close(code, reason);
    } catch (err) {
      console.warn('[WS] disconnect error', err);
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

  _flushOutbox() {
    if (!this.ws || this.ws.readyState !== 1) return;
    while (this.outbox.length) {
      const next = this.outbox.shift();
      try {
        this.ws.send(JSON.stringify(next));
      } catch (err) {
        console.warn('[WS] failed to flush queued message', err);
        break;
      }
    }
  }

  _onConnStatus({ status, rtt_ms }) {
    const now = Date.now();
    this.lastStatusAt = now;
    this.lastRttMs = typeof rtt_ms === "number" ? rtt_ms : this.lastRttMs;

    if (status === "alive") {
      if (this.stalled) {
        this.stalled = false;
        this._emit("__unstalled__", { rtt_ms: this.lastRttMs });
      }
      this._emit("__ping__", {
        latencyMs: this.lastRttMs,
        serverTs: null,
        receivedAt: now
      });
    } else if (status === "stale") {
      if (!this.stalled) {
        this.stalled = true;
        this._emit("__stalled__", { missFor: now - (this.lastStatusAt ?? now) });
      }
    }
  }

  _startHeartbeat() {
    this.lastStatusAt = Date.now();
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);

    this.heartbeatTimer = setInterval(() => {
      const now = Date.now();
      const sinceStatus = now - this.lastStatusAt;
      const isHidden = typeof document !== "undefined" && document.hidden;

      // if we haven't heard any conn_status for too long
      if (sinceStatus > this.missToleranceMs) {
        if (!this.stalled) {
          this.stalled = true;
          this._emit("__stalled__", { missFor: sinceStatus });
        }

        // if tab hidden, don't spam-stall; tolerate backgrounding
        if (isHidden) {
          this.lastStatusAt = now;
        }
        return;
      }

      if (this.stalled && sinceStatus <= this.missToleranceMs) {
        this.stalled = false;
        this._emit("__unstalled__", { rtt_ms: this.lastRttMs });
      }
    }, this.heartbeatMs);
  }

  _stopHeartbeat() {
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    this.heartbeatTimer = null;
    this.stalled = false;
  }

  _startHubList() {
    // send subscribe to hubs list
    this.send({ type: 'list', channel: 'hubs_list' });
  }
}
