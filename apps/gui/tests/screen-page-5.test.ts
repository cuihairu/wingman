import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/screen/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { screen as screenStore } from '$lib/stores/screen';
import {
	installCanvasStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	makeImgLoaded,
	stubRect,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

/** 屏幕预览页第五组：零尺寸截图的 overlay 隐藏、readPixel 二次取上下文失败、坐标清空与以坐标为中心。 */

const RECT_W = 128;
const RECT_H = 72;
const SURFACE_LABEL = '屏幕截图预览：拖拽框选区域，单击拾取坐标与颜色';

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	installCanvasStub();
	logs.clear();
	screenStore.clear();
	await connection.disconnect();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	screenStore.clear();
	vi.restoreAllMocks();
});

async function waitImg(): Promise<HTMLImageElement> {
	await waitFor(() => {
		expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
	});
	return screen.getByRole('img', { name: '屏幕截图' }) as HTMLImageElement;
}

describe('屏幕预览页：零尺寸截图（invoke 模式）', () => {
	it('截图宽高为 0 时选框 overlay 隐藏', async () => {
		vi.resetModules();
		localStorage.removeItem('wingman-settings');
		localStorage.removeItem('wingman.theme');
		(window as any).__TAURI_INVOKE__ = vi.fn((cmd: string) => {
			if (cmd === 'capture_screenshot') {
				return {
					image: 'data:image/png;base64,shot',
					width: 0,
					height: 0,
					region: { x: 0, y: 0, width: 0, height: 0 },
					timestamp: Date.now(),
				};
			}
			if (cmd === 'list_monitors') return [];
			return {};
		});
		const pageMod = await import('../src/routes/screen/+page.svelte');
		const { render, screen, waitFor } = await import('@testing-library/svelte');
		render(pageMod.default as any);
		await waitFor(() => {
			const overlay = document.querySelector('.region-overlay') as HTMLElement;
			expect(overlay).toBeTruthy();
			expect(overlay.getAttribute('style')).toContain('display: none');
		});
		// fresh 模块的 cleanup 未接入全局 afterEach，手动清理避免污染后续测试
		document.body.innerHTML = '';
	});
});

describe('屏幕预览页：readPixel 上下文获取失败', () => {
	it('离屏画布建立后再次取上下文返回 null 时取色为 null', async () => {
		// 第一次 getContext（建画布）成功，之后（readPixel）返回 null
		const proto = (window as any).HTMLCanvasElement.prototype;
		const real = proto.getContext;
		let calls = 0;
		proto.getContext = vi.fn((...args: unknown[]) => {
			calls += 1;
			return calls <= 1 ? real.apply(proto, args) : null;
		});

		render(Page);
		const img = await waitImg();
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 50));

		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerMove(surface, { clientX: 64, clientY: 36, pointerId: 1 });
		await new Promise(r => setTimeout(r, 50));
		// readPixel 返回 null：悬停读数不出现色块
		expect(document.querySelector('.hover-readout .swatch')).toBeNull();
	});

	it('点击拾取时颜色为 null：坐标卡片显示但拾色结果区隐藏', async () => {
		// 建画布成功，之后 readPixel 的 getContext 返回 null → pointer.color = null
		const proto = (window as any).HTMLCanvasElement.prototype;
		const real = proto.getContext;
		let calls = 0;
		proto.getContext = vi.fn((...args: unknown[]) => {
			calls += 1;
			return calls <= 1 ? real.apply(proto, args) : null;
		});

		render(Page);
		const img = await waitImg();
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 50));

		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByText('128, 72')).toBeInTheDocument();
		});
		// pointer 存在但 color 为 null：不渲染拾色结果（.picked-color 隐藏）
		expect(document.querySelector('.coordinate-card')).toBeTruthy();
		expect(document.querySelector('.picked-color')).toBeNull();
	});
});

describe('屏幕预览页：坐标输入与以坐标为中心', () => {
	it('区域宽高清零后以坐标为中心回退默认宽高', async () => {
		render(Page);
		const img = await waitImg();
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 50));

		// 单击拾取坐标（pointer 存在后「以坐标为中心」才可用）
		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByText('128, 72')).toBeInTheDocument();
		});

		// W/H 清零 → region.width||240 / height||160 回退默认尺寸
		const wInput = screen.getByLabelText('W') as HTMLInputElement;
		await fireEvent.input(wInput, { target: { value: '' } });
		const hInput = screen.getByLabelText('H') as HTMLInputElement;
		await fireEvent.input(hInput, { target: { value: '' } });
		await fireEvent.click(screen.getByRole('button', { name: '以坐标为中心' }));
		await new Promise(r => setTimeout(r, 50));

		expect((screen.getByLabelText('W') as HTMLInputElement).value).toBe('240');
		expect((screen.getByLabelText('H') as HTMLInputElement).value).toBe('160');
	});
});

