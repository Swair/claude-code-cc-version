// WS 协议类型 —— 与后端 main_src/im_gateway/web_protocol.{h,cc} 一一对应

// ── 客户端 → 服务端 ──
export type ClientFrame =
  | { type: 'hello'; token?: string; client?: string }
  | { type: 'send'; chat_id: string; text: string; client_msg_id: string }
  | { type: 'stop'; chat_id: string }
  | { type: 'ping'; t?: string }
  | { type: 'get_online'; chat_id: string };

// ── 服务端 → 客户端 ──
export interface WelcomeUser {
  user_id: string;
  username?: string;
  display_name?: string;
}
export interface OnlineMember {
  user_id: string;
  display_name?: string;
}

export type ServerFrame =
  | { type: 'welcome'; user: WelcomeUser; server_ts: number; token?: string }
  | {
      type: 'ack';
      client_msg_id: string;
      chat_id: string;
      session_id: string;
      ts: number;
    }
  | {
      type: 'session_ready';
      chat_id: string;
      session_id: string;
      role_id: string;
      created: boolean;
    }
  | { type: 'stream_start'; chat_id: string; session_id: string; role_id: string }
  | { type: 'delta'; chat_id: string; session_id: string; delta: string }
  | {
      type: 'state';
      chat_id: string;
      session_id: string;
      state: string; // idle | working | streaming | thinking | waiting_permission | error
      state_msg?: string;
    }
  | { type: 'reply'; chat_id: string; session_id: string; content: string; ts: number }
  | { type: 'error'; code: string; message: string; chat_id?: string }
  | { type: 'pong'; t?: string }
  | {
      type: 'online';
      chat_id: string;
      members: OnlineMember[];
      online_ids: string[];
    }
  | { type: 'presence'; chat_id: string; user_id: string; online: boolean };
