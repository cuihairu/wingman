import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

async function fresh() {
	vi.resetModules();
	return import('$lib/router.svelte');
}

describe('hash router', () => {
	beforeEach(() => {
		window.location.hash = '';
	});

	afterEach(() => {
		vi.restoreAllMocks();
	});

	it('初始无 hash 时落在 dashboard', async () => {
		const { router } = await fresh();
		expect(get(router.current)).toBe('dashboard');
		expect(get(router)).toEqual({ current: 'dashboard' });
	});

	it('navigate 更新 hash 与状态', async () => {
		const { router } = await fresh();
		router.navigate('scripts');
		expect(window.location.hash).toBe('#scripts');
		expect(get(router.current)).toBe('scripts');
		expect(get(router)).toEqual({ current: 'scripts' });
	});

	it('init 后监听 hashchange（浏览器前进/后退）', async () => {
		const { router } = await fresh();
		router.init();
		window.location.hash = '#logs';
		await new Promise(resolve => setTimeout(resolve, 0));
		expect(get(router.current)).toBe('logs');
	});

	it('hash 变为空（#）时回退 dashboard', async () => {
		const { router } = await fresh();
		router.navigate('settings');
		router.init();
		window.location.hash = '#';
		await new Promise(resolve => setTimeout(resolve, 0));
		expect(get(router.current)).toBe('dashboard');
	});
});
