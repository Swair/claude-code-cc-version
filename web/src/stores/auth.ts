import { create } from 'zustand';
import { rest } from '../api/rest';

interface AuthUser {
  user_id: string;
  username: string;
  display_name: string;
}

interface AuthState {
  token: string | null;
  user: AuthUser | null;
  ready: boolean;
  login: (token: string, user: AuthUser) => void;
  logout: () => void;
  init: () => Promise<void>;
  reauth: () => Promise<void>;
}

const TOKEN_KEY = 'prosophor_token';
const USER_KEY = 'prosophor_user';
const DEVICE_KEY = 'prosophor_device_id';

/// 设备身份:首次访问生成 UUID(免注册),持久化 localStorage。
export function getOrCreateDeviceId(): string {
  let id = localStorage.getItem(DEVICE_KEY);
  if (!id) {
    id = genDeviceId();
    localStorage.setItem(DEVICE_KEY, id);
  }
  return id;
}

/// 生成设备 ID:crypto.randomUUID 仅在安全上下文可用
/// (HTTPS 或 localhost);http://局域网IP 是非安全上下文,需手动生成。
function genDeviceId(): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID().replace(/-/g, '');
  }
  const rand = () => Math.random().toString(16).slice(2, 10);
  return `${Date.now().toString(16)}${rand()}${rand()}`;
}

/// 读取导航认证下发的 token cookie(浏览器扩展会拦截页面内 fetch,
/// 但不拦页面导航——认证走"导航携带 device_id → 服务器 Set-Cookie"路径)。
export function getCookieToken(): string | null {
  const m = document.cookie.match(/(?:^|;\s*)prosophor_token=([^;]+)/);
  return m ? decodeURIComponent(m[1]) : null;
}

export const useAuthStore = create<AuthState>((set, get) => ({
  token: null,
  user: null,
  ready: false,

  init: async () => {
    const token = localStorage.getItem(TOKEN_KEY);
    const userRaw = localStorage.getItem(USER_KEY);
    if (token && userRaw) {
      try {
        // 先用本地 token;若已失效,REST 401 会 logout → App 重新 init → 走免注册
        set({ token, user: JSON.parse(userRaw), ready: true });
        return;
      } catch {
        localStorage.removeItem(TOKEN_KEY);
        localStorage.removeItem(USER_KEY);
      }
    }
    // 导航认证的 cookie(fetch 可能被浏览器/扩展拦截,导航不会)
    const cookieToken = getCookieToken();
    if (cookieToken) {
      try {
        const me = await rest.me();  // 验证 cookie token 有效并取用户信息
        get().login(cookieToken, me);
        set({ ready: true });
        return;
      } catch {
        // cookie 失效:清掉继续走 deviceAuth
        document.cookie = 'prosophor_token=; Path=/; Max-Age=0';
      }
    }
    // 无 token → 设备免注册:优先导航认证(不依赖 fetch)
    const deviceId = getOrCreateDeviceId();
    if (!location.search.includes('device_id')) {
      // 带 device_id 重新导航 → 服务器 Set-Cookie → 页面重载后走 cookie 路径
      location.replace(`/?device_id=${encodeURIComponent(deviceId)}`);
      return;  // 页面即将重载,不再设置 ready
    }
    // 兜底:导航后仍无 cookie(极端环境)→ 直接 fetch 免注册
    try {
      const res = await rest.deviceAuth(deviceId);
      get().login(res.token, {
        user_id: res.user_id,
        username: res.username,
        display_name: res.display_name,
      });
    } catch {
      // 服务不可达时保持未认证(前端显示连接中)
    }
    set({ ready: true });
  },

  /// 401 登出后调用:清空本地并重新走免注册(旧 token 失效自愈)
  reauth: async () => {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(USER_KEY);
    set({ token: null, user: null });
    try {
      const deviceId = getOrCreateDeviceId();
      const res = await rest.deviceAuth(deviceId);
      get().login(res.token, {
        user_id: res.user_id,
        username: res.username,
        display_name: res.display_name,
      });
    } catch {
      // 重试失败保持未认证(页面显示连接中)
    }
  },

  login: (token, user) => {
    localStorage.setItem(TOKEN_KEY, token);
    localStorage.setItem(USER_KEY, JSON.stringify(user));
    set({ token, user });
  },

  logout: () => {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(USER_KEY);
    set({ token: null, user: null });
  },
}));
