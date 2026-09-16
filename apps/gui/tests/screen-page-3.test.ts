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

/** 屏幕预览页第三组：画布建立的延迟路径与防御分支（主交互见 screen-page.test.ts） */

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

async function readyPage() {
	const img = await waitImg();
	makeImgLoaded(img, 1280, 720);
	stubRect(img, 0, 0, RECT_W, RECT_H);
	await fireEvent.load(img);
	await new Promise(r => setTimeout(r, 0));
	return img;
}

describe('屏幕预览页：画布建立路径', () => {
	it('图片未加载完成时 prepareCanvas 走 onload 延迟绘制', async () => {
		render(Page);
		const img = await waitImg();
		// complete=false：prepareCanvas 不立即绘制，转而挂 img.onload
		Object.defineProperty(img, 'naturalWidth', { value: 1280, configurable: true });
		Object.defineProperty(img, 'naturalHeight', { value: 720, configurable: true });
		Object.defineProperty(img, 'complete', { value: false, configurable: true });
		stubRect(img, 0, 0, RECT_W, RECT_H);

		await fireEvent.load(img);
		// onload 已被 draw 接管
		expect(img.onload).toBeTypeOf('function');
		// load 前画布未建立，交互被 hasImage 拦截
		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		expect(screen.queryByText('128, 72')).not.toBeInTheDocument();

		// 图片加载完成后 draw 建立 offscreen → 拾取生效
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 0));
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByText('128, 72')).toBeInTheDocument();
		});
	});
});

describe('屏幕预览页：指针坐标换算防御', () => {
	it('img 矩形为 0 时 pointerdown/move 均早退，拾取与悬停不变', async () => {
		render(Page);
		const img = await readyPage();
		const surface = screen.getByRole('img', { name: SURFACE_LABEL });

		// 先正常拾取一点
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByText('128, 72')).toBeInTheDocument();
		});

		// 矩形归零：eventToImage 返回 null，pointerdown/move 双路径早退
		stubRect(img, 0, 0, 0, 0);
		await fireEvent.pointerMove(surface, { clientX: 64, clientY: 36, pointerId: 1 });
		await fireEvent.pointerDown(surface, { clientX: 64, clientY: 36, pointerId: 2 });
		await fireEvent.pointerUp(surface, { pointerId: 2 });
		await new Promise(r => setTimeout(r, 50));

		// 已拾取坐标未被覆盖，悬停读数也不产生
		expect(screen.getByText('128, 72')).toBeInTheDocument();
		expect(screen.queryByText('640,360')).not.toBeInTheDocument();
	});
});

describe('屏幕预览页：上下文获取失败防御', () => {
	it('getContext 返回 null 时放大镜与匹配高亮早退不抛错', async () => {
		render(Page);
		await readyPage();
		const surface = screen.getByRole('img', { name: SURFACE_LABEL });

		// 正常路径先建立匹配状态（stub 上下文全 0 像素 → 0 命中）
		await fireEvent.click(screen.getByRole('checkbox', { name: '在画面中高亮匹配像素' }));
		await waitFor(() => {
			expect(screen.getByText('无匹配像素')).toBeInTheDocument();
		});

		// 之后所有 getContext 均失败（readPixel/drawMagnifier/匹配 effect）
		const proto = (window as any).HTMLCanvasElement.prototype;
		const saved = proto.getContext;
		proto.getContext = vi.fn(() => null);
		try {
			// 悬停：readPixel 与 drawMagnifier 均拿到 null 上下文，坐标读数仍应更新
			await fireEvent.pointerMove(surface, { clientX: 64, clientY: 36, pointerId: 1 });
			await waitFor(() => {
				expect(screen.getByText('640,360')).toBeInTheDocument();
			});

			// 匹配 effect 重跑（容差变化）→ ctx null 早退，matchCount 保持 0
			const range = document.querySelector('.color-picker input[type="range"]') as HTMLInputElement;
			await fireEvent.input(range, { target: { value: '30' } });
			await new Promise(r => setTimeout(r, 50));
			expect(screen.getByText('无匹配像素')).toBeInTheDocument();
		} finally {
			proto.getContext = saved;
		}
	});
});
