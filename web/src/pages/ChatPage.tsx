import { useEffect, useMemo, useRef, useState } from 'react';
import { useParams } from 'react-router-dom';
import { rest } from '../api/rest';
import { wsClient } from '../api/ws';
import { useAuthStore, getOrCreateDeviceId } from '../stores/auth';
import { useChatStore } from '../stores/chat';
import MessageList from '../components/MessageList';
import ChatInput from '../components/ChatInput';
import RoleSidebar from '../components/RoleSidebar';

const STATE_LABEL: Record<string, string> = {
  streaming: '正在输入…',
  thinking: '思考中…',
  working: '调用工具…',
  waiting_permission: '等待授权…',
  error: '出错了',
};

export default function ChatPage() {
  const { roleId } = useParams();
  const token = useAuthStore((s) => s.token);
  const user = useAuthStore((s) => s.user);
  // 用户 × 角色 独立会话:chat_id = p2p_{uid}_{role}
  const chatKey = user && roleId ? `p2p_${user.user_id}_${roleId}` : '';

  const messages = useChatStore((s) => s.messages);
  const state = useChatStore((s) => s.state);
  const stateMsg = useChatStore((s) => s.stateMsg);
  const error = useChatStore((s) => s.error);
  const handleFrame = useChatStore((s) => s.handleFrame);
  const loadHistory = useChatStore((s) => s.loadHistory);
  const sendMessage = useChatStore((s) => s.sendMessage);
  const stop = useChatStore((s) => s.stop);
  const reset = useChatStore((s) => s.reset);

  const [connected, setConnected] = useState(false);
  const listRef = useRef<HTMLDivElement>(null);

  // 连接 + 帧分发 + 历史回填(每次 chatKey 变化重建会话上下文)
  useEffect(() => {
    if (!chatKey) return;
    // 免注册:WS 握手带 device_id,服务端自动建号并在 welcome 帧返回 token
    wsClient.connect({ deviceId: getOrCreateDeviceId(), token: token ?? undefined });
    const offFrame = wsClient.onFrame(handleFrame);
    const offStatusSub = wsClient.onStatus(setConnected);

    // 切换角色/刷新:查会话 → 加载历史(无历史会话则空开始)
    rest
      .sessions()
      .then((r) => r.sessions.find((s) => s.chat_id === chatKey))
      .then((s) => (s ? rest.messages(s.session_id) : null))
      .then((h) => {
        if (h) loadHistory(h.messages);
      })
      .catch(() => {});

    return () => {
      offFrame();
      offStatusSub();
      reset();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [chatKey, token]);

  // 自动滚动到底部
  useEffect(() => {
    listRef.current?.scrollTo({ top: listRef.current.scrollHeight });
  }, [messages]);

  const statusLine = useMemo(() => {
    if (!connected) return '连接中…';
    if (state !== 'idle') {
      const label = STATE_LABEL[state] ?? state;
      return stateMsg ? `${label} ${stateMsg}` : label;
    }
    return '';
  }, [connected, state, stateMsg]);

  return (
    <div className="app-shell">
      <RoleSidebar currentRoleId={roleId ?? ''} />
      <div className="chat-page">
        <header className="chat-header">
          <span className="chat-title">{roleId ?? 'AI 智能体'}</span>
          <span className="chat-status">{statusLine}</span>
          <span className={`dot ${connected ? 'on' : ''}`} title={connected ? '已连接' : '未连接'} />
        </header>

        <div className="chat-list" ref={listRef}>
          <MessageList messages={messages} />
          {error && <div className="chat-error">{error}</div>}
        </div>

        <footer className="chat-footer">
          <ChatInput
            disabled={!connected}
            onSend={(text) => sendMessage(chatKey, text)}
            onStop={() => stop(chatKey)}
            streaming={state === 'streaming' || state === 'thinking' || state === 'working'}
          />
        </footer>
      </div>
    </div>
  );
}
