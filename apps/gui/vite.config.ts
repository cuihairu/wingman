import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import path from 'path';

export default defineConfig({
	plugins: [svelte()],
	clearScreen: false,
	resolve: {
		alias: {
			'$lib': path.resolve(__dirname, 'src/lib'),
		},
	},
	server: {
		port: 5173,
		strictPort: true,
		watch: {
			ignored: ['**/src-tauri/**'],
		},
	},
	build: {
		target: ['es2021', 'chrome100', 'safari13'],
		outDir: 'dist',
		emptyOutDir: true,
	},
});
