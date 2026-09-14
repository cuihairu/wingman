import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

/**
 * 宏页核心链路（录制/回放/保存/载入）只在 invoke 模式下有真实行为，
 * 而宏页与 connection/macros store 都在模块求值时捕获 invoke，
 * 因此本文件整体重置模块注册表后动态加载全部依赖。
 */

let rtl: typeof import('@testing-library/svelte');
let pageMod: { default: any };

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [connectionMod, macrosMod, logsMod, svelteStore, testingLib, page] = await Promise.all([
		import('$lib/stores/connection'),
		import('$lib/stores/macros'),
		import('$lib/stores/logs'),
		import('svelte/store'),
		import('@testing-library/svelte'),
		import('../src/routes/macros/+page.svelte'),
	]);
	rtl = testingLib as typeof import('@testing-library/svelte');
	pageMod = page as { default: any };
	await connectionMod.connection.connect();
	return {
		connection: connectionMod.connection,
		macros: macrosMod.macros,
		logs: logsMod.logs,
		render: rtl.render,
		screen: rtl.screen,
		fireEvent: rtl.fireEvent,
		waitFor: rtl.waitFor,
		get: svelteStore.get,
	};
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	// fresh import 的 RTL 实例不会被 setup.ts 的自动清理覆盖，需手动卸载
	rtl?.cleanup();
	delete (window as any).__TAURI_INVOKE__;
});

describe('宏录制页面（invoke 模式）', () => {
	it('录制 → 停止 → 回放全链路反映在 UI 与日志', async () => {
		const { render, screen, fireEvent, waitFor, get, logs, macros } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return { server: 'w', version: '1', uptime: 1, running_scripts: 0, paused: false };
			if (cmd === 'macro_record') return null;
			if (cmd === 'macro_stop') return { eventCount: 42 };
			if (cmd === 'macro_play') return null;
			return null;
		});
		render(pageMod.default);

		// 录制
		fireEvent.click(screen.getByRole('button', { name: '开始录制' }));
		await waitFor(() => {
			expect(screen.getByText('正在捕获全局输入')).toBeInTheDocument();
		});
		expect(screen.getByText('录制进行中')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '停止录制' })).toBeInTheDocument();
		expect(screen.getByText('录制中')).toBeInTheDocument(); // 状态指标

		// 停止 → 事件数 42
		fireEvent.click(screen.getByRole('button', { name: '停止录制' }));
		await waitFor(() => {
			expect(screen.getByText('42')).toBeInTheDocument(); // 已捕获指标与 events 计数
		});
		expect(screen.getByText('42 events')).toBeInTheDocument();
		expect(get(logs).some(e => e.message === '停止录制，捕获 42 个事件')).toBe(true);

		// 回放按钮可用；修改速度与重复后回放
		const playBtn = screen.getByRole('button', { name: '开始回放' });
		expect(playBtn).toBeEnabled();
		const numbers = screen.getAllByRole('spinbutton');
		fireEvent.input(numbers[0], { target: { value: '150' } }); // 速度
		fireEvent.input(numbers[1], { target: { value: '3' } }); // 重复
		fireEvent.click(screen.getByRole('button', { name: '开始回放' }));
		await waitFor(() => {
			const messages = get(logs).map(e => e.message);
			expect(messages).toContain('回放宏（速度 150%，重复 3）');
			expect(messages).toContain('宏回放完成');
		});
		expect(get(macros).eventCount).toBe(42);
	});

	it('保存与载入宏文件', async () => {
		const { render, screen, fireEvent, waitFor, get, logs, macros } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return { server: 'w', version: '1', uptime: 1, running_scripts: 0, paused: false };
			if (cmd === 'macro_load') return { eventCount: 7 };
			return null;
		});
		render(pageMod.default);

		// 载入：事件数 7 → 回放可用（占位符相同的保存/载入输入各一，按 DOM 顺序取）
		const pathInputs = screen.getAllByPlaceholderText('D:/macros/m1.json');
		expect(pathInputs).toHaveLength(2); // [0] 保存路径、[1] 载入路径
		fireEvent.input(pathInputs[1], { target: { value: 'D:/macros/m1.json' } });
		fireEvent.click(screen.getByRole('button', { name: '载入宏文件' }));
		await waitFor(() => {
			expect(screen.getByText('7 events')).toBeInTheDocument();
		});
		expect(get(logs).some(e => e.message === '宏已载入: D:/macros/m1.json')).toBe(true);
		expect(screen.getByRole('button', { name: '开始回放' })).toBeEnabled();

		// 保存
		fireEvent.input(pathInputs[0], { target: { value: 'D:/macros/out.json' } });
		fireEvent.click(screen.getByRole('button', { name: '保存为 JSON' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.message === '宏已保存: D:/macros/out.json')).toBe(true);
		});
	});

	it('后端失败路径：录制启动失败与回放失败写错误日志', async () => {
		const { render, screen, fireEvent, waitFor, get, logs } = await fresh((cmd) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_ipc_state') return { connected: true, endpoint: 'e' };
			if (cmd === 'get_system_status') return { server: 'w', version: '1', uptime: 1, running_scripts: 0, paused: false };
			if (cmd === 'macro_record') throw new Error('hook denied');
			if (cmd === 'macro_load') throw new Error('file missing');
			return null;
		});
		render(pageMod.default);

		fireEvent.click(screen.getByRole('button', { name: '开始录制' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message.includes('录制启动失败'))).toBe(true);
		});
		expect(screen.getByText('当前未录制')).toBeInTheDocument();

		const loadInput = screen.getAllByPlaceholderText('D:/macros/m1.json')[1];
		fireEvent.input(loadInput, { target: { value: 'bad.json' } });
		fireEvent.click(screen.getByRole('button', { name: '载入宏文件' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message.includes('载入宏失败'))).toBe(true);
		});
	});
});
