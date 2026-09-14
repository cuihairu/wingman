import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/svelte';
import ColorPicker from '../src/lib/components/ColorPicker.svelte';
import RegionPicker from '../src/lib/components/RegionPicker.svelte';
import {
	installCanvasStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	readyPickerModal,
	uninstallTolerantStyleAttrStub,
	type FakeCanvasContext,
} from './helpers/test-utils';

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
});

describe('ColorPicker', () => {
	it('渲染默认值与容差，合法值无 invalid 样式', () => {
		render(ColorPicker);
		const input = screen.getByPlaceholderText('#ff0000') as HTMLInputElement;
		expect(input.value).toBe('#ff0000');
		expect(input).not.toHaveClass('invalid');
		expect(screen.getByText('10')).toBeInTheDocument(); // 默认容差
	});

	it('非法颜色加 invalid 样式且预览回退黑色，改回合法后恢复', async () => {
		const { container } = render(ColorPicker);
		const input = screen.getByPlaceholderText('#ff0000');
		const preview = container.querySelector('.color-preview') as HTMLDivElement;

		await fireEvent.input(input, { target: { value: 'zzz' } });
		expect(input).toHaveClass('invalid');
		// jsdom 会把 style 归一化为 rgb() 序列化
		expect(preview.style.backgroundColor).toBe('rgb(0, 0, 0)');

		await fireEvent.input(input, { target: { value: '#00ff00' } });
		expect(input).not.toHaveClass('invalid');
		expect(preview.style.backgroundColor).toBe('rgb(0, 255, 0)');
	});

	it('容差滑杆更新数值，非数字回退 0', async () => {
		render(ColorPicker);
		const range = screen.getByRole('slider');
		await fireEvent.input(range, { target: { value: '35' } });
		expect(screen.getByText('35')).toBeInTheDocument();

		// range input 会把非法 value 消毒为 50，需手动构造 target 才能触发 parseInt NaN → 0 分支
		const event = new window.Event('input', { bubbles: true });
		Object.defineProperty(event, 'target', { value: { value: 'abc' }, configurable: true });
		await fireEvent(range, event);
		expect(screen.getByText('0')).toBeInTheDocument();
	});

	it('取色按钮打开截屏弹窗，Esc 关闭', async () => {
		installCanvasStub();
		render(ColorPicker);
		await fireEvent.click(screen.getByRole('button', { name: '取色' }));
		expect(await screen.findByRole('dialog')).toBeInTheDocument();

		await fireEvent.keyDown(window, { key: 'Escape' });
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});

	it('完整取色链路：点击截图拾取颜色后写回输入框并关闭弹窗', async () => {
		// getImageData 返回 #10ff00
		const contexts: FakeCanvasContext[] = installCanvasStub((_x, _y, w, h) => {
			const data = new Uint8ClampedArray(w * h * 4);
			data[0] = 16;
			data[1] = 255;
			return { data, width: w, height: h };
		});
		expect(contexts).toHaveLength(0); // 惰性创建
		render(ColorPicker);
		await fireEvent.click(screen.getByRole('button', { name: '取色' }));
		await readyPickerModal();

		const surface = screen.getByRole('img', { name: '屏幕截图，点击拾取颜色' });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.click(await screen.findByRole('button', { name: '确定' }));

		const input = screen.getByPlaceholderText('#ff0000') as HTMLInputElement;
		expect(input.value).toBe('#10ff00');
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});
});

describe('RegionPicker', () => {
	it('渲染四个坐标输入框', () => {
		render(RegionPicker);
		expect(screen.getByLabelText('X')).toHaveValue(0);
		expect(screen.getByLabelText('Y')).toHaveValue(0);
		expect(screen.getByLabelText('W')).toHaveValue(0);
		expect(screen.getByLabelText('H')).toHaveValue(0);
	});

	it('输入更新坐标，非数字回退 0', async () => {
		render(RegionPicker);
		const x = screen.getByLabelText('X');
		const h = screen.getByLabelText('H');

		await fireEvent.input(x, { target: { value: '120' } });
		expect(screen.getByLabelText('X')).toHaveValue(120);

		await fireEvent.input(h, { target: { value: '48' } });
		expect(screen.getByLabelText('H')).toHaveValue(48);

		await fireEvent.input(x, { target: { value: 'abc' } });
		expect(screen.getByLabelText('X')).toHaveValue(0);
	});

	it('拾取按钮打开截屏弹窗，Esc 关闭', async () => {
		installCanvasStub();
		render(RegionPicker);
		await fireEvent.click(screen.getByRole('button', { name: '拾取' }));
		expect(await screen.findByRole('dialog')).toBeInTheDocument();

		await fireEvent.keyDown(window, { key: 'Escape' });
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});

	it('完整拾取链路：拖拽确认后写回四个字段', async () => {
		installCanvasStub();
		render(RegionPicker);
		await fireEvent.click(screen.getByRole('button', { name: '拾取' }));
		await readyPickerModal();

		const surface = screen.getByRole('img', { name: '屏幕截图，拖拽选择区域' });
		// 容器 128x72 对应 1280x720：拖出 100x50 图像像素的区域
		await fireEvent.pointerDown(surface, { clientX: 10, clientY: 5, pointerId: 1 });
		await fireEvent.pointerMove(surface, { clientX: 20, clientY: 10, pointerId: 1 });
		await fireEvent.pointerUp(surface, { pointerId: 1 });
		await fireEvent.click(await screen.findByRole('button', { name: '确定' }));

		expect(screen.getByLabelText('X')).toHaveValue(100);
		expect(screen.getByLabelText('Y')).toHaveValue(50);
		expect(screen.getByLabelText('W')).toHaveValue(100);
		expect(screen.getByLabelText('H')).toHaveValue(50);
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});
});
