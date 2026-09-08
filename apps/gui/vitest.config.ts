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
	},
});
