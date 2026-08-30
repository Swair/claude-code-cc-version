import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './index.css';

// 启动诊断:JS 异常直接显示在页面上(定位"页面空白/无请求"类问题)
const APP_VERSION = 'v3-get-auth';
window.addEventListener('error', (e) => {
  const root = document.getElementById('root');
  if (root) {
    root.innerHTML = `<pre style="color:#e05252;padding:24px;font-size:13px;white-space:pre-wrap">
Prosophor ${APP_VERSION} JS Error
${e.message ?? 'unknown'}
at ${e.filename ?? ''}:${e.lineno ?? ''}:${e.colno ?? ''}
${e.error?.stack ?? ''}</pre>`;
  }
});
window.addEventListener('unhandledrejection', (e) => {
  const root = document.getElementById('root');
  if (root) {
    root.innerHTML = `<pre style="color:#e05252;padding:24px;font-size:13px;white-space:pre-wrap">
Prosophor ${APP_VERSION} Unhandled Rejection
${e.reason?.message ?? String(e.reason)}</pre>`;
  }
});
// 版本标记(证明当前页面跑的是哪版 JS)
console.info(`Prosophor ${APP_VERSION}`);

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
