import type { ClientFrame, ServerFrame } from '../types/protocol';

type FrameHandler = (frame: ServerFrame) => void;
type StatusHandler = (connected: boolean) => void;

const RECONNECT_BASE_MS = 1000;
const RECONNECT_MAX_MS = 15000;

/// WS 单例:自动重连(指数退避)、按 type 分发帧到订阅者。
/// 认证 Phase 1 免登录;Phase 2 起在 query 带 token。
class WsClient {
  private ws: WebSocket | null = null;
  private handlers = new Set<FrameHandler>();
  private statusHandlers = new Set<StatusHandler>();
  private connected = false;
  private manualClose = false;
  private reconnectDelay = RECONNECT_BASE_MS;
  private url = '';

  /**
   * identity:device_id(免注册,优先)或 token(既有会话)
   * 幂等:同 URL 已连接/连接中 → 直接返回(欢迎帧换发的新 token 会触发
   * 上层 login → 组件 effect 重跑,这里不得再开新连接,否则死循环);
   * 身份/URL 变化 → 先关旧连接(抑制其重连回调)再新开。
   */
  connect(identity: { deviceId?: string; token?: string } = {}) {
    this.manualClose = false;
    const scheme = location.protocol === 'https:' ? 'wss' : 'ws';
    const qs = identity.deviceId
      ? `?device_id=${encodeURIComponent(identity.deviceId)}`
      : identity.token
        ? `?token=${encodeURIComponent(identity.token)}`
        : '';
    const url = `${scheme}://${location.host}/ws${qs}`;
    const openOrPending =
      this.ws &&
      (this.ws.readyState === WebSocket.OPEN ||
        this.ws.readyState === WebSocket.CONNECTING);
    if (openOrPending && this.url === url) return;
    if (this.ws && this.url !== url) {
      // 旧连接交给新身份接管:清回调防重连,再关闭
      this.ws.onopen = null;
      this.ws.onmessage = null;
      this.ws.onclose = null;
      this.ws.onerror = null;
      this.ws.close();
      this.ws = null;
    }
    this.url = url;
    this.open();
  }

  private open() {
    const ws = new WebSocket(this.url);
    this.ws = ws;

    ws.onopen = () => {
      this.connected = true;
      this.reconnectDelay = RECONNECT_BASE_MS;
      this.notifyStatus(true);
    };

    ws.onmessage = (ev) => {
      try {
        const frame = JSON.parse(ev.data as string) as ServerFrame;
        for (const h of this.handlers) h(frame);
      } catch {
        // 忽略非 JSON 帧
      }
    };

    ws.onclose = () => {
      this.connected = false;
      this.notifyStatus(false);
      if (!this.manualClose) {
        setTimeout(() => this.open(), this.reconnectDelay);
        this.reconnectDelay = Math.min(this.reconnectDelay * 2, RECONNECT_MAX_MS);
      }
    };

    ws.onerror = () => {
      ws.close();
    };
  }

  /** 发送客户端帧;未连接时静默丢弃(重连后由页面重新同步)。 */
  send(frame: ClientFrame) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(frame));
    }
  }

  isConnected() {
    return this.connected;
  }

  onFrame(handler: FrameHandler): () => void {
    this.handlers.add(handler);
    return () => this.handlers.delete(handler);
  }

  onStatus(handler: StatusHandler): () => void {
    this.statusHandlers.add(handler);
    return () => this.statusHandlers.delete(handler);
  }

  close() {
    this.manualClose = true;
    this.ws?.close();
  }

  private notifyStatus(connected: boolean) {
    for (const h of this.statusHandlers) h(connected);
  }
}

export const wsClient = new WsClient();
