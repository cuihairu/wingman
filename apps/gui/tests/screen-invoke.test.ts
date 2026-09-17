import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * 屏幕预览页（invoke 模式）：显示器列表/选择、截图失败错误面板、失效显示器重置。
 * screen store 在模块求值时捕获 __TAURI_INVOKE__，整体 resetModules 后动态加载。
 */

type Rtl = typeof import('@testing-library/svelte');

let rtl: Rtl;
let get: typeof import('svelte/store').get;

interface Fresh {
	connection: import('$lib/stores/connection')['connection'];
	logs: import('$lib/stores/logs')['logs'];
	screenStore: import('$lib/stores/screen')['screen'];
	render: Rtl['render'];
	screen: Rtl['screen'];
	fireEvent: Rtl['fireEvent'];
	waitFor: Rtl['waitFor'];
	Page: any;
}

let freshRef: Fresh | null = null;

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown): Promise<Fresh> {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [connectionMod, logsMod, screenMod, svelteStore, testingLib, pageMod] = await Promise.all([
		import('$lib/stores/connection'),
		import('$lib/stores/logs'),
		import('$lib/stores/screen'),
		import('svelte/store'),
		import('@testing-library/svelte'),
		import('../src/routes/screen/+page.svelte'),
	]);
	rtl = testingLib as unknown as Rtl;
	get = svelteStore.get;
	freshRef = {
		connection: connectionMod.connection,
		logs: logsMod.logs,
		screenStore: screenMod.screen,
		render: rtl.render,
		screen: rtl.screen,
		fireEvent: rtl.fireEvent,
		waitFor: rtl.waitFor,
		Page: pageMod.default as any,
	};
	return freshRef;
}

function makeShot(width = 800, height = 600) {
	return {
		image: 'data:image/png;base64,shot',
		width,
		height,
		region: { x: 0, y: 0, width, height },
		timestamp: Date.now(),
	};
}

const MONITORS = [
	{ id: 1, name: '主屏', isPrimary: true, bounds: { x: 0, y: 0, width: 1920, height: 1080 } },
	{ id: 2, name: '副屏', isPrimary: false, bounds: { x: 1920, y: 0, width: 1280, height: 720 } },
];

beforeEach(() => {
	(window as any).HTMLCanvasElement.prototype.getContext = vi.fn(() => null);
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	rtl?.cleanup();
	freshRef = null;
	delete (window as any).__TAURI_INVOKE__;
});

describe('屏幕预览页（invoke 模式）', () => {
	it('显示器列表渲染下拉选项，选择后截图携带 displayId', async () => {
		const shots: Array<Record<string, unknown> | undefined>[] = [];
		const args: Array<Record<string, unknown>> = [];
		let currentDisplay: number | null = null;
		const { screenStore, render, screen, fireEvent, waitFor, Page } = await fresh((cmd, a) => {
			if (cmd === 'capture_screenshot') {
				args.push(a as Record<string, unknown>);
				currentDisplay = (a as any)?.displayId ?? null;
				return makeShot();
			}
			if (cmd === 'list_monitors') return MONITORS;
			return {};
		});

		render(Page);
		await waitFor(() => {
			expect(screen.getByText(/2 个/)).toBeInTheDocument();
		});
		// 初始 selectedDisplayId=null → 选中「主显示器」；选项文本：名称 + 主标记 + 分辨率
		const select = screen.getByDisplayValue('主显示器') as HTMLSelectElement;
		const optionTexts = [...select.options].map(o => o.textContent?.trim().replace(/\s+/g, ' '));
		expect(optionTexts[0]).toBe('主显示器');
		expect(optionTexts[1]).toBe('主屏 （主） · 1920x1080');
		expect(optionTexts[2]).toBe('副屏 · 1280x720');

		// 切到副屏（id=2）→ 显式刷新时截图参数携带 displayId=2
		await fireEvent.change(select, { target: { value: '2' } });
		await fireEvent.click(screen.getByRole('button', { name: '刷新截图' }));
		await waitFor(() => {
			expect(args.some(a => a.displayId === 2)).toBe(true);
		});
		expect(currentDisplay).toBe(2);
		expect(screenStore).toBeTruthy();
	});

	it('截图失败显示错误面板并写失败日志，重试成功恢复', async () => {
		let fail = true;
		const { logs, render, screen, fireEvent, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'capture_screenshot') {
				if (fail) throw new Error('display server gone');
				return makeShot();
			}
			if (cmd === 'list_monitors') return [];
			return {};
		});

		render(Page);
		await waitFor(() => {
			expect(screen.getByText('display server gone')).toBeInTheDocument();
		});
		// 错误面板的重试按钮（初始自动截图走 capture() 无日志；点重试 → capture(true)）
		await fireEvent.click(screen.getByRole('button', { name: '重试' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message === '屏幕预览刷新失败: display server gone')).toBe(true);
		});

		fail = false;
		await fireEvent.click(screen.getByRole('button', { name: '重试' }));
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已刷新屏幕预览: 800x600')).toBe(true);
		});
	});

	it('已选显示器从列表消失时重置回主显示器', async () => {
		let monitors = [...MONITORS];
		const { render, screen, fireEvent, waitFor, Page } = await fresh((cmd, a) => {
			if (cmd === 'capture_screenshot') return makeShot();
			if (cmd === 'list_monitors') return monitors;
			return {};
		});

		render(Page);
		await waitFor(() => {
			expect(screen.getByText(/2 个/)).toBeInTheDocument();
		});
		const select = screen.getByDisplayValue('主显示器') as HTMLSelectElement;
		await fireEvent.change(select, { target: { value: '2' } });

		// 副屏拔出：列表只剩主屏 → listMonitors 校验把失效的 selectedDisplayId 重置为 null（主显示器）
		monitors = [MONITORS[0]];
		await fireEvent.click(screen.getByRole('button', { name: '刷新截图' }));
		await waitFor(() => {
			const s = screen.getByDisplayValue('主显示器') as HTMLSelectElement;
			expect([...s.options].some(o => o.value === '2')).toBe(false);
		});
	});

	it('list_monitors 失败显示错误提示', async () => {
		const { render, screen, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'capture_screenshot') return makeShot();
			if (cmd === 'list_monitors') throw new Error('xrandr missing');
			return {};
		});
		render(Page);
		await waitFor(() => {
			expect(screen.getByText(/xrandr missing/)).toBeInTheDocument();
		});
	});

	it('显示器无名称且非主屏时回退「显示器 id」显示', async () => {
		const { render, screen, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'capture_screenshot') return makeShot();
			if (cmd === 'list_monitors') {
				return [
					{ id: 1, name: '主屏', isPrimary: true, bounds: { x: 0, y: 0, width: 1920, height: 1080 } },
					{ id: 3, name: '', isPrimary: false, bounds: { x: 0, y: 0, width: 1024, height: 768 } },
				];
			}
			return {};
		});
		render(Page);
		await waitFor(() => {
			const select = screen.getByDisplayValue('主显示器') as HTMLSelectElement;
			const option = [...select.options].map(o => o.textContent?.trim().replace(/\s+/g, ' '));
			expect(option).toContain('显示器 3 · 1024x768');
		});
	});

	it('截图与显示器枚举抛出非错误对象时错误消息取字符串形式', async () => {
		const { render, screen, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'capture_screenshot') throw 'display raw failure';
			if (cmd === 'list_monitors') throw 'monitor raw failure';
			return {};
		});
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('display raw failure')).toBeInTheDocument();
			expect(screen.getByText(/monitor raw failure/)).toBeInTheDocument();
		});
	});

	it('重试进行中主按钮切换为「刷新中」', async () => {
		let release!: (v: unknown) => void;
		const gate = new Promise(r => { release = r; });
		let fail = true;
		const { render, screen, fireEvent, waitFor, Page } = await fresh((cmd) => {
			if (cmd === 'capture_screenshot') {
				if (fail) throw new Error('first boom');
				return gate;
			}
			if (cmd === 'list_monitors') return [];
			return {};
		});
		render(Page);
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '重试' })).toBeInTheDocument();
		});

		// 重试挂起 → loading=true：error 面板清空，主按钮变「刷新中」
		fail = false;
		await fireEvent.click(screen.getByRole('button', { name: '重试' }));
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '刷新中' })).toBeDisabled();
		});
		release(makeShot());
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
	});
});
