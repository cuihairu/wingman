import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

/**
 * 宏 store invoke 链路：record/stop/play/save/load/clear 的成功与失败路径、
 * 无 invoke 时的守卫早退、refresh 的空响应与异常吞掉、stop 的 ?? 0 回退。
 */

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const macrosMod = await import('$lib/stores/macros');
	const logsMod = await import('$lib/stores/logs');
	return { macros: macrosMod.macros, logs: logsMod.logs };
}

async function devMode() {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	delete (window as any).__TAURI_INVOKE__;
	const macrosMod = await import('$lib/stores/macros');
	return macrosMod.macros;
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	vi.restoreAllMocks();
	delete (window as any).__TAURI_INVOKE__;
});

describe('宏 store：invoke 成功路径', () => {
	it('record 成功置 recording 并写日志', async () => {
		const { macros, logs } = await fresh((cmd) => {
			if (cmd === 'macro_record') return {};
			return {};
		});
		await macros.record();
		expect(get(macros).recording).toBe(true);
		expect(get(logs).some(e => e.message === '开始录制宏' && e.type === 'info')).toBe(true);
	});

	it('stop 返回事件数并写 success 日志', async () => {
		const { macros, logs } = await fresh((cmd) => {
			if (cmd === 'macro_stop') return { eventCount: 7 };
			return {};
		});
		const count = await macros.stop();
		expect(count).toBe(7);
		expect(get(macros)).toMatchObject({ recording: false, eventCount: 7 });
		expect(get(logs).some(e => e.message === '停止录制，捕获 7 个事件' && e.type === 'success')).toBe(true);
	});

	it('stop 响应缺 eventCount 时回退 0', async () => {
		const { macros } = await fresh((cmd) => {
			if (cmd === 'macro_stop') return {};
			return {};
		});
		const count = await macros.stop();
		expect(count).toBe(0);
		expect(get(macros).eventCount).toBe(0);
	});

	it('play 成功写回放完成日志', async () => {
		const calls: Array<[string, Record<string, unknown> | undefined]> = [];
		const { macros, logs } = await fresh((cmd, args) => {
			calls.push([cmd, args]);
			return {};
		});
		await macros.play(50, 3);
		expect(calls).toContainEqual(['macro_play', { speed: 50, repeat: 3 }]);
		expect(get(logs).some(e => e.message === '宏回放完成' && e.type === 'success')).toBe(true);
	});

	it('save 成功返回 true 并写日志', async () => {
		const { macros, logs } = await fresh(() => ({}));
		await expect(macros.save('m.json')).resolves.toBe(true);
		expect(get(logs).some(e => e.message === '宏已保存: m.json' && e.type === 'success')).toBe(true);
	});

	it('load 成功写入 eventCount 并返回 true', async () => {
		const { macros, logs } = await fresh((cmd) => {
			if (cmd === 'macro_load') return { eventCount: 4 };
			return {};
		});
		await expect(macros.load('m.json')).resolves.toBe(true);
		expect(get(macros).eventCount).toBe(4);
		expect(get(logs).some(e => e.message === '宏已载入: m.json' && e.type === 'success')).toBe(true);
	});

	it('clear 成功清零事件数', async () => {
		const { macros, logs } = await fresh(() => ({}));
		await macros.clear();
		expect(get(macros).eventCount).toBe(0);
		expect(get(logs).some(e => e.message === '已清空录制' && e.type === 'info')).toBe(true);
	});

	it('refresh 用返回状态更新 store', async () => {
		const { macros } = await fresh((cmd) => {
			if (cmd === 'macro_status') return { recording: true, paused: false, eventCount: 9 };
			return {};
		});
		await macros.refresh();
		expect(get(macros)).toEqual({ recording: true, paused: false, eventCount: 9 });
	});

	it('refresh 收到空响应时保持原状态', async () => {
		const { macros } = await fresh((cmd) => {
			if (cmd === 'macro_status') return null;
			return {};
		});
		await macros.refresh();
		expect(get(macros)).toEqual({ recording: false, paused: false, eventCount: 0 });
	});
});

describe('宏 store：invoke 失败路径', () => {
	it('record 失败写 error 日志', async () => {
		const { macros, logs } = await fresh(() => { throw new Error('hook busy'); });
		await macros.record();
		expect(get(macros).recording).toBe(false);
		expect(get(logs).some(e => e.type === 'error' && e.message.includes('hook busy'))).toBe(true);
	});

	it('stop 失败返回 0 并写 error 日志', async () => {
		const { macros, logs } = await fresh(() => { throw new Error('not recording'); });
		await expect(macros.stop()).resolves.toBe(0);
		expect(get(logs).some(e => e.type === 'error' && e.message.includes('not recording'))).toBe(true);
	});

	it('play 失败写 error 日志', async () => {
		const { logs } = await fresh(() => { throw new Error('no events'); });
		const { macros } = { macros: await import('$lib/stores/macros').then(m => m.macros) };
		await macros.play();
		expect(get(logs).some(e => e.type === 'error' && e.message.includes('no events'))).toBe(true);
	});

	it('save 失败返回 false', async () => {
		const { macros, logs } = await fresh(() => { throw new Error('disk full'); });
		await expect(macros.save('m.json')).resolves.toBe(false);
		expect(get(logs).some(e => e.type === 'error' && e.message.includes('disk full'))).toBe(true);
	});

	it('load 失败返回 false', async () => {
		const { macros } = await fresh(() => { throw new Error('bad json'); });
		await expect(macros.load('m.json')).resolves.toBe(false);
	});

	it('clear 失败写 error 日志', async () => {
		const { macros, logs } = await fresh(() => { throw new Error('denied'); });
		await macros.clear();
		expect(get(logs).some(e => e.type === 'error' && e.message.includes('denied'))).toBe(true);
	});

	it('refresh 异常被吞掉不抛出', async () => {
		const { macros } = await fresh(() => { throw new Error('ipc down'); });
		await expect(macros.refresh()).resolves.toBeUndefined();
	});
});

describe('宏 store：无 invoke 的 dev 守卫', () => {
	it('dev 模式各操作均为 no-op', async () => {
		const macros = await devMode();
		await expect(macros.record()).resolves.toBeUndefined();
		await expect(macros.stop()).resolves.toBe(0);
		await expect(macros.play()).resolves.toBeUndefined();
		await expect(macros.save('a')).resolves.toBe(false);
		await expect(macros.load('a')).resolves.toBe(false);
		await expect(macros.clear()).resolves.toBeUndefined();
		await expect(macros.refresh()).resolves.toBeUndefined();
		expect(get(macros)).toEqual({ recording: false, paused: false, eventCount: 0 });
	});
});
