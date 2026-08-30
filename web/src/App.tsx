import { useEffect } from 'react';
import { BrowserRouter, Navigate, Route, Routes, useNavigate } from 'react-router-dom';
import ChatPage from './pages/ChatPage';
import { rest } from './api/rest';
import { useAuthStore } from './stores/auth';

/// 根路径:加载角色列表后跳到第一个角色(角色选择由侧边栏承担)
function RedirectToFirstRole() {
  const navigate = useNavigate();
  useEffect(() => {
    rest
      .roles()
      .then((r) => navigate(`/chat/${r.roles[0] ?? 'default'}`, { replace: true }))
      .catch(() => navigate('/chat/default', { replace: true }));
  }, [navigate]);
  return <div className="app-loading">Prosophor …</div>;
}

export default function App() {
  const init = useAuthStore((s) => s.init);
  const token = useAuthStore((s) => s.token);
  const ready = useAuthStore((s) => s.ready);

  useEffect(() => {
    if (!ready) init();
  }, [ready, init]);

  if (!ready) {
    return <div className="app-loading">Prosophor …</div>;
  }

  return (
    <BrowserRouter>
      <Routes>
        <Route path="/chat/:roleId" element={token ? <ChatPage /> : <Navigate to="/" replace />} />
        {/* 无 token(免注册进行中/失败)→ 显示连接中,不渲染空路由 */}
        <Route path="/" element={token ? <RedirectToFirstRole /> : <div className="app-loading">连接中…</div>} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </BrowserRouter>
  );
}
