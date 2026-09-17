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

/** 屏幕预览页第四组：画布建立的空图/空上下文路径、矩形零防御、零宽区域 overlay 隐藏。 */

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

describe('屏幕预览页：画布建立空值路径', () => {
	it('图片 naturalWidth 为 0 时不建立画布', async () => {
		render(Page);
		const img = await waitImg();
		// complete=true 但 naturalWidth=0：if 判定失败转 onload，draw 里再次早退
		makeImgLoaded(img, 0, 0);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		await fireEvent.load(img);
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 50));

		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		// hasImage=false：无坐标记录
		expect(screen.queryByText('128, 72')).not.toBeInTheDocument();
	});

	it('离屏画布上下文获取失败时不建立画布', async () => {
		// 重建 stub 后立刻改为返回 null：draw 内 getContext 返回 null → 早退
		const proto = (window as any).HTMLCanvasElement.prototype;
		proto.getContext = vi.fn(() => null);
		render(Page);
		const img = await waitImg();
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 50));
		expect(screen.queryByText('128, 72')).not.toBeInTheDocument();
	});
});

describe('屏幕预览页：未就绪时的指针移动', () => {
	it('画布未建立时 pointermove 被 hasImage 守卫拦截', async () => {
		render(Page);
		const img = await waitImg();
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		// 不触发 load → offscreen 未建立
		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerMove(surface, { clientX: 64, clientY: 36, pointerId: 1 });
		await new Promise(r => setTimeout(r, 50));
		// 悬停读数不出现
		expect(screen.queryByText('640,360')).not.toBeInTheDocument();
	});
});

describe('屏幕预览页：零宽区域', () => {
	it('区域宽高清零后选框隐藏且复制区域按钮禁用', async () => {
		render(Page);
		const img = await waitImg();
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		await fireEvent.load(img);
		await new Promise(r => setTimeout(r, 0));

		// 默认 region 为全图（overlay 可见）；W 清零后 overlay 隐藏
		const overlay = document.querySelector('.region-overlay') as HTMLElement;
		expect(overlay.getAttribute('style')).not.toContain('display: none');

		const wInput = screen.getByLabelText('W') as HTMLInputElement;
		await fireEvent.input(wInput, { target: { value: '' } });
		await new Promise(r => setTimeout(r, 0));
		expect(overlay.getAttribute('style')).toContain('display: none');
		// 复制区域按钮禁用；以坐标为中心（无 pointer）也禁用
		expect(screen.getByRole('button', { name: '复制区域' })).toBeDisabled();
		expect(screen.getByRole('button', { name: '以坐标为中心' })).toBeDisabled();
	});
});
