import { memo } from 'react';
import type { ChatMessage } from '../stores/chat';

function formatTime(ts: number): string {
  const d = new Date(ts);
  const hh = String(d.getHours()).padStart(2, '0');
  const mm = String(d.getMinutes()).padStart(2, '0');
  return `${hh}:${mm}`;
}

function MessageBubble({ msg }: { msg: ChatMessage }) {
  if (msg.role === 'user') {
    return (
      <div className="msg msg-user">
        <div className="bubble bubble-user">
          {msg.content}
          <span className="tail tail-user" />
        </div>
        <span className="msg-time">{formatTime(msg.ts)}</span>
      </div>
    );
  }
  return (
    <div className="msg msg-assistant">
      <span className="msg-time">{formatTime(msg.ts)}</span>
      <div className={`bubble bubble-assistant ${msg.streaming ? 'streaming' : ''}`}>
        {msg.content || '…'}
        {msg.streaming && <span className="cursor">▍</span>}
        <span className="tail tail-assistant" />
      </div>
    </div>
  );
}

export default memo(function MessageList({ messages }: { messages: ChatMessage[] }) {
  if (messages.length === 0) {
    return (
      <div className="chat-empty">
        与 AI 智能体开始对话吧
        <br />
        <small>支持工具调用、记忆与多角色</small>
      </div>
    );
  }
  return (
    <>
      {messages.map((m) => (
        <MessageBubble key={m.id} msg={m} />
      ))}
    </>
  );
});
