import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * 宏页第二组（invoke 模式）：回放中的重复回放守卫、清空会话与刷新状态。
 * 录制/回放/保存/载入主链路见 macros-invoke.test.ts。
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
	rtl?.cleanup();
	delete (window as any).__TAURI_INVOKE__;
});

describe('宏页（invoke 模式）第二组', () => {
	it('回放进行中再次点击回放被守卫拦截，完成后复位', async () => {
		let release!: (v: unknown) => void;
		const gate = new Promise(r => { release = r; });
		let plays = 0;
		const { render, screen, fireEvent, waitFor, get } = await fresh((cmd) => {
			if (cmd === 'macro_play') {
				plays++;
				return gate;
			}
			if (cmd === 'macro_status') return { recording: false, eventCount: 2 };
			return {};
		});

		render(pageMod.default);
		const playBtn = screen.getByRole('button', { name: '开始回放' });
		await fireEvent.click(playBtn);
		// playing=true：按钮禁用且文案切换为「回放中」
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '回放中' })).toBeDisabled();
		});
		await fireEvent.click(screen.getByRole('button', { name: '回放中' }));
		expect(plays).toBe(1);

		release({});
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '开始回放' })).toBeInTheDocument();
			expect(screen.getByText('空闲')).toBeInTheDocument();
		});
	});

	it('事件数非零时清空会话并复位计数', async () => {
		let cleared = false;
		const { macros, render, screen, fireEvent, waitFor, get, logs } = await fresh((cmd) => {
			if (cmd === 'macro_status') return { recording: false, eventCount: cleared ? 0 : 5 };
			if (cmd === 'macro_clear') {
				cleared = true;
				return {};
			}
			return {};
		});

		render(pageMod.default);
		// 等挂载时的 macro_status 刷新（eventCount=5）解除禁用
		await waitFor(() => {
			expect(get(macros).eventCount).toBe(5);
		});
		const clearBtn = screen.getByRole('button', { name: '清空会话' });
		expect(clearBtn).not.toBeDisabled();
		await fireEvent.click(clearBtn);
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'info' && e.message === '已清空录制')).toBe(true);
		});
		await waitFor(() => {
			expect(get(macros).eventCount).toBe(0);
		});
	});

	it('刷新状态按钮重新拉取宏状态', async () => {
		let count = 3;
		const { macros, render, screen, fireEvent, waitFor, get } = await fresh((cmd) => {
			if (cmd === 'macro_status') return { recording: false, eventCount: count };
			return {};
		});

		render(pageMod.default);
		await waitFor(() => {
			expect(get(macros).eventCount).toBe(3);
		});

		count = 8;
		await fireEvent.click(screen.getByRole('button', { name: '刷新状态' }));
		await waitFor(() => {
			expect(get(macros).eventCount).toBe(8);
		});
	});
});
