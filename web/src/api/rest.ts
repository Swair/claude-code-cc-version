import { useAuthStore } from '../stores/auth';

/// REST 客户端:自动带 Bearer token,401 时登出并跳登录页。
export interface ApiError {
  code: string;
  message: string;
}

export class ApiException extends Error {
  status: number;
  code: string;
  constructor(status: number, code: string, message: string) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const token = useAuthStore.getState().token;
  const headers: Record<string, string> = {
    'content-type': 'application/json',
    ...(options.headers as Record<string, string>),
  };
  if (token) headers['authorization'] = `Bearer ${token}`;

  const res = await fetch(`/api${path}`, { ...options, headers });
  if (res.status === 401 && path !== '/auth/device') {
    // 旧 token 失效:清本地 + 重新免注册(自愈;多次并发 401 只触发一次)
    const auth = useAuthStore.getState();
    if (auth.token) {
      auth.logout();
      auth.reauth();
    }
    throw new ApiException(401, 'unauthorized', '登录已过期');
  }
  if (res.status === 204) return undefined as T;
  const body = await res.json().catch(() => null);
  if (!res.ok) {
    const err = body?.error;
    throw new ApiException(res.status, err?.code ?? 'error', err?.message ?? res.statusText);
  }
  return body as T;
}

export const rest = {
  // GET 免注册:部分浏览器/网络对局域网 IP 的 fetch POST 不发,GET 全通
  deviceAuth: (deviceId: string) =>
    request<{ token: string; user_id: string; username: string; display_name: string }>(
      `/auth/device?device_id=${encodeURIComponent(deviceId)}`,
    ),
  me: () => request<{ user_id: string; username: string; display_name: string }>('/me'),
  roles: () => request<{ roles: string[] }>('/roles'),
  sessions: () =>
    request<{
      sessions: Array<{
        session_id: string;
        chat_id: string;
        chat_type: string;
        role_id: string;
      }>;
    }>('/sessions'),
  messages: (sessionId: string, limit = 50) =>
    request<{
      session_id: string;
      messages: Array<{ role: string; content: string; sender?: string; ts?: string }>;
    }>(`/sessions/${sessionId}/messages?limit=${limit}`),
  groups: () =>
    request<{
      groups: Array<{
        group_id: string;
        name: string;
        description: string;
        role_id: string;
        owner_id: string;
        member_count: number;
      }>;
    }>('/groups'),
  createGroup: (name: string, description: string, roleId: string) =>
    request<{ group_id: string }>('/groups', {
      method: 'POST',
      body: JSON.stringify({ name, description, role_id: roleId }),
    }),
  groupDetail: (gid: string) =>
    request<{
      group_id: string;
      name: string;
      description: string;
      role_id: string;
      owner_id: string;
      members: Array<{ user_id: string; display_name: string }>;
    }>(`/groups/${gid}`),
  joinGroup: (gid: string) =>
    request<{ group_id: string }>(`/groups/${gid}/join`, { method: 'POST' }),
  leaveGroup: (gid: string) =>
    request<{ group_id: string }>(`/groups/${gid}/leave`, { method: 'POST' }),
};
