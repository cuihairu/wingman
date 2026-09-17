import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import RegionPicker from '$lib/components/RegionPicker.svelte';
import { logs } from '$lib/stores/logs';
import {
	installCanvasStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	makeImgLoaded,
	readyPickerModal,
	stubRect,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

/**
 * ScreenPickerModal 的 region 模式全路径（经 RegionPicker 拾取入口）：
 * ready 前拦截、零矩形拖拽、过小选区、有效拖拽回填、空选区确认守卫、像素画布创建失败。
 * color 模式主链路见 triggers-page-3 的拾取弹窗测试。
 */

const PICK_BTN_TITLE = '从屏幕截图上拖拽选择区域';
const CONTAINER_LABEL = '屏幕截图，拖拽选择区域';

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	installCanvasStub();
	logs.clear();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	vi.restoreAllMocks();
});

async function openPicker() {
	render(RegionPicker, { props: { value: { x: 0, y: 0, width: 0, height: 0 } } });
	await fireEvent.click(screen.getByTitle(PICK_BTN_TITLE));
	await screen.findByRole('dialog');
}

describe('ScreenPickerModal region 模式', () => {
	it('画布就绪前 pointerdown/move 被拦截，不进入拖拽', async () => {
		await openPicker();
		const img = await waitFor(() => screen.getByRole('img', { name: '屏幕截图' })) as HTMLImageElement;
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, 128, 72);
		// 尚未触发 load → ready=false
		const container = screen.getByRole('img', { name: CONTAINER_LABEL });
		await fireEvent.pointerDown(container, { clientX: 20, clientY: 10, pointerId: 1 });
		await fireEvent.pointerMove(container, { clientX: 40, clientY: 20, pointerId: 1 });
		await fireEvent.pointerUp(container, { pointerId: 1 });
		await new Promise(r => setTimeout(r, 50));
		// 无拖拽选框、确认保持禁用
		expect(document.querySelector('.sel-rect.dragging')).not.toBeInTheDocument();
		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
	});

	it('图片矩形为零时拖拽坐标换算返回 null，不产生选区', async () => {
		await openPicker();
		await readyPickerModal(0, 0);
		const container = screen.getByRole('img', { name: CONTAINER_LABEL });
		await fireEvent.pointerDown(container, { clientX: 20, clientY: 10, pointerId: 1 });
		await fireEvent.pointerMove(container, { clientX: 40, clientY: 20, pointerId: 1 });
		await fireEvent.pointerUp(container, { pointerId: 1 });
		await new Promise(r => setTimeout(r, 50));
		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
	});

	it('拖拽距离过小（<2px）判定无效选区', async () => {
		await openPicker();
		await readyPickerModal();
		const container = screen.getByRole('img', { name: CONTAINER_LABEL });
		// 同点按下松开 → 宽高 0 → selectedRegion=null
		await fireEvent.pointerDown(container, { clientX: 20, clientY: 10, pointerId: 1 });
		await fireEvent.pointerUp(container, { pointerId: 1 });
		await new Promise(r => setTimeout(r, 50));
		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
	});

	it('空选区时点击确定/Enter 均被守卫拦截', async () => {
		await openPicker();
		await readyPickerModal();
		// 无选区：确定按钮禁用但 handler 仍执行（confirm 守卫）
		await fireEvent.click(screen.getByRole('button', { name: '确定' }));
		await fireEvent.keyDown(window, { key: 'Enter' });
		await new Promise(r => setTimeout(r, 50));
		expect(screen.getByRole('dialog')).toBeInTheDocument();
	});

	it('有效拖拽生成选区并回填 RegionPicker', async () => {
		await openPicker();
		await readyPickerModal();
		const container = screen.getByRole('img', { name: CONTAINER_LABEL });

		await fireEvent.pointerDown(container, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.pointerMove(container, { clientX: 64, clientY: 43.2, pointerId: 1 });
		await fireEvent.pointerUp(container, { pointerId: 1 });

		// 选区固定框渲染（松开后）+ 确定可用
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '确定' })).toBeEnabled();
		});
		expect(document.querySelector('.sel-rect:not(.dragging)')).toBeInTheDocument();

		await fireEvent.click(screen.getByRole('button', { name: '确定' }));
		await waitFor(() => {
			expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
		});
		// 回填：图像坐标 128,72 → 640,432（1 容器 px = 10 图像 px）
		expect((screen.getByLabelText('X') as HTMLInputElement).value).toBe('128');
		expect((screen.getByLabelText('Y') as HTMLInputElement).value).toBe('72');
		expect((screen.getByLabelText('W') as HTMLInputElement).value).toBe('512');
		expect((screen.getByLabelText('H') as HTMLInputElement).value).toBe('360');
	});

	it('拖拽后可重新拖拽覆盖旧选区', async () => {
		await openPicker();
		await readyPickerModal();
		const container = screen.getByRole('img', { name: CONTAINER_LABEL });

		// 第一次选区
		await fireEvent.pointerDown(container, { clientX: 10, clientY: 10, pointerId: 1 });
		await fireEvent.pointerMove(container, { clientX: 30, clientY: 30, pointerId: 1 });
		await fireEvent.pointerUp(container, { pointerId: 1 });
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '确定' })).toBeEnabled();
		});

		// 重新拖拽：pointerdown 清空 selectedRegion
		await fireEvent.pointerDown(container, { clientX: 12.8, clientY: 7.2, pointerId: 2 });
		await fireEvent.pointerMove(container, { clientX: 64, clientY: 43.2, pointerId: 2 });
		expect(document.querySelector('.sel-rect.dragging')).toBeInTheDocument();
		await fireEvent.pointerUp(container, { pointerId: 2 });
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '确定' })).toBeEnabled();
		});
		expect((screen.getByLabelText('X') as HTMLInputElement).value).toBe('0');
	});

	it('像素画布创建失败时显示错误与重新截图按钮', async () => {
		await openPicker();
		// 所有 getContext 返回 null → prepareCanvas 报「无法创建像素画布」
		const proto = (window as any).HTMLCanvasElement.prototype;
		proto.getContext = vi.fn(() => null);
		const img = await waitFor(() => screen.getByRole('img', { name: '屏幕截图' })) as HTMLImageElement;
		makeImgLoaded(img, 1280, 720);
		stubRect(img, 0, 0, 128, 72);
		await fireEvent.load(img);
		await waitFor(() => {
			expect(screen.getByText('无法创建像素画布')).toBeInTheDocument();
		});
		// 原型污染交由下一个 beforeEach 的 installCanvasStub 覆盖
	});
});
