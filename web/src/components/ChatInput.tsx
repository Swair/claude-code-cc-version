import { useState } from 'react';

interface Props {
  disabled?: boolean;
  streaming?: boolean;
  onSend: (text: string) => void;
  onStop: () => void;
}

export default function ChatInput({ disabled, streaming, onSend, onStop }: Props) {
  const [text, setText] = useState('');

  const submit = () => {
    if (!text.trim() || streaming) return;
    onSend(text);
    setText('');
  };

  return (
    <div className="chat-input">
      <textarea
        value={text}
        placeholder={streaming ? 'AI 正在回复…' : '输入消息,Enter 发送,Shift+Enter 换行'}
        disabled={disabled || streaming}
        onChange={(e) => setText(e.target.value)}
        onKeyDown={(e) => {
          if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            submit();
          }
        }}
        rows={2}
      />
      <div className="chat-input-actions">
        {streaming ? (
          <button className="btn btn-danger" onClick={onStop}>
            停止
          </button>
        ) : (
          <button className="btn btn-primary" onClick={submit} disabled={disabled || !text.trim()}>
            发送
          </button>
        )}
      </div>
    </div>
  );
}
