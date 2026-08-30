import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// 开发期代理到本地 prosophor_web(免 CORS);生产由服务端同源托管 dist
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/api': 'http://127.0.0.1:8787',
      '/ws': { target: 'ws://127.0.0.1:8787', ws: true },
    },
  },
  build: {
    outDir: 'dist',
    sourcemap: false,
  },
});
