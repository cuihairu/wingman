import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/triggers/+page.svelte';
import { triggers, type TriggerConfig } from '$lib/stores/triggers';
import { logs } from '$lib/stores/logs';

/**
 * 触发器页第四组：条件/动作 switch 全类型、新建流程、删除确认取消、
 * 空格键选中、未知条件类型回显、动作按钮禁用态与缺省字段。
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
	logs.clear();
	triggers.set([]);
});

afterEach(() => {
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

/// 切换条件类型（bind:value 的 select 用 change 事件）
async function changeConditionType(type: string) {
	const select = document.getElementById('triggerConditionTypeSelect') as HTMLSelectElement;
	await fireEvent.change(select, { target: { value: type } });
}

describe('触发器页：条件类型 switch 全类型', () => {
	it.each([
		['image_found', '模板图片路径'],
		['pixel_changed', '监测像素坐标 (x,y)'],
		['window_opened', '窗口标题（模糊匹配）'],
		['process_started', '进程名 (如 game.exe)'],
		['hotkey_pressed', '热键（单键，如 F9 / 1 / A）'],
	])('条件 %s 显示 value 语义 %s', async (type, label) => {
		await openEditor(makeTrigger());
		await changeConditionType(type);
		expect(screen.getByText(label)).toBeInTheDocument();
	});
});

describe('触发器页：动作占位与按钮状态', () => {
	it('play_audio / stop_script 动作的参数占位', async () => {
		await openEditor(makeTrigger());
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		const card = document.querySelector('.action-card')!;
		const select = card.querySelector('select') as HTMLSelectElement;

		await fireEvent.change(select, { target: { value: 'play_audio' } });
		expect(card.querySelector('input')!.getAttribute('placeholder')).toBe('audio/alert.wav');

		await fireEvent.change(select, { target: { value: 'stop_script' } });
		expect(card.querySelector('input')!.getAttribute('placeholder')).toBe('脚本名或路径');
	});

	it('多个动作的首尾上移/下移禁用与未知类型占位回退', async () => {
		await openEditor(makeTrigger({
			// value 缺省（undefined）：changeActionType 后回退空串显示
			actions: [
				{ type: 'delay', delay: 100 } as any,
				{ type: 'log', value: '两条' } as any,
			],
		}));
		const upBtns = screen.getAllByTitle('上移');
		const downBtns = screen.getAllByTitle('下移');
		expect(upBtns[0]).toBeDisabled();
		expect(upBtns[1]).toBeEnabled();
		expect(downBtns[0]).toBeEnabled();
		expect(downBtns[1]).toBeDisabled();

		// delay 动作 value 为 undefined：切到 log 后输入框显示空串（?? '' 路径）
		const firstCard = document.querySelectorAll('.action-card')[0];
		const select = firstCard.querySelector('select') as HTMLSelectElement;
		await fireEvent.change(select, { target: { value: 'log' } });
		expect((firstCard.querySelector('input') as HTMLInputElement).value).toBe('');
	});

	it('click 动作坐标缺省显示 0', async () => {
		await openEditor(makeTrigger({
			actions: [{ type: 'click' } as any],
		}));
		const card = document.querySelector('.action-card')!;
		const inputs = [...card.querySelectorAll('input[type="number"]')] as HTMLInputElement[];
		expect(inputs.map(i => i.value)).toEqual(['0', '0']);
	});
});

describe('触发器页：删除边界', () => {
	it('删除确认取消时不移除触发器', async () => {
		vi.spyOn(window, 'confirm').mockReturnValue(false);
		await openEditor(makeTrigger());

		await fireEvent.click(screen.getByRole('button', { name: '删除' }));
		await new Promise(r => setTimeout(r, 50));
		expect(get(triggers)).toHaveLength(1);
		expect(screen.getByText('编辑触发器: 回血触发器')).toBeInTheDocument();
		expect(get(logs).some(e => e.message === '已删除触发器')).toBe(false);
	});
});

describe('触发器页：键盘选中', () => {
	it('空格键触发列表项选中', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		const item = screen.getByText('回血触发器').closest('.trigger-item, [class*="trigger"]') as HTMLElement;
		await fireEvent.keyDown(item, { key: ' ' });
		await waitFor(() => {
			expect(screen.getByText('编辑触发器: 回血触发器')).toBeInTheDocument();
		});
	});
});
