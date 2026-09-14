import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type SettingsStore = Awaited<typeof import('$lib/stores/settings')>['settings'];
type Settings = ReturnType<typeof get<SettingsStore>>;

const KEY = 'wingman-settings';

async function freshStore(): Promise<SettingsStore> {
	vi.resetModules();
	const mod = await import('$lib/stores/settings');
	return mod.settings;
}

describe('settings store', () => {
	beforeEach(() => {
		localStorage.removeItem(KEY);
	});

	afterEach(() => {
		vi.restoreAllMocks();
		localStorage.removeItem(KEY);
	});

	it('无存储时返回默认设置', async () => {
		const settings = await freshStore();
		expect(get(settings)).toMatchObject({
			ipcEndpoint: 'wingman',
			orchestratorUrl: 'http://localhost:9527',
			autoReconnect: true,
			autoStart: false,
			minimizeOnStart: false,
			logLevel: 'info',
			theme: 'dark',
		});
	});

	it('已存设置与默认值合并（旧配置缺字段用默认补齐）', async () => {
		localStorage.setItem(KEY, JSON.stringify({ logLevel: 'warn', autoStart: true }));
		const settings = await freshStore();
		expect(get(settings)).toMatchObject({ logLevel: 'warn', autoStart: true, ipcEndpoint: 'wingman' });
	});

	it('存储内容非法 JSON 时回退默认设置', async () => {
		localStorage.setItem(KEY, '{not json');
		const settings = await freshStore();
		expect(get(settings).logLevel).toBe('info');
	});

	it('localStorage.getItem 抛错时回退默认设置（Tauri webview 受限场景）', async () => {
		vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
			throw new Error('storage blocked');
		});
		const settings = await freshStore();
		expect(get(settings).logLevel).toBe('info');
	});

	it('update 修改字段并持久化到 localStorage', async () => {
		const settings = await freshStore();
		settings.update({ logLevel: 'error', autoReconnect: false });
		const saved = JSON.parse(localStorage.getItem(KEY)!);
		expect(saved).toMatchObject({ logLevel: 'error', autoReconnect: false });
		expect(get(settings).logLevel).toBe('error');
	});

	it('持久化失败（setItem 抛错）不影响内存状态', async () => {
		const settings = await freshStore();
		vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
			throw new Error('quota exceeded');
		});
		expect(() => settings.update({ logLevel: 'warn' })).not.toThrow();
		expect(get(settings).logLevel).toBe('warn');
	});
});
