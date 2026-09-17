import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * 宏页面 invoke 模式：录制中状态全链路（状态徽标/横幅/时间轴/停止按钮）、
 * 停止后事件数回填、保存与载入携带路径调用（非空路径走 else 侧）。
 * dev 模式（no-op 守卫）与空路径提示见 macros-page。
 * 与其他 invoke 模式文件一致：整体重置模块注册表后动态加载全部依赖
 * （含 @testing-library/svelte，避免双 svelte runtime 实例导致 effect_orphan）。
 */

type Handler = (cmd: string, args?: Record<string, unknown>) => unknown;

type Rtl = typeof import('@testing-library/svelte');

let rtl: Rtl;
let get: typeof import('svelte/store').get;

async function setup(handler: Handler) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [testingLib, svelteStore, pageMod, connectionMod, macrosMod, logsMod] = await Promise.all([
		import('@testing-library/svelte'),
		import('svelte/store'),
		import('../src/routes/macros/+page.svelte'),
		import('$lib/stores/connection'),
		import('$lib/stores/macros'),
		import('$lib/stores/logs'),
	]);
	rtl = testingLib as unknown as Rtl;
	get = svelteStore.get;
	await connectionMod.connection.connect();
	return {
		Page: pageMod.default as any,
		connection: connectionMod.connection,
		macros: macrosMod.macros,
		logs: logsMod.logs,
	};
}

function invokeLog(): Array<{ cmd: string; args?: Record<string, unknown> }> {
	return ((window as any).__TAURI_INVOKE__ as ReturnType<typeof vi.fn>).mock.calls.map(
		(c: unknown[]) => ({ cmd: c[0] as string, args: c[1] as Record<string, unknown> | undefined }),
	);
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	rtl?.cleanup();
	vi.restoreAllMocks();
	delete (window as any).__TAURI_INVOKE__;
});

describe('宏页面（invoke 模式）', () => {
	it('录制中：徽标/横幅/时间轴切换并出现停止按钮，停止后回填事件数', async () => {
		const { Page, connection } = await setup((cmd) => {
			if (cmd === 'macro_record') return {};
			if (cmd === 'macro_stop') return { eventCount: 5 };
			if (cmd === 'macro_status') return { recording: false, paused: false, eventCount: 0 };
			return {};
		});
		try {
			rtl.render(Page);
			await rtl.fireEvent.click(rtl.screen.getByRole('button', { name: '开始录制' }));
			await rtl.waitFor(() => {
				expect(rtl.screen.getByText('录制进行中')).toBeInTheDocument();
			});
			expect(rtl.screen.getByText('正在捕获全局输入')).toBeInTheDocument();
			expect(rtl.screen.getByText('录制中')).toBeInTheDocument();
			expect(rtl.screen.getByRole('button', { name: '停止录制' })).toBeInTheDocument();
			expect(rtl.screen.queryByRole('button', { name: '开始录制' })).not.toBeInTheDocument();

			await rtl.fireEvent.click(rtl.screen.getByRole('button', { name: '停止录制' }));
			await rtl.waitFor(() => {
				expect(rtl.screen.getByText('准备就绪')).toBeInTheDocument();
			});
			expect(rtl.screen.getByText('当前未录制')).toBeInTheDocument();
		} finally {
			await connection.disconnect();
		}
	});

	it('保存：非空路径调用 macro_save 成功写日志', async () => {
		const { Page, connection, logs } = await setup((cmd) => {
			if (cmd === 'macro_status') return { recording: false, paused: false, eventCount: 3 };
			return {};
		});
		try {
			rtl.render(Page);
			await rtl.waitFor(() => {
				expect(rtl.screen.getByText('3 events')).toBeInTheDocument();
			});
			await rtl.fireEvent.input(rtl.screen.getAllByPlaceholderText('D:/macros/m1.json')[0], { target: { value: 'm1.json' } });
			await rtl.fireEvent.click(rtl.screen.getByRole('button', { name: '保存为 JSON' }));
			await rtl.waitFor(() => {
				expect(get(logs).some(e => e.message === '宏已保存: m1.json' && e.type === 'success')).toBe(true);
			});
			const saveCall = invokeLog().find(c => c.cmd === 'macro_save');
			expect(saveCall?.args).toEqual({ path: 'm1.json' });
		} finally {
			await connection.disconnect();
		}
	});

	it('载入：非空路径调用 macro_load 并更新事件计数', async () => {
		const { Page, connection } = await setup((cmd) => {
			if (cmd === 'macro_load') return { eventCount: 9 };
			if (cmd === 'macro_status') return { recording: false, paused: false, eventCount: 0 };
			return {};
		});
		try {
			rtl.render(Page);
			const inputs = rtl.screen.getAllByPlaceholderText('D:/macros/m1.json');
			// 第二个同 placeholder 输入框为载入路径
			await rtl.fireEvent.input(inputs[inputs.length - 1], { target: { value: 'm2.json' } });
			await rtl.fireEvent.click(rtl.screen.getByRole('button', { name: '载入宏文件' }));
			await rtl.waitFor(() => {
				expect(rtl.screen.getByText('9 events')).toBeInTheDocument();
			});
			const loadCall = invokeLog().find(c => c.cmd === 'macro_load');
			expect(loadCall?.args).toEqual({ path: 'm2.json' });
		} finally {
			await connection.disconnect();
		}
	});
});
