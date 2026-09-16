import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * App 外壳第二组（invoke 模式）：首次直连成功日志、autoStart 关闭守卫、
 * 自启脚本失败路径、5s 心跳的静默与断线日志。
 * 退避重连主链路见 app-invoke.test.ts。
 */

type Rtl = typeof import('@testing-library/svelte');

let rtl: Rtl;
let get: typeof import('svelte/store').get;

async function fresh(
	handler: (cmd: string, args?: Record<string, unknown>) => unknown,
	opts: { autoStart?: boolean } = {},
) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	localStorage.setItem('wingman-settings', JSON.stringify({ autoStart: opts.autoStart ?? false }));
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [connectionMod, logsMod, scriptsMod, svelteStore, testingLib, appMod] = await Promise.all([
		import('$lib/stores/connection'),
		import('$lib/stores/logs'),
		import('$lib/stores/scripts'),
		import('svelte/store'),
		import('@testing-library/svelte'),
		import('../src/App.svelte'),
	]);
	rtl = testingLib as unknown as Rtl;
	get = svelteStore.get;
	return {
		connection: connectionMod.connection,
		logs: logsMod.logs,
		scripts: scriptsMod.scripts,
		App: appMod.default as any,
		render: rtl.render.bind(rtl),
		waitFor: rtl.waitFor,
		cleanup: rtl.cleanup.bind(rtl),
	};
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	rtl?.cleanup();
	delete (window as any).__TAURI_INVOKE__;
});

const PROFILE_WITH_AUTOSTART = [{
	id: 'p1', name: '自启配置', version: '1.0', description: '',
	window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
	colors: [], images: [], triggers: [],
	scripts: [{ name: 'auto.lua', path: 'scripts/auto.lua', autoStart: true, restartOnCrash: false, priority: 0 }],
	hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
	settings: {},
}];

describe('App 外壳（invoke 模式）第二组', () => {
	it('首次直连成功：记录成功日志，autoStart 关闭时不自启脚本', { timeout: 20_000 }, async () => {
		const startCalls: string[] = [];
		const { render, waitFor, cleanup, logs, connection, App } = await fresh((cmd, args) => {
			switch (cmd) {
				case 'connect_ipc': return {};
				case 'get_ipc_state': return { connected: true, endpoint: 'wingman' };
				case 'get_system_status':
					return { server: 'wingman', version: '1.0', uptime: 5, running_scripts: 0, paused: false };
				case 'get_profiles': return PROFILE_WITH_AUTOSTART;
				case 'get_active_profile': return { id: 'p1' };
				case 'get_scripts': return [];
				case 'start_script':
					startCalls.push((args as any)?.id);
					return {};
				default: return {};
			}
		}, { autoStart: false });

		try {
			render(App);
			await waitFor(() => {
				expect(get(logs).some(e => e.type === 'success' && e.message === '已自动连接到本地 runtime IPC')).toBe(true);
			});
			expect(get(connection).ipc.state).toBe('connected');
			// autoStart=false → autoStartProfileScripts 直接 return，不发起 start_script
			await new Promise(r => setTimeout(r, 300));
			expect(startCalls).toEqual([]);
			expect(get(logs).some(e => e.message.includes('已自动启动脚本'))).toBe(false);
		} finally {
			cleanup();
		}
	});

	it('自启脚本 start 失败记录错误日志', { timeout: 20_000 }, async () => {
		const { render, waitFor, cleanup, logs, App } = await fresh((cmd) => {
			switch (cmd) {
				case 'connect_ipc': return {};
				case 'get_ipc_state': return { connected: true, endpoint: 'wingman' };
				case 'get_system_status':
					return { server: 'wingman', version: '1.0', uptime: 5, running_scripts: 0, paused: false };
				case 'get_profiles': return PROFILE_WITH_AUTOSTART;
				case 'get_active_profile': return { id: 'p1' };
				case 'get_scripts': return [];
				case 'start_script': throw new Error('script load error');
				default: return {};
			}
		}, { autoStart: true });

		try {
			render(App);
			await waitFor(() => {
				expect(get(logs).some(e => e.type === 'error' && e.message.includes('自动启动脚本失败 auto.lua'))).toBe(true);
			});
		} finally {
			cleanup();
		}
	});

	it('心跳：未连接的 tick 静默返回', { timeout: 20_000 }, async () => {
		const { render, waitFor, cleanup, logs, App } = await fresh(() => {
			throw new Error('ipc nowhere');
		});

		try {
			render(App);
			await waitFor(() => {
				expect(get(logs).some(e => e.message.includes('自动连接失败'))).toBe(true);
			});
			// 5s 心跳 tick 时未连接 → 静默（不写「连接中断」日志）
			await new Promise(r => setTimeout(r, 5400));
			expect(get(logs).some(e => e.message.includes('与本地 runtime 的连接中断'))).toBe(false);
		} finally {
			cleanup();
		}
	});

	it('心跳：连接中 refresh 失败写断线日志', { timeout: 20_000 }, async () => {
		let statusOk = true;
		const { render, waitFor, cleanup, logs, connection, App } = await fresh((cmd) => {
			switch (cmd) {
				case 'connect_ipc': return {};
				case 'get_ipc_state': return { connected: true, endpoint: 'wingman' };
				case 'get_system_status': {
					if (!statusOk) throw new Error('status down');
					return { server: 'wingman', version: '1.0', uptime: 5, running_scripts: 0, paused: false };
				}
				case 'get_profiles': return [];
				case 'get_active_profile': return null;
				case 'get_scripts': return [];
				default: return {};
			}
		});

		try {
			render(App);
			await waitFor(() => {
				expect(get(connection).ipc.state).toBe('connected');
			});
			// 等 dashboard 的 3s 自动刷新跑完一轮，让 5s 心跳 tick 成为首个失败的 refresh
			await new Promise(r => setTimeout(r, 3600));
			statusOk = false;
			await waitFor(() => {
				expect(get(logs).some(e => e.message.includes('与本地 runtime 的连接中断'))).toBe(true);
			}, { timeout: 8000 });
		} finally {
			cleanup();
		}
	});
});
