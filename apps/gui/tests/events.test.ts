import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

const INVOKE_KEY = '__TAURI_INVOKE__';

/// 同一 module registry 取事件轮询器与其消费的各 store，保证相互引用一致
async function fresh() {
	vi.resetModules();
	const eventsMod = await import('$lib/stores/events');
	const triggersMod = await import('$lib/stores/triggers');
	const logsMod = await import('$lib/stores/logs');
	const scriptsMod = await import('$lib/stores/scripts');
	const connectionMod = await import('$lib/stores/connection');
	const settingsMod = await import('$lib/stores/settings');
	return {
		createEventPoller: eventsMod.createEventPoller,
		triggers: triggersMod.triggers,
		logs: logsMod.logs,
		scripts: scriptsMod.scripts,
		connection: connectionMod.connection,
		settings: settingsMod.settings,
	};
}

function event(method: string, payload: Record<string, any>, timestamp = 1700000000000) {
	return { method, payload, timestamp };
}

describe('events 轮询器与事件分发', () => {
	beforeEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('无 invoke 时 tick 为 no-op', async () => {
		const { createEventPoller } = await fresh();
		const poller = createEventPoller();
		poller.start(); // tick 内部发现无 invoke 直接返回，不抛错
		poller.stop();
	});

	it('start/stop：立即拉取一次并按 500ms 轮询，stop 后不再拉取', async () => {
		vi.useFakeTimers();
		try {
			const invokeMock = vi.fn(async () => ({ events: [], dropped: 0 }));
			(window as any)[INVOKE_KEY] = invokeMock;
			const { createEventPoller } = await fresh();
			const poller = createEventPoller();

			poller.start();
			await vi.advanceTimersByTimeAsync(0);
			expect(invokeMock).toHaveBeenCalledTimes(1);

			await vi.advanceTimersByTimeAsync(500);
			await vi.advanceTimersByTimeAsync(500);
			expect(invokeMock).toHaveBeenCalledTimes(3);

			poller.start(); // 重复 start 不叠加定时器
			poller.stop();
			poller.stop(); // 重复 stop 安全
			await vi.advanceTimersByTimeAsync(2000);
			expect(invokeMock).toHaveBeenCalledTimes(3);
		} finally {
			vi.useRealTimers();
		}
	});

	it('runtime 返回 dropped 计数时通知 logs', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({ events: [], dropped: 7 }));
		const { createEventPoller, logs } = await fresh();
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		expect(get(logs.dropped)).toBe(7);
		poller.stop();
	});

	it('invoke 失败时静默（下一轮重试）', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => { throw new Error('down'); });
		const { createEventPoller, logs } = await fresh();
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		expect(get(logs)).toHaveLength(0);
		poller.stop();
	});

	it('单条事件分发抛错不影响后续事件', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('trigger.fired', { id: '1', name: 'T1' }),
				event('log.line', { message: 'after crash', level: 'info' }),
			],
		}));
		const { createEventPoller, triggers, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		vi.spyOn(triggers, 'markFired').mockImplementation(() => {
			throw new Error('dispatch boom');
		});
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		expect(get(logs).some(e => e.message === 'after crash')).toBe(true);
		poller.stop();
	});

	it('log.line：级别映射全表（trace/debug/err/error/critical/warn/warning/info/success/未知）', async () => {
		const levels = ['trace', 'debug', 'err', 'error', 'critical', 'warn', 'warning', 'info', 'success', 'bogus'];
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: levels.map(level => event('log.line', { message: `msg-${level}`, level })),
		}));
		const { createEventPoller, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();

		const entries = get(logs);
		expect(entries.map(e => e.type)).toEqual([
			'debug', 'debug', 'error', 'error', 'error', 'warning', 'warning', 'info', 'success', 'info',
		]);
		expect(entries.every(e => e.source === 'runtime')).toBe(true);
		expect(entries[0].time).toBe(`[${new Date(1700000000000).toTimeString().split(' ')[0]}]`);
	});

	it('log.line：message 非字符串时忽略', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [event('log.line', { message: 42 })],
		}));
		const { createEventPoller, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();
		expect(get(logs)).toHaveLength(0);
	});

	it('trigger.fired：标记命中；携带 name 时记录 success 日志', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('trigger.fired', { id: 't1', name: 'HP 药水' }),
				event('trigger.fired', { id: 't2' }),
			],
		}));
		const { createEventPoller, triggers, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		triggers.set([
			{ id: 't1', name: 'HP 药水', enabled: true, condition: { type: 'color_found', value: '', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 }, actions: [] },
			{ id: 't2', name: '无名字段', enabled: true, condition: { type: 'color_found', value: '', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 }, actions: [] },
		]);
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();

		const items = get(triggers);
		expect(items[0].last_triggered).toBe(true);
		expect(items[1].last_triggered).toBe(true);
		const messages = get(logs).map(e => e.message);
		expect(messages).toContain('触发器命中: HP 药水');
		expect(messages.filter(m => m.startsWith('触发器命中'))).toHaveLength(1);
	});

	it('script.state_changed：联动脚本状态并记录日志，error 字段透传', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('script.state_changed', { id: 'a', state: 'paused' }),
				event('script.state_changed', { id: 'a', state: 'error', error: 'lua panic' }),
				event('script.state_changed', { id: 'a', state: 42 }),
			],
		}));
		const { createEventPoller, scripts, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		scripts.set([
			{ id: 'a', name: 'a.lua', path: 'scripts/a.lua', size: 1, is_running: true, state: 'running', error: '', loaded_at: 0 },
		]);
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();

		expect(get(scripts)[0]).toMatchObject({ state: 'error', is_running: false, error: 'lua panic' });
		const messages = get(logs).map(e => e.message);
		expect(messages).toContain('脚本状态变更: a → paused');
		expect(messages).toContain('脚本状态变更: a → error');
	});

	it('script.output：带 id 前缀、不带 id、空输出与非字符串输出', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('script.output', { id: 'a', output: 'line1' }),
				event('script.output', { output: 'line2' }),
				event('script.output', { id: 'a', output: '' }),
				event('script.output', { id: 'a', output: 7 }),
			],
		}));
		const { createEventPoller, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();

		const messages = get(logs).map(e => e.message);
		expect(messages).toEqual(['[a] line1', 'line2']);
	});

	it('connection.state_changed：connected→success、error→error、其他→info，message 可选', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('connection.state_changed', { state: 'connected' }),
				event('connection.state_changed', { state: 'error', message: 'handshake failed' }),
				event('connection.state_changed', { state: 'reconnecting' }),
				event('connection.state_changed', { state: 9 }),
			],
		}));
		const { createEventPoller, logs, connection, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();

		const entries = get(logs);
		expect(entries.map(e => e.type)).toEqual(['success', 'error', 'info']);
		expect(entries[1].message).toBe('远程连接: error — handshake failed');
		// 事件按序消费，remote 停留在最后一条有效状态
		expect(get(connection).remote).toEqual({ state: 'reconnecting', message: '' });
	});

	it('connection.ipc_client：上线/下线日志', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('connection.ipc_client', { state: 'connected' }),
				event('connection.ipc_client', { state: 'disconnected' }),
				event('connection.ipc_client', { state: 1 }),
			],
		}));
		const { createEventPoller, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();

		const entries = get(logs);
		expect(entries.map(e => e.message)).toEqual(['runtime IPC 客户端上线', 'runtime IPC 客户端下线']);
		expect(entries.map(e => e.type)).toEqual(['success', 'warning']);
	});

	it('screenshot.frame 与未知 method 均安全忽略', async () => {
		(window as any)[INVOKE_KEY] = vi.fn(async () => ({
			events: [
				event('screenshot.frame', { data: 'x' }),
				event('totally.unknown', { foo: 1 }),
			],
		}));
		const { createEventPoller, logs, settings } = await fresh();
		settings.update({ logLevel: 'debug' });
		const poller = createEventPoller();
		poller.start();
		await Promise.resolve();
		await Promise.resolve();
		poller.stop();
		expect(get(logs)).toHaveLength(0);
	});
});
