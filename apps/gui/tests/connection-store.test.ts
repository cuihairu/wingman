import { describe, it, expect, vi } from 'vitest';
import { get } from 'svelte/store';

type ConnectionStore = Awaited<typeof import('$lib/stores/connection')>['connection'];
type ConnectionState = ReturnType<typeof get<ConnectionStore>>;

const INVOKE_KEY = '__TAURI_INVOKE__';

/// 以可控的 invoke mock 重新加载 connection store（invoke 的第一个参数是命令名字符串）
async function freshStore(handler: (command: string, args?: Record<string, unknown>) => unknown): Promise<ConnectionStore> {
	vi.resetModules();
	(window as any)[INVOKE_KEY] = vi.fn(handler);
	const mod = await import('$lib/stores/connection');
	return mod.connection;
}

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

describe('connection store 远程链路状态合并（IPC 连接状态改进）', () => {

	it('refresh() 合并 system.getStatus 的 remote_state，事件陈旧问题被心跳纠正', async () => {
		let remoteState = 'connected';
		const store = await freshStore((command) => {
			if (command === 'connect_ipc') return {};
			if (command === 'get_ipc_state') return { connected: true, endpoint: 'unix:/tmp/w' };
			if (command === 'get_system_status') return statusPayload({ remote_state: remoteState });
			return {};
		});

		await store.connect('wingman');
		expect(get(store).remote?.state).toBe('connected');

		// 远程链路在 runtime 侧断开：下一次心跳（refresh）即纠正徽标
		remoteState = 'disconnected';
		await store.refresh();
		expect(get(store).remote?.state).toBe('disconnected');
	});

	it('refresh() 记录 runtime 视角 ipc_client_connected；旧 runtime 缺字段时回退 null', async () => {
		let ipcClient: boolean | null = true;
		const store = await freshStore((command) => {
			if (command === 'get_system_status')
				return statusPayload(
					ipcClient === null ? {} : { ipc_client_connected: ipcClient, remote_state: 'connected' },
				);
			return {};
		});

		await store.refresh();
		expect(get(store).runtimeView.ipcClientConnected).toBe(true);

		ipcClient = false;
		await store.refresh();
		expect(get(store).runtimeView.ipcClientConnected).toBe(false);

		ipcClient = null; // 旧 runtime 不回报该字段
		await store.refresh();
		expect(get(store).runtimeView.ipcClientConnected).toBeNull();
	});

	it('心跳失败清空 remote 状态并进入 error + 自动重连调度', async () => {
		let fail = false;
		const store = await freshStore((command) => {
			if (command === 'connect_ipc') return {};
			if (command === 'get_ipc_state') return { connected: true, endpoint: 'unix:/tmp/w' };
			if (command === 'get_system_status') {
				if (fail) throw new Error('IPC read timeout');
				return statusPayload({ remote_state: 'connected' });
			}
			return {};
		});

		await store.connect('wingman');
		expect(get(store).remote?.state).toBe('connected');

		fail = true;
		const result = await store.refresh();
		expect(result).toBeNull();
		const state: ConnectionState = get(store);
		// error 为瞬时态：同 tick 内即被 scheduleReconnect 迁移为 reconnecting
		expect(['error', 'reconnecting']).toContain(state.ipc.state);
		expect(state.ipc.message).toContain('IPC read timeout');
		expect(state.remote).toBeNull(); // 断线后远程状态未知，清空避免陈旧
		expect(state.ipc.attempts).toBeGreaterThan(0); // 已调度重连
	});

	it('握手阶段（connect）即填充远程状态，GUI 重启后无需等待事件', async () => {
		const store = await freshStore((command) => {
			if (command === 'connect_ipc') return {};
			if (command === 'get_ipc_state') return { connected: true, endpoint: 'unix:/tmp/w' };
			if (command === 'get_system_status')
				return statusPayload({ remote_state: 'reconnecting' });
			return {};
		});

		await store.connect('wingman');
		expect(get(store).remote?.state).toBe('reconnecting');
		expect(get(store).version).toBe('1.2.3');
	});
});
