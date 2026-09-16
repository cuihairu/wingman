import { defineConfig } from 'vitest/config';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import path from 'path';

export default defineConfig({
	plugins: [svelte()],
	resolve: {
		alias: {
			'$lib': path.resolve(__dirname, 'src/lib'),
		},
		/// jsdom 环境下必须走 svelte 的 browser 导出（default 指向 server 运行时）
		conditions: ['browser'],
	},
	test: {
		environment: 'jsdom',
		include: ['tests/**/*.test.ts'],
		setupFiles: ['tests/setup.ts'],
		/// jsdom 环境创建占全程 ~40%，并发下 waitFor 轮询易超默认 5s（实测
		/// 单跑 3-7s 通过、全量并发 6-9s 超时），放宽到 20s 消除假失败。
		testTimeout: 20_000,
		coverage: {
			provider: 'v8',
			include: ['src/**'],
			// main.ts 为入口引导（jsdom 下不可测，挂载副作用），同 Dashboard 惯例排除
			exclude: ['src/**/*.d.ts', 'src/main.ts', 'src-tauri/**'],
			reporter: ['text', 'json-summary', 'json'],
		},
	},
});
