import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * invoke 模式的 TopBar 需要整体重置模块注册表（connection 在模块求值时捕获
 * __TAURI_INVOKE__，组件又要与 store 使用同一份 Svelte 运行时），因此本文件
 * 内所有依赖（含 @testing-library/svelte）都在 resetModules 之后动态加载。
 */

type Rtl = typeof import('@testing-library/svelte');

let rtl: Rtl;
let get: typeof import('svelte/store').get;

interface Fresh {
	connection: import('$lib/stores/connection')['connection'];
	logs: import('$lib/stores/logs')['logs'];
	render: Rtl['render'];
	screen: Rtl['screen'];
	fireEvent: Rtl['fireEvent'];
	waitFor: Rtl['waitFor'];
	TopBar: Rtl extends { render: (c: infer C, o?: any) => any } ? C : never;
}

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown): Promise<Fresh> {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [connectionMod, logsMod, svelteStore, testingLib, topBarMod] = await Promise.all([
		import('$lib/stores/connection'),
		import('$lib/stores/logs'),
		import('svelte/store'),
		import('@testing-library/svelte'),
		import('../src/lib/components/layout/TopBar.svelte'),
	]);
	rtl = testingLib as unknown as Rtl;
	get = svelteStore.get;
	return {
		connection: connectionMod.connection,
		logs: logsMod.logs,
		render: rtl.render,
		screen: rtl.screen,
		fireEvent: rtl.fireEvent,
		waitFor: rtl.waitFor,
		TopBar: topBarMod.default as any,
	};
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	delete (window as any).__TAURI_INTERNALS__;
});

afterEach(() => {
	// fresh import 的 RTL 实例不会被 setup.ts 的自动清理覆盖，需手动卸载
	rtl?.cleanup();
	delete (window as any).__TAURI_INVOKE__;
	delete (window as any).__TAURI_INTERNALS__;
});

function statusPayload(extra: Record<string, unknown> = {}) {
	return { server: 'wingman', version: '1.2.3', uptime: 1, running_scripts: 0, paused: false, ...extra };
}

describe('TopBar（invoke 模式）', () => {
	it('连接错误显示错误态，点击状态按钮重试成功', async () => {
		let fail = true;
		const { connection, logs, render, screen, fireEvent, waitFor, TopBar } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') {
				if (fail) throw new Error('dial tcp: refused');
				return {};
			}
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return statusPayload();
			return {};
		});
		await expect(connection.connect()).rejects.toThrow('dial tcp: refused');

		render(TopBar);
		expect(screen.getByText('连接错误')).toBeInTheDocument();
		expect(document.querySelector('.status-dot.error')).toBeInTheDocument();

		fail = false;
		fireEvent.click(screen.getByText('连接错误'));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已重新连接到本地 runtime IPC')).toBe(true);
		});
		expect(get(connection).ipc.state).toBe('connected');
		await waitFor(() => {
			expect(screen.getByText('已连接')).toBeInTheDocument();
		});
	});

	it('重连中显示尝试次数与提示；重试恢复连接', async () => {
		let fail = false;
		const { connection, render, screen, fireEvent, waitFor, TopBar } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') {
				if (fail) throw new Error('timeout');
				return statusPayload();
			}
			return {};
		});
		await connection.connect();
		fail = true;
		await connection.refresh(); // 心跳失败 → reconnecting, attempts=1

		render(TopBar);
		expect(screen.getByText('重连中 (1)')).toBeInTheDocument();
		const statusBtn = screen.getByText(/重连中/).closest('button')!;
		expect(statusBtn.getAttribute('title')).toContain('第 1 次重连（指数退避）');

		fail = false;
		fireEvent.click(statusBtn);
		await waitFor(() => {
			expect(get(connection).ipc.state).toBe('connected');
		});
	});

	it('重试失败记录错误日志', async () => {
		const { connection, logs, render, screen, fireEvent, waitFor, TopBar } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') throw new Error('down');
			return {};
		});
		await expect(connection.connect()).rejects.toThrow('down');

		render(TopBar);
		fireEvent.click(screen.getByText('连接错误'));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message.includes('重连失败'))).toBe(true);
		});
	});
});
