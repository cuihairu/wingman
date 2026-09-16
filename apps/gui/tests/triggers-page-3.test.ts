import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/triggers/+page.svelte';
import { triggers, type TriggerConfig } from '$lib/stores/triggers';
import { logs } from '$lib/stores/logs';
import {
	installCanvasStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	readyPickerModal,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

/**
 * 触发器页第三组：delay 字段写路径、ColorPicker 双向绑定写路径、拾取弹窗关闭路径。
 * 列表/条件分支见 triggers-page.test.ts，动作类型切换见 triggers-page-2.test.ts。
 */

function makeTrigger(overrides: Partial<TriggerConfig> = {}): TriggerConfig {
	return {
		id: 't1',
		name: '回血触发器',
		enabled: true,
		condition: { type: 'color_found', value: '#ff0000', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 },
		actions: [],
		...overrides,
	} as TriggerConfig;
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	logs.clear();
	triggers.set([]);
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	vi.restoreAllMocks();
});

async function openEditor(trigger: TriggerConfig) {
	triggers.set([trigger]);
	render(Page);
	await fireEvent.click(screen.getByText(trigger.name));
	await waitFor(() => {
		expect(screen.getByText(`编辑触发器: ${trigger.name}`)).toBeInTheDocument();
	});
}

describe('触发器页：字段写路径补全', () => {
	it('delay 动作的延时输入经 parseInt0 保存', async () => {
		await openEditor(makeTrigger());
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		const card = document.querySelector('.action-card')!;
		await fireEvent.change(card.querySelector('select')!, { target: { value: 'delay' } });
		// 切换后默认 500，改成 750 再保存
		const delayInput = card.querySelector('input[type="number"]') as HTMLInputElement;
		expect(delayInput).toHaveValue(500);
		await fireEvent.input(delayInput, { target: { value: '750' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			expect(get(triggers)[0].actions[0]).toMatchObject({ type: 'delay', delay: 750 });
		});
	});

	it('ColorPicker 的颜色与容差写路径保存到条件', async () => {
		await openEditor(makeTrigger({
			condition: { type: 'color_found', value: '#00ff00', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 20, interval: 1000 },
		}));

		// ColorPicker 文本输入（placeholder #ff0000）
		const colorInput = screen.getByPlaceholderText('#ff0000') as HTMLInputElement;
		expect(colorInput.value).toBe('#00ff00');
		await fireEvent.input(colorInput, { target: { value: '#123456' } });

		// 容差滑杆（ColorPicker 内 input[type=range]）
		const range = document.querySelector('.color-picker input[type="range"]') as HTMLInputElement;
		await fireEvent.input(range, { target: { value: '42' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			const saved = get(triggers)[0];
			expect(saved.condition.value).toBe('#123456');
			expect(saved.condition.tolerance).toBe(42);
		});
	});
});

describe('触发器页：拾取弹窗关闭路径', () => {
	it('pixel 拾取弹窗点右上角关闭按钮复位，不回填', async () => {
		installCanvasStub();
		await openEditor(makeTrigger({
			// normalizeTrigger 在 pixel_changed 下以 region 坐标作为显示值
			condition: { type: 'pixel_changed', value: '', region: { x: 5, y: 5, width: 0, height: 0 }, tolerance: 10, interval: 1000 },
		}));

		await fireEvent.click(screen.getByTitle('从屏幕拾取像素坐标'));
		await screen.findByRole('dialog');
		await readyPickerModal();

		await fireEvent.click(screen.getByTitle('关闭 (Esc)'));
		await waitFor(() => {
			expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
		});
		// 关闭不回填
		expect((screen.getByPlaceholderText('100,200') as HTMLInputElement).value).toBe('5,5');
	});

	it('click 拾取坐标弹窗点取消按钮复位', async () => {
		installCanvasStub();
		await openEditor(makeTrigger());
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		await fireEvent.change(document.querySelector('.action-card select')!, { target: { value: 'click' } });
		await fireEvent.click(screen.getByTitle('从屏幕拾取点击坐标'));
		await screen.findByRole('dialog');
		await readyPickerModal();

		await fireEvent.click(screen.getByRole('button', { name: '取消' }));
		await waitFor(() => {
			expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
		});
		// 再次打开正常（clickPickIndex 已复位）
		await fireEvent.click(screen.getByTitle('从屏幕拾取点击坐标'));
		await screen.findByRole('dialog');
	});
});
