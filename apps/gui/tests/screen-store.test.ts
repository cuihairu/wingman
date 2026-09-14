import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type ScreenModule = Awaited<typeof import('$lib/stores/screen')>;
type ScreenStore = ScreenModule['screen'];
type ScreenState = ReturnType<typeof get<ScreenStore>>;

const INVOKE_KEY = '__TAURI_INVOKE__';

async function fresh(): Promise<ScreenModule> {
	vi.resetModules();
	return import('$lib/stores/screen');
}

describe('screen store（dev 模式，无 __TAURI_INVOKE__）', () => {
	beforeEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('capture 生成开发预览截图（SVG data URL），region 尺寸为 0 时回退默认 1280x720', async () => {
		const { screen } = await fresh();
		const shot = await screen.capture({ x: 5, y: 6, width: 0, height: 0 });
		expect(shot).not.toBeNull();
		expect(shot!.image).toMatch(/^data:image\/svg\+xml/);
		expect(shot!.width).toBe(1280);
		expect(shot!.height).toBe(720);
		expect(shot!.region).toEqual({ x: 5, y: 6, width: 1280, height: 720 });

		const state = get(screen);
		expect(state.current).not.toBeNull();
		expect(state.loading).toBe(false);
		expect(state.error).toBe('');
		expect(state.lastUpdated).toMatch(/\d{1,2}:\d{2}:\d{2}/);
	});

	it('capture 遵循非零 region 尺寸', async () => {
		const { screen } = await fresh();
		const shot = await screen.capture({ x: 1, y: 2, width: 320, height: 240 });
		expect(shot!.width).toBe(320);
		expect(shot!.height).toBe(240);
	});

	it('listMonitors 提示 runtime 未连接', async () => {
		const { screen } = await fresh();
		await expect(screen.listMonitors()).resolves.toEqual([]);
		expect(get(screen).monitorsError).toContain('runtime 未连接');
	});

	it('clear 重置全部状态', async () => {
		const { screen } = await fresh();
		await screen.capture();
		screen.clear();
		expect(get(screen)).toEqual({
			current: null, loading: false, error: '', lastUpdated: '-', monitors: [], monitorsError: '',
		});
	});

	it('loadDevData 直接填充预览截图', async () => {
		const { screen } = await fresh();
		screen.loadDevData();
		expect(get(screen).current).not.toBeNull();
	});
});

describe('screen store（invoke 模式）', () => {
	let invokeMock: ReturnType<typeof vi.fn>;

	beforeEach(async () => {
		invokeMock = vi.fn();
		(window as any)[INVOKE_KEY] = invokeMock;
	});

	afterEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('capture 调用 capture_screenshot 并保留显示器列表', async () => {
		const { screen } = await fresh();
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'capture_screenshot') {
				return { image: 'data:image/png;base64,x', width: 100, height: 80, timestamp: 1234, region: { x: 0, y: 0, width: 100, height: 80 } };
			}
			if (cmd === 'list_monitors') {
				return [{ id: 0, name: 'Display 0', isPrimary: true, bounds: { x: 0, y: 0, width: 1920, height: 1080 } }];
			}
			return null;
		});
		await screen.listMonitors();
		const shot = await screen.capture({ x: 0, y: 0, width: 0, height: 0 }, 1);
		expect(invokeMock).toHaveBeenCalledWith('capture_screenshot', { region: { x: 0, y: 0, width: 0, height: 0 }, displayId: 1 });
		expect(shot!.width).toBe(100);
		const state = get(screen);
		expect(state.monitors).toHaveLength(1); // 截图后显示器列表不丢
		expect(state.lastUpdated).toMatch(/\d{1,2}:\d{2}:\d{2}/);
	});

	it('capture displayId 缺省时传 null（主显示器兼容）', async () => {
		const { screen } = await fresh();
		invokeMock.mockResolvedValue({ image: '', width: 1, height: 1, timestamp: 0, region: { x: 0, y: 0, width: 1, height: 1 } });
		await screen.capture({ x: 0, y: 0, width: 0, height: 0 });
		expect(invokeMock).toHaveBeenCalledWith('capture_screenshot', { region: { x: 0, y: 0, width: 0, height: 0 }, displayId: null });
	});

	it('capture 失败：优先取 error.message，置 error 并返回 null', async () => {
		const { screen } = await fresh();
		invokeMock.mockRejectedValue(new Error('capture boom'));
		await expect(screen.capture()).resolves.toBeNull();
		expect(get(screen).error).toBe('capture boom');

		invokeMock.mockRejectedValue('plain string error');
		await expect(screen.capture()).resolves.toBeNull();
		expect(get(screen).error).toBe('plain string error');
	});

	it('listMonitors 成功/失败分别处理', async () => {
		const { screen } = await fresh();
		const monitors = [
			{ id: 0, name: 'Main', isPrimary: true, bounds: { x: 0, y: 0, width: 1920, height: 1080 } },
			{ id: 1, name: '', isPrimary: false, bounds: { x: 1920, y: 0, width: 1280, height: 720 } },
		];
		invokeMock.mockResolvedValueOnce(monitors);
		await expect(screen.listMonitors()).resolves.toEqual(monitors);
		expect(get(screen).monitorsError).toBe('');

		invokeMock.mockRejectedValue(new Error('monitor boom'));
		await expect(screen.listMonitors()).resolves.toEqual([]);
		expect(get(screen).monitorsError).toBe('monitor boom');
	});
});
