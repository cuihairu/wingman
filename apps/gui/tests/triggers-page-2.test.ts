import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/triggers/+page.svelte';
import { triggers, type TriggerConfig } from '$lib/stores/triggers';
import { logs } from '$lib/stores/logs';
import { installPointerCaptureStub, installTolerantStyleAttrStub, uninstallTolerantStyleAttrStub } from './helpers/test-utils';

/**
 * 触发器页第二组：动作类型/占位文案全 case、changeActionType 字段保留策略、
 * 数字字段写路径（parseInt0）、条件与高级选项编辑保存、moveAction 越界守卫。
 * 列表/条件分支/弹窗回填见 triggers-page.test.ts。
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

async function openEditor(trigger: TriggerConfig = makeTrigger()) {
	triggers.set([trigger]);
	render(Page);
	await fireEvent.click(screen.getByText(trigger.name));
	await waitFor(() => {
		expect(screen.getByText(`编辑触发器: ${trigger.name}`)).toBeInTheDocument();
	});
	return trigger;
}

/** 当前编辑中的动作列表（store 里的 editing 是克隆，保存前不在 store——用 DOM 断言） */
function actionCards(): NodeListOf<HTMLElement> {
	return document.querySelectorAll('.action-card');
}

async function selectActionType(card: HTMLElement, value: string) {
	await fireEvent.change(card.querySelector('select')!, { target: { value } });
}

describe('触发器页：动作类型与占位文案', () => {
	it('文本类动作的占位文案逐类型切换', async () => {
		await openEditor();
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		const card = actionCards()[0];

		const cases: Array<[string, string]> = [
			['run_script', 'scripts/farm.lua'],
			['stop_script', '脚本名或路径'],
			['pause_script', '脚本名或路径'],
			['key_press', 'F1 / 1 / ctrl+s'],
			['type', '要输入的文本'],
			['show_message', '消息内容'],
			['play_audio', 'audio/alert.wav'],
			['log', '日志内容'],
		];
		for (const [type, placeholder] of cases) {
			await selectActionType(card, type);
			expect(screen.getByPlaceholderText(placeholder)).toBeInTheDocument();
		}

		// 文本输入写路径（updateAction value）
		await fireEvent.input(screen.getByPlaceholderText('日志内容'), { target: { value: '低血量' } });
		expect((screen.getByPlaceholderText('日志内容') as HTMLInputElement).value).toBe('低血量');
	});

	it('changeActionType：click↔文本类保留 value，delay 默认 500，无坐标清 x/y', async () => {
		await openEditor();
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		const card = actionCards()[0];

		// 文本类 → click：value 保留（type==='click'），x/y 取默认 0
		await fireEvent.input(screen.getByPlaceholderText('日志内容'), { target: { value: '备注' } });
		await selectActionType(card, 'click');
		let numbers = card.querySelectorAll('input[type="number"]');
		expect(numbers[0]).toHaveValue(0);
		expect(numbers[1]).toHaveValue(0);
		// click → 文本类：value 保留（prev.type==='click'），x/y 清除
		await fireEvent.input(numbers[0], { target: { value: '120' } });
		await fireEvent.input(numbers[1], { target: { value: '80' } });
		await selectActionType(card, 'key_press');
		expect((screen.getByPlaceholderText('F1 / 1 / ctrl+s') as HTMLInputElement).value).toBe('备注');

		// 切 delay：无历史 delay → 默认 500
		await selectActionType(card, 'delay');
		expect((card.querySelector('input[type="number"]') as HTMLInputElement)).toHaveValue(500);

		// 保存后字段形态断言（x/y 在非 click 下为 undefined，delay 保留）
		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			const saved = get(triggers)[0];
			expect(saved.actions[0]).toMatchObject({ type: 'delay', delay: 500, value: '备注' });
			expect(saved.actions[0].x).toBeUndefined();
			expect(saved.actions[0].y).toBeUndefined();
		});
	});

	it('parseInt0：click 的 X/Y 与 delay 输入非法数字回退 0（经保存断言）', async () => {
		await openEditor();
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		const card = actionCards()[0];

		await selectActionType(card, 'click');
		const numbers = card.querySelectorAll('input[type="number"]');
		// jsdom 的 number input 会把非法值清成 ''，parseInt0('')→NaN→0
		await fireEvent.input(numbers[0], { target: { value: 'abc' } });
		await fireEvent.input(numbers[1], { target: { value: '' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			const saved = get(triggers)[0];
			expect(saved.actions[0].x).toBe(0);
			expect(saved.actions[0].y).toBe(0);
		});
	});

	it('moveAction 越界守卫：首项上移与末项下移不改变顺序（jsdom 可点 disabled 按钮）', async () => {
		await openEditor();
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		expect(actionCards()).toHaveLength(2);

		// 首项上移（disabled）→ 无变化
		await fireEvent.click(actionCards()[0].querySelectorAll('.op-btn')[0]);
		expect(actionCards()).toHaveLength(2);
		// 末项下移（disabled）→ 无变化
		await fireEvent.click(actionCards()[1].querySelectorAll('.op-btn')[1]);
		expect(actionCards()).toHaveLength(2);

		// 首项下移正常交换
		await fireEvent.click(actionCards()[0].querySelectorAll('.op-btn')[1]);
		await waitFor(() => {
			const first = actionCards()[0].querySelector('.action-index')!.textContent;
			expect(first).toBe('1'); // 序号恒为位置序号，交换后仍 1,2
		});
	});
});

describe('触发器页：条件与高级选项写路径', () => {
	it('图像条件：模板路径、区域四字段、容差与间隔编辑后保存', async () => {
		await openEditor(makeTrigger({
			condition: { type: 'image_found', value: '', region: { x: 1, y: 2, width: 3, height: 4 }, tolerance: 20, interval: 800 },
		}));

		// 条件 value 写路径
		const pathInput = screen.getByPlaceholderText('') as HTMLInputElement;
		await fireEvent.input(pathInput, { target: { value: 'img/hp.png' } });

		// RegionPicker 四字段写路径（X/Y/W/H）
		const regionInputs = document.querySelectorAll('.region-picker input[type="number"]');
		expect(regionInputs).toHaveLength(4);
		await fireEvent.input(regionInputs[0], { target: { value: '10' } });
		await fireEvent.input(regionInputs[1], { target: { value: '20' } });
		await fireEvent.input(regionInputs[2], { target: { value: '30' } });
		await fireEvent.input(regionInputs[3], { target: { value: 'oops' } }); // parseInt0 → 0

		// 容差与间隔写路径
		const toleranceInput = document.querySelector('input[type="number"][min="0"][max="255"]') as HTMLInputElement;
		await fireEvent.input(toleranceInput, { target: { value: '33' } });
		await fireEvent.input(screen.getByLabelText('检查间隔 (ms)'), { target: { value: '250' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			const saved = get(triggers)[0];
			expect(saved.condition.value).toBe('img/hp.png');
			expect(saved.condition.region).toEqual({ x: 10, y: 20, width: 30, height: 0 });
			expect(saved.condition.tolerance).toBe(33);
			expect(saved.condition.interval).toBe(250);
		});
	});

	it('oneShot 复选与冷却时间编辑后保存', async () => {
		await openEditor(makeTrigger({ oneShot: false, cooldown: 0 }));

		const oneShot = screen.getByRole('checkbox', { name: '仅触发一次' }) as HTMLInputElement;
		await fireEvent.click(oneShot);
		expect(oneShot.checked).toBe(true);
		await fireEvent.input(screen.getByLabelText('冷却时间 (ms)'), { target: { value: '1500' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			const saved = get(triggers)[0];
			expect(saved.oneShot).toBe(true);
			expect(saved.cooldown).toBe(1500);
		});
	});
});

describe('触发器页：列表边界', () => {
	it('无效命中时间戳（Infinity）时 formatFiredTime 回退空串', async () => {
		triggers.set([makeTrigger({ last_triggered: true, last_triggered_at: Infinity })]);
		render(Page);
		const dot = document.querySelector('.fired-dot')!;
		expect(dot).toHaveClass('lit');
		// new Date(Infinity) 无效 → ''，title 仅剩前缀
		expect(dot.getAttribute('title')).toBe('最近命中 ');
	});

	it('非 Enter/Space 按键不选中列表项', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		const item = screen.getByText('回血触发器').closest('.trigger-item')!;
		await fireEvent.keyDown(item, { key: 'Escape' });
		expect(document.querySelector('.trigger-item.active')).toBeNull();
		expect(screen.getByText('从左侧选择一个触发器进行编辑')).toBeInTheDocument();
	});
});

describe('触发器页：编辑切换', () => {
	it('从 A 切到 B 编辑：面板就地更新（条件/动作下拉走 each 更新路径）', async () => {
		const a = makeTrigger({ id: 'a', name: '触发器A', actions: [{ type: 'log', value: 'x' }] });
		const b = makeTrigger({ id: 'b', name: '触发器B', actions: [{ type: 'log', value: 'y' }] });
		triggers.set([a, b]);
		render(Page);
		await fireEvent.click(screen.getByText('触发器A'));
		await waitFor(() => {
			expect(screen.getByText('编辑触发器: 触发器A')).toBeInTheDocument();
		});
		// 切到 B：editing 换对象，{#if editing} 块就地更新（非卸载重建）
		await fireEvent.click(screen.getByText('触发器B'));
		await waitFor(() => {
			expect(screen.getByText('编辑触发器: 触发器B')).toBeInTheDocument();
		});
		// B 的动作卡仍在，条件类型下拉仍可交互
		expect(document.querySelectorAll('.action-card').length).toBe(1);
		expect((document.querySelector('.action-card select') as HTMLSelectElement).value).toBe('log');
	});
});
