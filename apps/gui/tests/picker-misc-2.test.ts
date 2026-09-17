import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/svelte';
import ScreenPickerModal from '$lib/components/ScreenPickerModal.svelte';
import {
	installCanvasStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	readyPickerModal,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

/**
 * ScreenPickerModal 补充路径：region 模式未按下的纯悬停移动、
 * color 模式就绪后画布 ctx 失效（readPixel/drawMagnifier 早退、读数无色值）、
 * 无关按键（非 Esc/Enter）不触发确认与关闭。
 */

const CONTAINER_LABEL_REGION = '屏幕截图，拖拽选择区域';
const CONTAINER_LABEL_COLOR = '屏幕截图，点击拾取颜色';

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	installCanvasStub();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	vi.restoreAllMocks();
});

describe('ScreenPickerModal 补充路径', () => {
	it('region 模式未按下时悬停移动不进入拖拽', async () => {
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'region', onconfirm, onclose } });
		await readyPickerModal();
		const container = screen.getByRole('img', { name: CONTAINER_LABEL_REGION });
		// 无 pointerdown 直接 move：else if (dragStart) 的空侧
		await fireEvent.pointerMove(container, { clientX: 30, clientY: 20, pointerId: 1 });
		await new Promise(r => setTimeout(r, 20));
		expect(document.querySelector('.sel-rect.dragging')).not.toBeInTheDocument();
		expect(screen.getByRole('button', { name: '确定' })).toBeDisabled();
	});

	it('color 模式就绪后 ctx 全部失效：readPixel/drawMagnifier 早退且读数无色值', async () => {
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'color', onconfirm, onclose } });
		await readyPickerModal();
		// ready 后所有 getContext 返回 null：readPixel 与 drawMagnifier 均走 ctx 守卫早退
		(window as any).HTMLCanvasElement.prototype.getContext = vi.fn(() => null);
		const container = screen.getByRole('img', { name: CONTAINER_LABEL_COLOR });
		// 第一次 move 建立 hoverPos（渲染放大镜与读数 DOM）
		await fireEvent.pointerMove(container, { clientX: 30, clientY: 20, pointerId: 1 });
		await new Promise(r => setTimeout(r, 20));
		// 第二次 move：magnifierEl 已绑定，drawMagnifier 进入 ctx null 早退
		await fireEvent.pointerMove(container, { clientX: 32, clientY: 22, pointerId: 1 });
		await new Promise(r => setTimeout(r, 20));
		// hoverColor 为 null：读数区只有坐标，没有色块与色值
		expect(document.querySelector('.readout-swatch')).not.toBeInTheDocument();
		expect(document.querySelector('.readout-hex')).not.toBeInTheDocument();
		expect(screen.getByRole('dialog')).toBeInTheDocument();
	});

	it('无关按键（非 Esc/Enter）不触发确认与关闭', async () => {
		const onconfirm = vi.fn();
		const onclose = vi.fn();
		render(ScreenPickerModal, { props: { mode: 'region', onconfirm, onclose } });
		await readyPickerModal();
		await fireEvent.keyDown(window, { key: 'Tab' });
		await new Promise(r => setTimeout(r, 20));
		expect(onconfirm).not.toHaveBeenCalled();
		expect(onclose).not.toHaveBeenCalled();
		expect(screen.getByRole('dialog')).toBeInTheDocument();
	});
});
