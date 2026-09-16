import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/screen/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { screen as screenStore } from '$lib/stores/screen';
import {
	installCanvasStub,
	installClipboardStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	makeImgLoaded,
	stubRect,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

/** 屏幕预览页第二组：悬停/键盘/复制失败/防御分支（主交互见 screen-page.test.ts） */

const RECT_W = 128;
const RECT_H = 72;

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

const SURFACE_LABEL = '屏幕截图预览：拖拽框选区域，单击拾取坐标与颜色';

async function readyPage() {
	await waitFor(() => {
		expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
	});
	const img = screen.getByRole('img', { name: '屏幕截图' }) as HTMLImageElement;
	makeImgLoaded(img, 1280, 720);
	stubRect(img, 0, 0, RECT_W, RECT_H);
	await fireEvent.load(img);
	await new Promise(r => setTimeout(r, 0));
	return img;
}

describe('屏幕预览页：悬停与空态分支', () => {
	it('悬停读数随 pointermove 更新，pointerleave 后清除', async () => {
		render(Page);
		await readyPage();

		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerMove(surface, { clientX: 64, clientY: 36, pointerId: 1 });
		// 悬停读数（屏幕绝对坐标 640,360）
		await waitFor(() => {
			expect(screen.getByText('640,360')).toBeInTheDocument();
		});

		await fireEvent.pointerLeave(surface);
		await waitFor(() => {
			expect(screen.queryByText('640,360')).not.toBeInTheDocument();
		});
	});

	it('画布未就绪时点击被 hasImage 守卫拦截，不记录坐标', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
		const img = screen.getByRole('img', { name: '屏幕截图' }) as HTMLImageElement;
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, RECT_W, RECT_H);
		// 不触发 load 事件 → offscreen 画布未建立 → hasImage=false，pointerdown 早退
		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await new Promise(r => setTimeout(r, 50));

		expect(screen.queryByText('128, 72')).not.toBeInTheDocument();
	});

	it('空态「立即捕获」按钮触发截图', async () => {
		let release!: (v: any) => void;
		const gate = new Promise(resolve => { release = resolve; });
		const originalCapture = screenStore.capture.bind(screenStore);
		vi.spyOn(screenStore, 'capture').mockImplementation(async (...args: any[]) => {
			await gate;
			return originalCapture(...(args as []));
		});

		render(Page);
		expect(screen.getByText('尚无截图')).toBeInTheDocument();
		await fireEvent.click(screen.getByRole('button', { name: '立即捕获' }));
		expect(screenStore.capture).toHaveBeenCalled();

		release(undefined);
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
	});
});

describe('屏幕预览页：防御分支与复制失败', () => {
	it('无截图时全图与以坐标为中心按钮可点但不改变区域', async () => {
		let release!: (v: any) => void;
		const gate = new Promise(resolve => { release = resolve; });
		const originalCapture = screenStore.capture.bind(screenStore);
		vi.spyOn(screenStore, 'capture').mockImplementation(async (...args: any[]) => {
			await gate;
			return originalCapture(...(args as []));
		});
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: '全图' }));
		await fireEvent.click(screen.getByRole('button', { name: '以坐标为中心' }));
		expect(screen.getByLabelText('X')).toHaveValue(0);

		release(undefined);
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
	});

	it('剪贴板不可用时复制坐标记录警告日志', async () => {
		const clip = installClipboardStub();
		clip.rejectWrites();
		render(Page);
		await readyPage();

		const surface = screen.getByRole('img', { name: SURFACE_LABEL });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByText('128, 72')).toBeInTheDocument();
		});

		await fireEvent.click(screen.getAllByRole('button', { name: '复制' })[0]);
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'warning' && e.message === '复制失败（剪贴板不可用）')).toBe(true);
		});
	});

	it('匹配开关打开但颜色非法时计 0 并显示无匹配', async () => {
		render(Page);
		await readyPage();

		await fireEvent.click(screen.getByRole('checkbox', { name: '在画面中高亮匹配像素' }));
		// 非法十六进制（parseHexColor 返回 null）→ 无匹配
		const colorInput = screen.getByPlaceholderText('#ff0000') as HTMLInputElement;
		await fireEvent.input(colorInput, { target: { value: 'not-a-color' } });
		await waitFor(() => {
			expect(screen.getByText('无匹配像素')).toBeInTheDocument();
		});
	});
});
