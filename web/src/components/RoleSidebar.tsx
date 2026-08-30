import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { rest } from '../api/rest';
import { useAuthStore } from '../stores/auth';

/// 角色 emoji 映射(展示用;无匹配用 🤖)
const ROLE_EMOJI: Record<string, string> = {
  default: '🤖',
  ayaka: '❄️',
  kazuha: '🍁',
  keqing: '⚡',
  sayu: '🍃',
  'skirk-2': '🗡️',
  'linnea-2': '🌹',
  architect: '🏗️',
  coder: '💻',
  reviewer: '🔍',
  teacher: '📚',
  disk_cleaner: '🧹',
  skills_creator: '🛠️',
};

/// 左侧角色侧边栏:常驻,点击切换 /chat/:roleId;底部用户信息 + 退出。
export default function RoleSidebar({ currentRoleId }: { currentRoleId: string }) {
  const navigate = useNavigate();
  const user = useAuthStore((s) => s.user);
  const logout = useAuthStore((s) => s.logout);
  const [roles, setRoles] = useState<string[]>([]);

  useEffect(() => {
    rest.roles().then((r) => setRoles(r.roles)).catch(() => {});
  }, []);

  return (
    <aside className="sidebar">
      <div className="sidebar-title">Prosophor</div>
      <div className="sidebar-list">
        {roles.map((role) => (
          <div
            key={role}
            className={`sidebar-item ${role === currentRoleId ? 'active' : ''}`}
            onClick={() => navigate(`/chat/${role}`)}
          >
            <span className="sidebar-emoji">{ROLE_EMOJI[role] ?? '🤖'}</span>
            <span className="sidebar-name">{role}</span>
          </div>
        ))}
      </div>
      <div className="sidebar-user">
        <span>{user?.display_name ?? user?.username}</span>
        <button
          className="btn-link"
          onClick={() => {
            logout();
            window.location.reload();
          }}
        >
          退出
        </button>
      </div>
    </aside>
  );
}
