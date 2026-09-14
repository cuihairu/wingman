import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import ScreenPickerModal from '../src/lib/components/ScreenPickerModal.svelte';
import { screen as screenStore } from '$lib/stores/screen';
import { installCanvasStub, installPointerCaptureStub, installTolerantStyleAttrStub, readyPickerModal, uninstallTolerantStyleAttrStub } from './helpers/test-utils';

/// dev 截图为 1280x720、region 原点 (0,0)。容器按 128x72 显示，即 1px 容器 = 10px 图像坐标
const RECT_W = 128;
const RECT_H = 72;

async function readyModal() {
	return readyPickerModal(RECT_W, RECT_H);
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	screenStore.clear();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	screenStore.clear();
});

describe('ScreenPickerModal（color 模式）', () => {
	/// 所有画布的 getImageData 统一返回红色像素
	function redPixel(x: number, y: number, w: number, h: number) {
		const data = new Uint8ClampedArray(w * h * 4);
		for (let i = 0; i < data.length; i += 4) {
			data[i] = 255;
		}
		return { data, width: w, height: h };
	}

	it('加载中显示捕获提示，完成后显示标题与提示，确定按钮初始禁用', async () => {
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'color', onconfirm, onclose } });

		expect(screen.getByText('正在捕获屏幕...')).toBeInTheDocument();
		await waitFor(() => {
			expect(screen.getByText('拾取屏幕颜色')).toBeInTheDocument();
		});
		expect(screen.getByText('移动鼠标查看放大像素，点击拾取颜色')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
	});

	it('hover 显示十字线与坐标读数，放大镜绘制执行；点击取色后可确认', async () => {
		const contexts = installCanvasStub(redPixel);
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'color', onconfirm, onclose } });
		const img = await readyModal();

		const surface = screen.getByRole('img', { name: '屏幕截图，点击拾取颜色' });
		// 第一次 move 渲染放大镜 canvas，第二次 move 才能执行绘制
		await fireEvent.pointerMove(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByText('#ff0000')).toBeInTheDocument();
		});
		await fireEvent.pointerMove(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		// 放大镜 drawImage(offscreen, sx, sy, 9, 9, 0, 0, 135, 135) 为 9 参调用
		await waitFor(() => {
			expect(contexts.some(c => c.drawImage.mock.calls.some(call => call.length === 9))).toBe(true);
		});
		expect(surface.parentElement!.querySelector('.crosshair-v')).toBeInTheDocument();
		expect(surface.parentElement!.querySelector('.crosshair-h')).toBeInTheDocument();
		expect(surface.parentElement!.querySelector('.magnifier')).toBeInTheDocument();
		expect(screen.getByText('128,72')).toBeInTheDocument(); // 屏幕绝对坐标读数

		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '确定' })).toBeEnabled();
		});
		expect(surface.parentElement!.querySelector('.picked-mark')).toBeInTheDocument();

		await fireEvent.click(screen.getByRole('button', { name: '确定' }));
		expect(onconfirm).toHaveBeenCalledWith({ color: '#ff0000', position: { x: 128, y: 72 } });
		expect(onclose).not.toHaveBeenCalled();
	});

	it('Enter 确认（未拾取时不确认）、Esc 取消', async () => {
		installCanvasStub();
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'color', onconfirm, onclose } });
		await readyModal();

		// 未拾取颜色时 Enter 不触发 onconfirm
		await fireEvent.keyDown(window, { key: 'Enter' });
		expect(onconfirm).not.toHaveBeenCalled();

		const surface = screen.getByRole('img', { name: '屏幕截图，点击拾取颜色' });
		await fireEvent.pointerDown(surface, { clientX: 0, clientY: 0, pointerId: 1 });
		await fireEvent.keyDown(window, { key: 'Enter' });
		expect(onconfirm).toHaveBeenCalledTimes(1);
		// 默认黑像素
		expect(onconfirm).toHaveBeenCalledWith({ color: '#000000', position: { x: 0, y: 0 } });

		await fireEvent.keyDown(window, { key: 'Escape' });
		expect(onclose).toHaveBeenCalledTimes(1);
	});

	it('取消按钮与右上角 ✕ 均回调 onclose', async () => {
		installCanvasStub();
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'color', onconfirm, onclose } });
		await readyModal();
		await fireEvent.click(screen.getByRole('button', { name: '取消' }));
		expect(onclose).toHaveBeenCalledTimes(1);
		await fireEvent.click(screen.getByRole('button', { name: '✕' }));
		expect(onclose).toHaveBeenCalledTimes(2);
	});

	it('截图失败显示错误与重新截图按钮，重试成功后恢复截图', async () => {
		installCanvasStub();
		const originalCapture = screenStore.capture.bind(screenStore);
		let fail = true;
		vi.spyOn(screenStore, 'capture').mockImplementation(async (...args: any[]) => {
			if (fail) return null;
			return originalCapture(...(args as []));
		});
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'region', onconfirm, onclose } });

		await waitFor(() => {
			expect(screen.getByText('截图失败，请确认 runtime 已连接')).toBeInTheDocument();
		});
		// 错误分支与页脚各有一个重试按钮
		expect(screen.getAllByRole('button', { name: '重新截图' })).toHaveLength(2);

		fail = false;
		await fireEvent.click(screen.getAllByRole('button', { name: '重新截图' })[0]);
		await waitFor(() => {
			expect(screen.getByRole('img', { name: '屏幕截图' })).toBeInTheDocument();
		});
		expect(screen.queryByText('截图失败，请确认 runtime 已连接')).not.toBeInTheDocument();
	});
});

describe('ScreenPickerModal（region 模式）', () => {
	async function ready(onconfirm: (r: any) => void, onclose: () => void) {
		installCanvasStub();
		render(ScreenPickerModal, { props: { mode: 'region', onconfirm, onclose } });
		return readyModal();
	}

	it('默认提示文案，确定按钮初始禁用', async () => {
		await ready(vi.fn(), vi.fn());
		expect(screen.getByText('拾取屏幕区域')).toBeInTheDocument();
		expect(screen.getByText('在截图上按住鼠标拖拽选择区域，松开后可调整或重新拖拽')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
	});

	it('拖拽生成选区并显示尺寸标签，确认回调屏幕绝对坐标', async () => {
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		await ready(onconfirm, onclose);

		const surface = screen.getByRole('img', { name: '屏幕截图，拖拽选择区域' });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerMove(surface, { clientX: 38.4, clientY: 36, pointerId: 1 });

		// 拖拽中：临时选框显示图内坐标差 256×288
		expect(screen.getByText('256×288')).toBeInTheDocument();
		expect(surface.parentElement!.querySelector('.sel-rect.dragging')).toBeInTheDocument();

		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '确定' })).toBeEnabled();
		});
		expect(screen.getByText('128,72 · 256×288')).toBeInTheDocument();
		expect(surface.parentElement!.querySelector('.sel-rect.dragging')).toBeNull();

		await fireEvent.click(screen.getByRole('button', { name: '确定' }));
		expect(onconfirm).toHaveBeenCalledWith({ region: { x: 128, y: 72, width: 256, height: 288 } });
	});

	it('拖拽过小（<2px）不生成选区，确定保持禁用', async () => {
		const onconfirm = vi.fn();
		await ready(onconfirm, vi.fn());

		const surface = screen.getByRole('img', { name: '屏幕截图，拖拽选择区域' });
		// 容器 1px = 图像 10px：1px 容器位移仅产生 1px 图像位移，低于 2px 阈值
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerMove(surface, { clientX: 12.9, clientY: 7.3, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });

		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
		expect(onconfirm).not.toHaveBeenCalled();
		// Enter 在无选区时也不确认
		await fireEvent.keyDown(window, { key: 'Enter' });
		expect(onconfirm).not.toHaveBeenCalled();
	});

	it('Esc 关闭且不确认', async () => {
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		await ready(onconfirm, onclose);
		await fireEvent.keyDown(window, { key: 'Escape' });
		expect(onclose).toHaveBeenCalledTimes(1);
		expect(onconfirm).not.toHaveBeenCalled();
	});
});
