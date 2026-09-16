import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * 设置页（invoke 模式）：连接区块的状态分支文案与「立即重试」。
 * connection 在模块求值时捕获 __TAURI_INVOKE__，整体 resetModules 后动态加载。
 */

type Rtl = typeof import('@testing-library/svelte');

let rtl: Rtl;
let get: typeof import('svelte/store').get;

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [connectionMod, logsMod, svelteStore, testingLib, pageMod] = await Promise.all([
		import('$lib/stores/connection'),
		import('$lib/stores/logs'),
		import('svelte/store'),
		import('@testing-library/svelte'),
		import('../src/routes/settings/+page.svelte'),
	]);
	rtl = testingLib as unknown as Rtl;
	get = svelteStore.get;
	return {
		connection: connectionMod.connection,
		logs: logsMod.logs,
		Page: pageMod.default as any,
		render: rtl.render,
		screen: rtl.screen,
		fireEvent: rtl.fireEvent,
		waitFor: rtl.waitFor,
	};
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	rtl?.cleanup();
	delete (window as any).__TAURI_INVOKE__;
});

function statusPayload(extra: Record<string, unknown> = {}) {
	return { server: 'wingman', version: '1.2.3', uptime: 1, running_scripts: 0, paused: false, ...extra };
}

describe('设置页连接区块（invoke 模式）', () => {
	it('未连接渲染默认按钮与双未知视图', async () => {
		const { render, screen, Page } = await fresh(() => { throw new Error('unreachable'); });
		render(Page);
		expect(screen.getByText('连接')).toBeInTheDocument();
		expect(screen.getByText('未连接')).toBeInTheDocument();
		expect(screen.getByText(/未知（IPC 未连接）/)).toBeInTheDocument();
		expect(screen.getByText(/未知（等待状态同步）/)).toBeInTheDocument();
	});

	it('连接错误：错误文案、消息与立即重试', async () => {
		let connectOk = false;
		const { connection, logs, render, screen, fireEvent, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') {
				if (!connectOk) throw new Error('dial unix: no such file');
				return {};
			}
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'wingman' };
			if (cmd === 'get_system_status') return statusPayload({ remote_state: 'connected', ipc_client_connected: true });
			return {};
		});
		await expect(connection.connect()).rejects.toThrow('dial unix: no such file');

		render(Page);
		expect(screen.getByText('连接错误')).toBeInTheDocument();
		expect(screen.getByText('连接')).toBeInTheDocument();
		// ipc 错误消息面板
		expect(screen.getByText('dial unix: no such file')).toBeInTheDocument();
		// runtime 视角（未连接）与远程链路（未同步）
		expect(screen.getByText(/未知（IPC 未连接）/)).toBeInTheDocument();
		expect(screen.getByText(/未知（等待状态同步）/)).toBeInTheDocument();
		// 实际端点
		expect(screen.getByText(/实际端点: wingman/)).toBeInTheDocument();

		// 立即重试成功 → 已连接 + runtime/remote 视图刷新
		connectOk = true;
		await fireEvent.click(screen.getByRole('button', { name: '立即重试' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已重新连接到本地 runtime IPC')).toBe(true);
		});
		await waitFor(() => {
			expect(screen.getByText('已连接')).toBeInTheDocument();
			expect(screen.getByText(/IPC 客户端在线/)).toBeInTheDocument();
			expect(screen.getByText(/远程链路: 已连接/)).toBeInTheDocument();
		});
	});

	it('连接中显示「连接中」状态与禁用的重试按钮', async () => {
		let release!: (v: unknown) => void;
		const gate = new Promise(r => { release = r; });
		const { render, screen, Page } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return gate;
			return {};
		});
		const { connection } = await import('$lib/stores/connection');
		const pending = connection.connect();
		await new Promise(r => setTimeout(r, 50));

		render(Page);
		expect(screen.getByText('连接中')).toBeInTheDocument();
		// connecting 状态下 canRetry=false，重试按钮不可用（jsdom 点击也不触发）
		const retry = screen.queryByRole('button', { name: /重试|连接/ });
		if (retry) expect(retry).toBeDisabled();

		release({});
		await pending;
	});

	it('重连中显示尝试次数、退避说明与重试失败日志', async () => {
		let statusOk = true;
		const { connection, logs, render, screen, fireEvent, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'wingman' };
			if (cmd === 'get_system_status') {
				if (!statusOk) throw new Error('timeout');
				return statusPayload({ remote_state: 'error', ipc_client_connected: false });
			}
			return {};
		});
		await connection.connect();
		// runtime 视角：已连接但 IPC 客户端离线；远程链路 error 状态透传
		render(Page);
		await waitFor(() => {
			expect(screen.getByText(/IPC 客户端离线/)).toBeInTheDocument();
			expect(screen.getByText(/远程链路: 连接错误/)).toBeInTheDocument();
			expect(screen.getByText('断开')).toBeInTheDocument();
		});

		// 心跳失败 → reconnecting；断线后 remote 未知（后台 1s 自动重连可能已推进次数）
		statusOk = false;
		await connection.refresh();
		await waitFor(() => {
			expect(screen.getByRole('button', { name: /重连中 \(\d+\)/ })).toBeInTheDocument();
			expect(screen.getByText('自动重连中')).toBeInTheDocument();
			expect(screen.getByText(/自动重连第 \d+ 次/)).toBeInTheDocument();
			// handleConnectionLost 清空 remote → 等待状态同步
			expect(screen.getByText(/未知（等待状态同步）/)).toBeInTheDocument();
		});

		// 立即重试（connect_ipc 成功但 get_system_status 仍失败 → 重连失败日志）
		await fireEvent.click(screen.getByRole('button', { name: '立即重试' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message.includes('重连失败'))).toBe(true);
		});
	});
});
