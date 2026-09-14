import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * App 外壳（invoke 模式）：自动连接失败 → 指数退避重连 → 恢复后重载数据并自启脚本。
 * 与其他 invoke 模式文件一致：整体重置模块注册表后动态加载全部依赖。
 *
 * 重连延迟仅 1s/2s 真实时间，直接用真实定时器 + waitFor 断言，
 * 避免在 worker 内安装 fake timers 与 vite 模块加载/渲染管线相互干扰。
 */

type Rtl = typeof import('@testing-library/svelte');

let rtl: Rtl;
let get: typeof import('svelte/store').get;

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	// 自启脚本依赖设置开关
	localStorage.setItem('wingman-settings', JSON.stringify({ autoStart: true }));
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

describe('App 外壳（invoke 模式）', () => {
	// 真实定时器退避（1s+2s 重连）贴近默认 5s 超时，慢机上会误报，放宽到 20s
	it('自动连接失败进入指数退避重连，恢复后重载数据并自启脚本', { timeout: 20_000 }, async () => {
		let connectCalls = 0;
		const startCalls: string[] = [];
		const { render, waitFor, cleanup, logs, connection, scripts, App } = await fresh((cmd, args) => {
			switch (cmd) {
				case 'connect_ipc':
					connectCalls++;
					if (connectCalls <= 2) throw new Error('dial tcp 127.0.0.1: refused');
					return {};
				case 'get_ipc_state':
					return { connected: connectCalls > 2, endpoint: 'wingman' };
				case 'get_system_status':
					return { server: 'wingman', version: '1.0', uptime: 5, running_scripts: 1, paused: false };
				case 'get_profiles':
					return [{
						id: 'p1', name: '自启配置', version: '1.0', description: '',
						window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
						colors: [], images: [], triggers: [],
						scripts: [{ name: 'auto.lua', path: 'scripts/auto.lua', autoStart: true, restartOnCrash: false, priority: 0 }],
						hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
						settings: {},
					}];
				case 'get_active_profile':
					return { id: 'p1' };
				case 'get_scripts':
					return [{ id: 'auto.lua', name: 'auto.lua', path: 'scripts/auto.lua', size: 12, is_running: true, state: 'running', error: '', loaded_at: 1 }];
				case 'start_script':
					startCalls.push((args as any)?.id);
					return {};
				default:
					return {};
			}
		});

		try {
			render(App);

			// 初始连接失败 → 警告日志 + 调度重连（attempts=1，1s 后第一次重试）
			await waitFor(() => {
				expect(get(logs).some(e => e.type === 'warning' && e.message.includes('自动连接失败'))).toBe(true);
			});
			expect(get(connection).ipc.state).toBe('reconnecting');
			expect(get(connection).ipc.attempts).toBe(1);

			// 第 1 次重连（1s 后）失败 → attempts=2 并记录退避日志
			await waitFor(() => {
				expect(get(connection).ipc.attempts).toBe(2);
			});
			expect(get(logs).some(e => e.message.includes('自动重连失败（第 1 次）'))).toBe(true);

			// 第 2 次重连（2s 后）成功 → 数据重载 + 自启脚本
			await waitFor(() => {
				expect(get(connection).ipc.state).toBe('connected');
			}, { timeout: 5000 });
			await waitFor(() => {
				expect(startCalls).toEqual(['auto.lua']);
			}, { timeout: 5000 });
			expect(get(logs).some(e => e.type === 'success' && e.message === '已自动启动脚本: auto.lua')).toBe(true);
			expect(get(scripts).some(s => s.name === 'auto.lua')).toBe(true);
			// 初始失败与退避日志仍在
			expect(get(logs).some(e => e.type === 'warning' && e.message.includes('自动连接失败'))).toBe(true);
		} finally {
			cleanup();
		}
	});
});
