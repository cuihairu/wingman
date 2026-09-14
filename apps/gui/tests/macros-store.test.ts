import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type MacrosStore = Awaited<typeof import('$lib/stores/macros')>['macros'];
type MacroStatus = Awaited<typeof import('$lib/stores/macros')>['MacroStatus'];
type LogsStore = Awaited<typeof import('$lib/stores/logs')>['logs'];

const INVOKE_KEY = '__TAURI_INVOKE__';

async function fresh(): Promise<{ macros: MacrosStore; logs: LogsStore }> {
	vi.resetModules();
	const macrosMod = await import('$lib/stores/macros');
	const logsMod = await import('$lib/stores/logs');
	return { macros: macrosMod.macros, logs: logsMod.logs };
}

describe('macros store（dev 模式，无 __TAURI_INVOKE__）', () => {
	beforeEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('所有操作为 no-op 并返回安全默认值', async () => {
		const { macros } = await fresh();
		await expect(macros.record()).resolves.toBeUndefined();
		await expect(macros.stop()).resolves.toBe(0);
		await expect(macros.play()).resolves.toBeUndefined();
		await expect(macros.save('/tmp/m.json')).resolves.toBe(false);
		await expect(macros.load('/tmp/m.json')).resolves.toBe(false);
		await expect(macros.clear()).resolves.toBeUndefined();
		await expect(macros.refresh()).resolves.toBeUndefined();
		expect(get(macros)).toEqual({ recording: false, paused: false, eventCount: 0 });
	});
});

describe('macros store（invoke 模式）', () => {
	let invokeMock: ReturnType<typeof vi.fn>;
	let macros: MacrosStore;
	let logs: LogsStore;

	function lastLog(): string | undefined {
		const entries = get(logs);
		return entries[entries.length - 1]?.message;
	}

	beforeEach(async () => {
		invokeMock = vi.fn();
		(window as any)[INVOKE_KEY] = invokeMock;
		({ macros, logs } = await fresh());
	});

	afterEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('record 成功：recording 置位并记录日志', async () => {
		invokeMock.mockResolvedValue(null);
		await macros.record();
		expect(invokeMock).toHaveBeenCalledWith('macro_record');
		expect(get(macros).recording).toBe(true);
		expect(lastLog()).toBe('开始录制宏');
	});

	it('record 失败：记录 error 日志且不置位', async () => {
		invokeMock.mockRejectedValue(new Error('hook busy'));
		await macros.record();
		expect(get(macros).recording).toBe(false);
		expect(lastLog()).toContain('录制启动失败');
	});

	it('stop 成功：返回事件数并更新 eventCount', async () => {
		invokeMock.mockResolvedValue({ eventCount: 42 });
		await expect(macros.stop()).resolves.toBe(42);
		expect(get(macros)).toMatchObject({ recording: false, eventCount: 42 });
		expect(lastLog()).toBe('停止录制，捕获 42 个事件');
	});

	it('stop 返回缺 eventCount 时按 0 处理', async () => {
		invokeMock.mockResolvedValue({});
		await expect(macros.stop()).resolves.toBe(0);
		expect(get(macros).eventCount).toBe(0);
	});

	it('stop 失败：返回 0 并记录 error 日志', async () => {
		invokeMock.mockRejectedValue('stop boom');
		await expect(macros.stop()).resolves.toBe(0);
		expect(lastLog()).toContain('停止录制失败');
	});

	it('play 成功与失败分别记录日志', async () => {
		invokeMock.mockResolvedValue(null);
		await macros.play(50, 3);
		expect(invokeMock).toHaveBeenCalledWith('macro_play', { speed: 50, repeat: 3 });
		expect(lastLog()).toBe('宏回放完成');

		invokeMock.mockRejectedValue(new Error('replay error'));
		await macros.play();
		expect(lastLog()).toContain('宏回放失败');
	});

	it('save 成功/失败返回布尔并记录日志', async () => {
		invokeMock.mockResolvedValue(null);
		await expect(macros.save('D:/m1.json')).resolves.toBe(true);
		expect(lastLog()).toBe('宏已保存: D:/m1.json');

		invokeMock.mockRejectedValue('disk full');
		await expect(macros.save('D:/m1.json')).resolves.toBe(false);
		expect(lastLog()).toContain('保存宏失败');
	});

	it('load 成功更新 eventCount，失败返回 false', async () => {
		invokeMock.mockResolvedValue({ eventCount: 7 });
		await expect(macros.load('D:/m1.json')).resolves.toBe(true);
		expect(get(macros).eventCount).toBe(7);
		expect(lastLog()).toBe('宏已载入: D:/m1.json');

		invokeMock.mockRejectedValue('bad file');
		await expect(macros.load('D:/m1.json')).resolves.toBe(false);
		expect(lastLog()).toContain('载入宏失败');
	});

	it('clear 成功清零 eventCount，失败记录日志', async () => {
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'macro_clear') return null;
			throw new Error('x');
		});
		await macros.clear();
		expect(get(macros).eventCount).toBe(0);
		expect(lastLog()).toBe('已清空录制');

		invokeMock.mockRejectedValue('clear failed');
		await macros.clear();
		expect(lastLog()).toContain('清空失败');
	});

	it('refresh 成功同步状态；status 为 null 时保持现状；失败静默', async () => {
		invokeMock.mockResolvedValue({ recording: true, paused: false, eventCount: 12 });
		await macros.refresh();
		expect(get(macros)).toEqual({ recording: true, paused: false, eventCount: 12 });

		invokeMock.mockResolvedValue(null);
		await macros.refresh();
		expect(get(macros).recording).toBe(true);

		invokeMock.mockRejectedValue(new Error('ipc down'));
		await expect(macros.refresh()).resolves.toBeUndefined();
		expect(get(macros).recording).toBe(true);
	});
});
