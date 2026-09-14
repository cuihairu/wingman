import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type ConnectionStore = Awaited<typeof import('$lib/stores/connection')>['connection'];
type ConnectionState = ReturnType<typeof get<ConnectionStore>>;

const INVOKE_KEY = '__TAURI_INVOKE__';

function statusPayload(extra: Record<string, unknown> = {}) {
	return {
		server: 'wingman',
		version: '1.2.3',
		uptime: 10,
		running_scripts: 0,
		paused: false,
		...extra,
	};
}

async function fresh(handler: ((cmd: string, args?: Record<string, unknown>) => unknown) | null) {
	vi.resetModules();
	/// 同文件先前的用例会把设置持久化到 localStorage，必须清理后再加载 store
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	if (handler) {
		(window as any)[INVOKE_KEY] = vi.fn(handler);
	} else {
		delete (window as any)[INVOKE_KEY];
	}
	const connMod = await import('$lib/stores/connection');
	const logsMod = await import('$lib/stores/logs');
	const settingsMod = await import('$lib/stores/settings');
	return {
		connection: connMod.connection,
		logs: logsMod.logs,
		settings: settingsMod.settings,
		invokeMock: (window as any)[INVOKE_KEY] as ReturnType<typeof vi.fn> | undefined,
	};
}

beforeEach(() => {
	vi.useFakeTimers();
});

afterEach(() => {
	vi.clearAllTimers();
	vi.useRealTimers();
	delete (window as any)[INVOKE_KEY];
});

describe('connection store（dev 模式，无 __TAURI_INVOKE__）', () => {
	it('connect 模拟已连接并写入 dev 版本号', async () => {
		const { connection } = await fresh(null);
		await connection.connect();
		const state = get(connection);
		expect(state.ipc.state).toBe('connected');
		expect(state.version).toBe('wingman 0.1.0 (dev)');
		expect(state.ipc.lastConnectedAt).not.toBeNull();

		await connection.connect();
		expect(get(connection).version).toBe('wingman 0.1.0 (dev)'); // 非首次不再覆盖
	});

	it('togglePause 本地切换 paused', async () => {
		const { connection } = await fresh(null);
		await connection.togglePause();
		expect(get(connection).paused).toBe(true);
		await connection.togglePause();
		expect(get(connection).paused).toBe(false);
	});

	it('dev 下 stopAll/startActiveProfile/stopActiveProfile 返回 0', async () => {
		const { connection } = await fresh(null);
		await expect(connection.stopAll()).resolves.toBe(0);
		await expect(connection.startActiveProfile()).resolves.toBe(0);
		await expect(connection.stopActiveProfile()).resolves.toBe(0);
	});

	it('dev refresh 返回本地状态快照（paused 透传）', async () => {
		const { connection } = await fresh(null);
		await connection.togglePause();
		const status = await connection.refresh();
		expect(status).toMatchObject({
			server: 'wingman',
			version: 'wingman 0.1.0 (dev)',
			paused: true,
		});
	});
});

describe('connection store（invoke 模式）', () => {
	it('connect 成功：状态/版本/端点/远程/runtime 视角填充，重连钩子触发', async () => {
		let hookCalls = 0;
		const { connection } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'unix:/tmp/sock' };
			if (cmd === 'get_system_status') return statusPayload({ remote_state: 'connected', ipc_client_connected: true });
			return {};
		});
		connection.onReconnected(async () => { hookCalls += 1; });

		await connection.connect('wingman');
		const state: ConnectionState = get(connection);
		expect(state.ipc.state).toBe('connected');
		expect(state.ipcEndpoint).toBe('unix:/tmp/sock');
		expect(state.version).toBe('1.2.3');
		expect(state.remote?.state).toBe('connected');
		expect(state.runtimeView.ipcClientConnected).toBe(true);
		expect(hookCalls).toBe(1);
	});

	it('重连钩子抛错：记录 warning，不影响连接成功', async () => {
		const { connection, logs } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return statusPayload();
			return {};
		});
		connection.onReconnected(async () => { throw new Error('reload boom'); });
		await connection.connect();
		expect(get(connection).ipc.state).toBe('connected');
		expect(get(logs).some(e => e.message.includes('连接后数据加载失败: reload boom') && e.type === 'warning')).toBe(true);
	});

	it('connect 失败：抛出错误、进入 error 并记录消息', async () => {
		const { connection } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') throw new Error('connect refused');
			return {};
		});
		await expect(connection.connect()).rejects.toThrow('connect refused');
		const state = get(connection);
		expect(state.ipc.state).toBe('error');
		expect(state.ipc.message).toBe('connect refused');
		expect(state.ipc.lastErrorAt).not.toBeNull();
	});

	it('并发 connect 共享同一 promise（connect_ipc 只调用一次）', async () => {
		const { connection, invokeMock } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return statusPayload();
			return {};
		});
		await Promise.all([connection.connect(), connection.connect()]);
		const calls = invokeMock!.mock.calls.filter(([cmd]: string[]) => cmd === 'connect_ipc');
		expect(calls).toHaveLength(1);
	});

	it('connect 无 endpoint 时使用 settings.ipcEndpoint', async () => {
		const { connection, settings, invokeMock } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: false, endpoint: '' };
			if (cmd === 'get_system_status') return statusPayload();
			return {};
		});
		settings.update({ ipcEndpoint: 'from-settings' });
		await connection.connect();
		expect(invokeMock).toHaveBeenCalledWith('connect_ipc', { endpoint: 'from-settings' });
		// get_ipc_state 报告未连接时不覆盖用户端点
		expect(get(connection).ipcEndpoint).toBe('from-settings');
	});

	it('refreshEndpoint 失败仅忽略，连接不受影响', async () => {
		const { connection } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') throw new Error('diag boom');
			if (cmd === 'get_system_status') return statusPayload({ version: '' });
			return {};
		});
		await connection.connect();
		const state = get(connection);
		expect(state.ipc.state).toBe('connected');
		expect(state.version).toBe('-'); // status.version 为空串时保留旧值
	});

	it('refresh 缺 remote_state 时保留旧 remote', async () => {
		let withRemote = true;
		const { connection } = await fresh((cmd) => {
			if (cmd === 'get_system_status') {
				return statusPayload(withRemote ? { remote_state: 'connected' } : {});
			}
			return {};
		});
		await connection.refresh();
		expect(get(connection).remote?.state).toBe('connected');

		withRemote = false;
		await connection.refresh();
		expect(get(connection).remote?.state).toBe('connected'); // 事件陈旧时不清空，等待心跳纠正
	});

	it('refresh 失败：返回 null、清空 remote、进入自动重连（1s 退避）', async () => {
		let fail = false;
		const { connection, logs } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') {
				if (fail) throw new Error('IPC read timeout');
				return statusPayload({ remote_state: 'connected' });
			}
			return {};
		});

		await connection.connect();
		fail = true;
		await expect(connection.refresh()).resolves.toBeNull();

		const state = get(connection);
		expect(state.ipc.state).toBe('reconnecting');
		expect(state.ipc.attempts).toBe(1);
		expect(state.remote).toBeNull();

		// 第一次自动重连（1s 后），仍失败：记录 warning 并继续调度
		await vi.advanceTimersByTimeAsync(1000);
		expect(get(connection).ipc.attempts).toBe(2);
		const warnings = get(logs).filter(e => e.type === 'warning').map(e => e.message);
		expect(warnings).toEqual(['自动重连失败（第 1 次）: IPC read timeout']);
	});

	it('重连失败日志在第 1 次与第 5 次记录（避免刷屏）', async () => {
		const { connection, logs } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') throw new Error('still down');
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return statusPayload();
			return {};
		});

		// 初始连接失败 → error（不自动调度，scheduleReconnect 由 App 调用）
		await expect(connection.connect()).rejects.toThrow('still down');
		connection.scheduleReconnect();

		// 退避 1s + 2s + 4s + 8s + 16s = 第 1~5 次尝试
		await vi.advanceTimersByTimeAsync(31_000);
		const warnings = get(logs).filter(e => e.type === 'warning').map(e => e.message);
		expect(warnings).toEqual([
			'自动重连失败（第 1 次）: still down',
			'自动重连失败（第 5 次）: still down',
		]);
	});

	it('settings.autoReconnect=false 时心跳失败后不调度重连', async () => {
		let fail = false;
		const { connection, settings } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') {
				if (fail) throw new Error('down');
				return statusPayload();
			}
			return {};
		});
		settings.update({ autoReconnect: false });

		await connection.connect();
		fail = true;
		await connection.refresh();
		expect(get(connection).ipc.state).toBe('error');

		await vi.advanceTimersByTimeAsync(60_000);
		expect(get(connection).ipc.attempts).toBe(0);
		expect(get(connection).ipc.state).toBe('error');
	});

	it('scheduleReconnect 幂等：已调度时跳过', async () => {
		let fail = false;
		const { connection } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') {
				if (fail) throw new Error('down');
				return statusPayload();
			}
			return {};
		});
		await connection.connect();
		fail = true;
		await connection.refresh();
		expect(get(connection).ipc.attempts).toBe(1);

		connection.scheduleReconnect(); // 已有 timer，跳过
		expect(get(connection).ipc.attempts).toBe(1);
	});

	it('手动断开后心跳失败不再自动重连', async () => {
		let fail = false;
		const { connection, invokeMock } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'disconnect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') {
				if (fail) throw new Error('down');
				return statusPayload();
			}
			return {};
		});
		await connection.connect();
		await connection.disconnect();
		expect(invokeMock).toHaveBeenCalledWith('disconnect_ipc');
		expect(get(connection).ipc.state).toBe('disconnected');

		fail = true;
		await connection.refresh();
		expect(get(connection).ipc.state).toBe('error');
		await vi.advanceTimersByTimeAsync(60_000);
		expect(get(connection).ipc.attempts).toBe(0);
	});

	it('disconnect_ipc 抛错时仍完成状态复位', async () => {
		const { connection } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'disconnect_ipc') throw new Error('ipc channel gone');
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return statusPayload();
			return {};
		});
		await connection.connect();
		await expect(connection.disconnect()).resolves.toBeUndefined();
		expect(get(connection).ipc.state).toBe('disconnected');
	});

	it('retryNow 清除退避并立即重连成功', async () => {
		let fail = false;
		const { connection } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') {
				if (fail) throw new Error('down');
				return statusPayload();
			}
			return {};
		});
		await connection.connect();
		fail = true;
		await connection.refresh();
		expect(get(connection).ipc.state).toBe('reconnecting');

		fail = false;
		await connection.retryNow();
		const state = get(connection);
		expect(state.ipc.state).toBe('connected');
		expect(state.ipc.attempts).toBe(0);
	});

	it('setRemoteState 直接更新远程链路状态', async () => {
		const { connection } = await fresh(null);
		connection.setRemoteState('reconnecting', 'heartbeat lost');
		expect(get(connection).remote).toEqual({ state: 'reconnecting', message: 'heartbeat lost' });
	});

	it('togglePause 走后端并同步 paused', async () => {
		let backendPaused = false;
		const { connection } = await fresh((cmd) => {
			if (cmd === 'toggle_pause') { backendPaused = !backendPaused; return backendPaused; }
			return null;
		});
		await expect(connection.togglePause()).resolves.toBeUndefined();
		expect(get(connection).paused).toBe(true);
		await connection.togglePause();
		expect(get(connection).paused).toBe(false);
	});

	it('stopAll/startActiveProfile/stopActiveProfile 走后端并复位 paused', async () => {
		const { connection } = await fresh((cmd) => {
			if (cmd === 'stop_all') return 5;
			if (cmd === 'start_active_profile_scripts') return ['a', 'b'];
			if (cmd === 'stop_active_profile_scripts') return 3;
			return null;
		});
		await expect(connection.stopAll()).resolves.toBe(5);
		await expect(connection.startActiveProfile()).resolves.toBe(2);
		await expect(connection.stopActiveProfile()).resolves.toBe(3);
		expect(get(connection).paused).toBe(false);
	});

	it('startActiveProfile 后端返回非数组时按 0 处理；stopAll 空返回按 0', async () => {
		const { connection } = await fresh((cmd) => {
			if (cmd === 'stop_all') return null;
			if (cmd === 'start_active_profile_scripts') return 'not-an-array';
			return null;
		});
		await expect(connection.stopAll()).resolves.toBe(0);
		await expect(connection.startActiveProfile()).resolves.toBe(0);
	});

	it('refresh 成功：更新版本/paused/远程/runtime 视角', async () => {
		const { connection } = await fresh((cmd) => {
			if (cmd === 'get_system_status') return statusPayload({ paused: true, remote_state: 'reconnecting', ipc_client_connected: false });
			return {};
		});
		const status = await connection.refresh();
		expect(status).toMatchObject({ version: '1.2.3', paused: true });
		const state = get(connection);
		expect(state.paused).toBe(true);
		expect(state.remote?.state).toBe('reconnecting');
		expect(state.runtimeView.ipcClientConnected).toBe(false);
	});
});
