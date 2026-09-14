import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type ThemeModule = Awaited<typeof import('$lib/stores/theme')>;

const KEY = 'wingman.theme';

async function freshModule(): Promise<ThemeModule> {
	vi.resetModules();
	return import('$lib/stores/theme');
}

describe('theme store', () => {
	beforeEach(() => {
		localStorage.removeItem(KEY);
		document.documentElement.dataset.theme = '';
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.unstubAllGlobals();
		localStorage.removeItem(KEY);
	});

	it('默认 dark 并在模块加载时应用到 documentElement', async () => {
		const { theme } = await freshModule();
		expect(get(theme)).toBe('dark');
		expect(document.documentElement.dataset.theme).toBe('dark');
	});

	it('已存储的合法主题在初始化时恢复', async () => {
		localStorage.setItem(KEY, 'light');
		const { theme } = await freshModule();
		expect(get(theme)).toBe('light');
		expect(document.documentElement.dataset.theme).toBe('light');
	});

	it('存储非法值时回退 dark', async () => {
		localStorage.setItem(KEY, 'blue');
		const { theme } = await freshModule();
		expect(get(theme)).toBe('dark');
	});

	it('localStorage 读取抛错时回退 dark', async () => {
		vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
			throw new Error('blocked');
		});
		const { theme } = await freshModule();
		expect(get(theme)).toBe('dark');
	});

	it('document 不可用时（SSR/受限环境）模块加载不失败', async () => {
		vi.stubGlobal('document', undefined);
		const { theme } = await freshModule();
		expect(get(theme)).toBe('dark');
	});

	it('setTheme 应用到 dataset 并持久化', async () => {
		const { setTheme } = await freshModule();
		setTheme('light');
		expect(document.documentElement.dataset.theme).toBe('light');
		expect(localStorage.getItem(KEY)).toBe('light');
	});

	it('setTheme 持久化失败时静默忽略', async () => {
		const { setTheme } = await freshModule();
		vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
			throw new Error('quota');
		});
		expect(() => setTheme('light')).not.toThrow();
		expect(document.documentElement.dataset.theme).toBe('light');
	});

	it('toggleTheme 在 dark/light 间往返', async () => {
		const { theme, toggleTheme } = await freshModule();
		toggleTheme();
		expect(get(theme)).toBe('light');
		toggleTheme();
		expect(get(theme)).toBe('dark');
		expect(localStorage.getItem(KEY)).toBe('dark');
	});

	it('toggleTheme 依据存储值而非当前 store 值（存储被外部改动后 toggle 结果可预期）', async () => {
		localStorage.setItem(KEY, 'light');
		const { theme, toggleTheme } = await freshModule();
		expect(get(theme)).toBe('light');
		toggleTheme();
		expect(get(theme)).toBe('dark');
	});
});
