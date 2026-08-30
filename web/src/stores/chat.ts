import { create } from 'zustand';
import type { ServerFrame } from '../types/protocol';
import { wsClient } from '../api/ws';
import { useAuthStore } from './auth';

export interface ChatMessage {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  streaming?: boolean;
  ts: number;
}

export type SessionState =
  | 'idle'
  | 'streaming'
  | 'thinking'
  | 'working'
  | 'waiting_permission'
  | 'error';

interface ChatState {
  messages: ChatMessage[];
  sessionId: string | null;
  roleId: string;
  state: SessionState;
  stateMsg: string;
  error: string;

  handleFrame: (frame: ServerFrame) => void;
  loadHistory: (items: Array<{ role: string; content: string; ts?: string }>) => void;
  sendMessage: (chatId: string, text: string) => void;
  stop: (chatId: string) => void;
  reset: () => void;
}

function genId(): string {
  return `m_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 8)}`;
}

/// 消息桶状态机:idle → (send → user msg) → stream_start 开桶 → delta 追加
/// → reply 终结。thinking 阶段的 delta 与正文 delta 进同一桶(与后端一致)。
export const useChatStore = create<ChatState>((set, get) => ({
  messages: [],
  sessionId: null,
  roleId: '',
  state: 'idle',
  stateMsg: '',
  error: '',

  /// 会话历史回填(切换角色/刷新时):JSONL ts 为 "YYYY-MM-DD HH:MM:SS" 字符串
  /// 接口按"新→旧"返回,聊天界面要"旧→新",这里 reverse
  loadHistory: (items) => {
    const toTs = (s?: string) => {
      if (!s) return Date.now();
      const t = new Date(s.replace(' ', 'T')).getTime();
      return Number.isNaN(t) ? Date.now() : t;
    };
    const msgs: ChatMessage[] = [...items].reverse().map((m, i) => ({
      id: `h_${i}_${Date.now().toString(36)}`,
      role: m.role === 'user' ? 'user' : 'assistant',
      content: m.content,
      streaming: false,
      ts: toTs(m.ts),
    }));
    set({
      messages: msgs,
      state: 'idle',
      stateMsg: '',
      error: '',
    });
  },

  handleFrame: (frame) => {
    switch (frame.type) {
      case 'welcome': {
        // 免注册:服务端签发 token,持久化供 REST 使用
        if (frame.token) {
          useAuthStore.getState().login(frame.token, {
            user_id: frame.user.user_id,
            username: frame.user.username ?? '',
            display_name: frame.user.display_name ?? '',
          });
        }
        break;
      }
      case 'session_ready':
        set({ sessionId: frame.session_id, roleId: frame.role_id });
        break;

      case 'stream_start': {
        // 打开回复桶:最后一个消息不是"进行中的助手消息"时才新建
        const msgs = get().messages;
        const last = msgs[msgs.length - 1];
        if (last && last.role === 'assistant' && last.streaming) return;
        set({
          messages: [
            ...msgs,
            { id: `a_${Date.now()}`, role: 'assistant', content: '', streaming: true, ts: Date.now() },
          ],
        });
        break;
      }

      case 'delta': {
        const msgs = [...get().messages];
        const last = msgs[msgs.length - 1];
        if (last && last.role === 'assistant' && last.streaming) {
          msgs[msgs.length - 1] = { ...last, content: last.content + frame.delta };
        } else if (last && last.role === 'assistant') {
          msgs.push({ id: `a_${Date.now()}`, role: 'assistant', content: frame.delta, streaming: true, ts: Date.now() });
        } else {
          msgs.push({ id: `a_${Date.now()}`, role: 'assistant', content: frame.delta, streaming: true, ts: Date.now() });
        }
        set({ messages: msgs });
        break;
      }

      case 'reply': {
        const msgs = [...get().messages];
        const last = msgs[msgs.length - 1];
        if (last && last.role === 'assistant' && last.streaming) {
          msgs[msgs.length - 1] = { ...last, content: frame.content, streaming: false, ts: frame.ts };
        } else {
          // 非流式 provider 或桶已关闭:追加终态消息
          msgs.push({ id: `a_${frame.ts}`, role: 'assistant', content: frame.content, streaming: false, ts: frame.ts });
        }
        set({ messages: msgs, state: 'idle', stateMsg: '' });
        break;
      }

      case 'state': {
        const stateMap: Record<string, SessionState> = {
          idle: 'idle',
          streaming: 'streaming',
          thinking: 'thinking',
          working: 'working',
          waiting_permission: 'waiting_permission',
          error: 'error',
        };
        set({ state: stateMap[frame.state] ?? 'idle', stateMsg: frame.state_msg ?? '' });
        break;
      }

      case 'error':
        set({ error: frame.message });
        break;

      default:
        break;
    }
  },

  sendMessage: (chatId, text) => {
    const trimmed = text.trim();
    if (!trimmed) return;
    const clientMsgId = genId();
    set((s) => ({
      messages: [...s.messages, { id: clientMsgId, role: 'user', content: trimmed, ts: Date.now() }],
      state: 'idle',
      stateMsg: '',
      error: '',
    }));
    wsClient.send({ type: 'send', chat_id: chatId, text: trimmed, client_msg_id: clientMsgId });
  },

  stop: (chatId) => {
    wsClient.send({ type: 'stop', chat_id: chatId });
    set({ state: 'idle', stateMsg: '' });
  },

  reset: () =>
    set({ messages: [], sessionId: null, roleId: '', state: 'idle', stateMsg: '', error: '' }),
}));
