import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type UpdatesStore = Awaited<typeof import('$lib/stores/updates')>['updates'];
type LogsStore = Awaited<typeof import('$lib/stores/logs')>['logs'];

const INVOKE_KEY = '__TAURI_INVOKE__';

const IDLE = {
	enabled: true,
	available: false,
	downloading: false,
	downloaded: false,
	localVersion: '0.1.2',
	candidate: {
		version: '', url: '', sha256: '', size: 0, notes: '', mandatory: false, fileName: '',
	},
	downloadPath: '',
	lastCheckMs: 0,
	lastError: '',
};

async function fresh(): Promise<{ updates: UpdatesStore; logs: LogsStore }> {
	vi.resetModules();
	const updatesMod = await import('$lib/stores/updates');
	const logsMod = await import('$lib/stores/logs');
	return { updates: updatesMod.updates, logs: logsMod.logs };
}

describe('updates store（dev 模式，无 __TAURI_INVOKE__）', () => {
	beforeEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('invoke 类操作为 no-op，事件回投仍生效', async () => {
		const { updates, logs } = await fresh();
		await expect(updates.refresh()).resolves.toBeUndefined();
		await expect(updates.check()).resolves.toBeUndefined();
		await expect(updates.download()).resolves.toBeUndefined();
		// applyStatus 不依赖 invoke，事件回投照常生效
		updates.applyStatus({ enabled: true, available: true, candidate: { version: '0.2.0' } });
		expect(get(updates)).toMatchObject({ enabled: true, available: true });
		// 空载荷忽略
		await updates.applyStatus(null);
		await updates.applyStatus('bogus');
		expect(get(updates)).toMatchObject({ enabled: true, available: true });
		expect(get(logs).some(e => e.message.includes('发现新版本 0.2.0'))).toBe(true);
	});
});

describe('updates store（invoke 模式）', () => {
	let invokeMock: ReturnType<typeof vi.fn>;
	let updates: UpdatesStore;
	let logs: LogsStore;

	function messages(): string[] {
		return get(logs).map(e => e.message);
	}

	function lastLog(): string {
		const list = messages();
		return list[list.length - 1] ?? '';
	}

	beforeEach(async () => {
		invokeMock = vi.fn();
		(window as any)[INVOKE_KEY] = invokeMock;
		({ updates, logs } = await fresh());
		logs.clear();
	});

	afterEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('refresh 同步状态；null 保持现状；失败静默', async () => {
		invokeMock.mockResolvedValue({ ...IDLE, localVersion: '0.1.9' });
		await updates.refresh();
		expect(invokeMock).toHaveBeenCalledWith('get_update_status');
		expect(get(updates)).toMatchObject({ localVersion: '0.1.9', available: false });

		invokeMock.mockResolvedValue(null);
		await updates.refresh();
		expect(get(updates).localVersion).toBe('0.1.9');

		invokeMock.mockRejectedValue('ipc down');
		await expect(updates.refresh()).resolves.toBeUndefined();
		expect(get(updates).localVersion).toBe('0.1.9');
	});

	it('check 成功受理并回填状态，失败记录日志', async () => {
		invokeMock.mockResolvedValue({ started: true, status: { ...IDLE, available: true, candidate: { ...IDLE.candidate, version: '0.2.0' } } });
		await updates.check();
		expect(invokeMock).toHaveBeenCalledWith('check_update');
		expect(get(updates)).toMatchObject({ available: true });
		expect(get(updates).candidate.version).toBe('0.2.0');

		invokeMock.mockRejectedValue('boom');
		await updates.check();
		expect(lastLog()).toContain('检查更新失败');
	});

	it('check 回执无 status 时不改动现有状态', async () => {
		await updates.refresh();
		invokeMock.mockResolvedValue({ started: false });
		await updates.check();
		expect(get(updates).available).toBe(false);
	});

	it('download 成功回填 downloading，失败记录日志', async () => {
		invokeMock.mockResolvedValue({ started: true, status: { ...IDLE, downloading: true } });
		await updates.download();
		expect(invokeMock).toHaveBeenCalledWith('download_update');
		expect(get(updates).downloading).toBe(true);

		invokeMock.mockRejectedValue('sha256 mismatch');
		await updates.download();
		expect(lastLog()).toContain('下载更新失败');
	});

	it('applyStatus 在关键点各记一条日志', async () => {
		logs.clear();
		const candidate = { ...IDLE.candidate, version: '0.2.0' };
		updates.applyStatus({ ...IDLE, available: true, candidate });
		expect(lastLog()).toBe('发现新版本 0.2.0');

		updates.applyStatus({ ...IDLE, available: true, downloading: true, candidate });
		expect(lastLog()).toBe('正在下载更新 0.2.0');

		updates.applyStatus({ ...IDLE, downloading: false, downloaded: true, downloadPath: '/tmp/wingman-0.2.0.exe' });
		expect(lastLog()).toBe('更新已就绪: /tmp/wingman-0.2.0.exe');

		updates.applyStatus({ ...IDLE, lastError: 'download failed (transfer or sha256 mismatch)' });
		expect(lastLog()).toBe('download failed (transfer or sha256 mismatch)');
	});

	it('applyStatus 对同一状态重复回投不重复记日志', async () => {
		logs.clear();
		const payload = { ...IDLE, available: true, candidate: { ...IDLE.candidate, version: '0.2.0' } };
		updates.applyStatus(payload);
		updates.applyStatus(payload);
		expect(messages().filter(m => m.includes('发现新版本')).length).toBe(1);
	});

	it('applyStatus 忽略空与非对象载荷', async () => {
		logs.clear();
		updates.applyStatus(null);
		updates.applyStatus(undefined);
		updates.applyStatus('not-an-object');
		expect(get(updates)).toMatchObject({ enabled: false, available: false, localVersion: '' });
		expect(messages()).toHaveLength(0);
	});
});
