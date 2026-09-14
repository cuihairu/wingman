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

describe('触发器页面', () => {
	it('空态渲染与添加入口', () => {
		render(Page);
		expect(screen.getByText('触发器')).toBeInTheDocument();
		expect(screen.getByText('暂无触发器，点击"添加触发器"创建')).toBeInTheDocument();
		expect(screen.getByText('从左侧选择一个触发器进行编辑')).toBeInTheDocument();
	});

	it('添加触发器：dev 新建默认配置并进入编辑', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '添加触发器' }));
		expect(get(triggers)).toHaveLength(1);
		expect(get(triggers)[0].name).toBe('新建触发器');
		expect(screen.getByText('编辑触发器: 新建触发器')).toBeInTheDocument();
		expect(screen.getByLabelText('名称')).toHaveValue('新建触发器');
		expect(get(logs).some(e => e.message === '已创建触发器')).toBe(true);
	});

	it('列表项渲染条件中文标签、命中标记与启停按钮', async () => {
		triggers.set([
			makeTrigger({ id: 'a', name: '启用项', enabled: true, last_triggered: true, last_triggered_at: 1700000000000 }),
			makeTrigger({ id: 'b', name: '停用项', enabled: false, condition: { type: 'hotkey_pressed', value: 'F9', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 } }),
		]);
		const { container } = render(Page);

		expect(screen.getByText('启用项')).toBeInTheDocument();
		expect(screen.getByText('颜色找到')).toBeInTheDocument();
		expect(screen.getByText('热键按下')).toBeInTheDocument();

		const dots = container.querySelectorAll('.fired-dot');
		expect(dots[0]).toHaveClass('lit');
		expect(dots[0].getAttribute('title')).toBe(`最近命中 ${new Date(1700000000000).toTimeString().split(' ')[0]}`);
		expect(dots[1]).not.toHaveClass('lit');
		expect(dots[1].getAttribute('title')).toBe('尚未命中');

		// 启停切换
		const toggles = container.querySelectorAll('.toggle-btn');
		expect(toggles[0]).toHaveClass('enabled');
		expect(toggles[0].textContent).toBe('开');
		await fireEvent.click(toggles[0]);
		expect(get(triggers)[0].enabled).toBe(false);
	});

	it('选中触发器显示编辑面板；键盘 Enter/Space 也可选中', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		const item = screen.getByText('回血触发器').closest('.trigger-item')!;

		await fireEvent.keyDown(item, { key: 'Enter' });
		expect(screen.getByText('编辑触发器: 回血触发器')).toBeInTheDocument();

		// Space 同样选中
		await fireEvent.click(screen.getByRole('button', { name: '添加触发器' })); // 新建另一项
		const items = document.querySelectorAll('.trigger-item');
		await fireEvent.keyDown(items[1], { key: ' ' });
		expect(document.querySelector('.trigger-item.active')).toBe(items[1]);
	});

	it('条件类型分支 UI：颜色/像素/图像/窗口/进程/热键/定时', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		await fireEvent.click(screen.getByText('回血触发器'));

		const typeSelect = screen.getByLabelText('条件类型');

		// color_found：ColorPicker + 区域 + 容差（页签自身的"容差"与 ColorPicker 的 field-label 各一个）
		expect(screen.getByText('目标颜色（可从屏幕取色）')).toBeInTheDocument();
		expect(screen.getByText('搜索区域（可在截图上框选）')).toBeInTheDocument();
		expect(screen.getAllByText('容差').length).toBe(2);
		expect(screen.getByText('区域为 0 时搜索整个屏幕')).toBeInTheDocument();

		// pixel_changed：坐标 value + 拾取按钮 + 容差，无区域
		await fireEvent.change(typeSelect, { target: { value: 'pixel_changed' } });
		expect(screen.getByText('监测像素坐标 (x,y)')).toBeInTheDocument();
		expect(screen.getByTitle('从屏幕拾取像素坐标')).toBeInTheDocument();
		expect(screen.queryByText('搜索区域（可在截图上框选）')).not.toBeInTheDocument();
		expect(screen.getAllByText('容差').length).toBe(1); // 无 ColorPicker，仅条件面板自身

		// image_found：模板路径 + 区域 + 容差
		await fireEvent.change(typeSelect, { target: { value: 'image_found' } });
		expect(screen.getByText('模板图片路径')).toBeInTheDocument();
		expect(screen.getByText('搜索区域（可在截图上框选）')).toBeInTheDocument();
		expect(screen.getAllByText('容差').length).toBe(1);
		expect(screen.queryByTitle('从屏幕拾取像素坐标')).not.toBeInTheDocument();

		// window_closed：窗口标题，无区域无容差
		await fireEvent.change(typeSelect, { target: { value: 'window_closed' } });
		expect(screen.getByText('窗口标题（模糊匹配）')).toBeInTheDocument();
		expect(screen.queryByText('搜索区域（可在截图上框选）')).not.toBeInTheDocument();
		expect(screen.queryByText('容差')).not.toBeInTheDocument();

		// process_started / hotkey_pressed / time_elapsed
		await fireEvent.change(typeSelect, { target: { value: 'process_started' } });
		expect(screen.getByText('进程名 (如 game.exe)')).toBeInTheDocument();
		await fireEvent.change(typeSelect, { target: { value: 'hotkey_pressed' } });
		expect(screen.getByText('热键（单键，如 F9 / 1 / A）')).toBeInTheDocument();
		await fireEvent.change(typeSelect, { target: { value: 'time_elapsed' } });
		expect(screen.queryByText('进程名 (如 game.exe)')).not.toBeInTheDocument();
		// 定时触发没有 value 输入字段（仅有间隔）
		expect(screen.getByLabelText('检查间隔 (ms)')).toBeInTheDocument();
	});

	it('动作编辑：添加、类型切换字段、上移下移删除边界', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		await fireEvent.click(screen.getByText('回血触发器'));

		expect(screen.getByText('尚无动作，点击下方按钮添加')).toBeInTheDocument();
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		expect(screen.getByPlaceholderText('日志内容')).toBeInTheDocument();

		// 切换为 click：X/Y 与拾取坐标
		const actionSelect = document.querySelector('.action-card select')!;
		await fireEvent.change(actionSelect, { target: { value: 'click' } });
		let card = document.querySelector('.action-card')!;
		expect(card.textContent).toContain('X');
		expect(card.textContent).toContain('Y');
		expect(screen.getByTitle('从屏幕拾取点击坐标')).toBeInTheDocument();

		// 输入坐标（作用域限定在动作卡内，避免命中"检查间隔"数字输入）
		let actionNumbers = card.querySelectorAll('input[type="number"]');
		await fireEvent.input(actionNumbers[0], { target: { value: '35' } });
		expect(document.querySelectorAll('.action-card input[type="number"]')[0]).toHaveValue(35);

		// 切换为 delay：延时字段
		await fireEvent.change(actionSelect, { target: { value: 'delay' } });
		expect(screen.getByText('延时 (ms)')).toBeInTheDocument();

		// 再加一个动作测试排序
		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		let cards = document.querySelectorAll('.action-card');
		expect(cards).toHaveLength(2);

		// 第一个的上移禁用，下移可用
		const up0 = cards[0].querySelector('.op-btn')!;
		expect(up0).toBeDisabled();
		await fireEvent.click(cards[0].querySelectorAll('.op-btn')[1]); // 下移
		cards = document.querySelectorAll('.action-card');
		// 原第一个（delay）应到第二位：第二张卡含延时字段
		expect(cards[1].textContent).toContain('延时 (ms)');

		// 删除一个
		await fireEvent.click(cards[0].querySelector('.op-btn.danger')!);
		expect(document.querySelectorAll('.action-card')).toHaveLength(1);
	});

	it('保存与删除（确认/取消两条路径）', async () => {
		triggers.set([makeTrigger()]);
		render(Page);
		await fireEvent.click(screen.getByText('回血触发器'));

		// 保存
		const nameInput = screen.getByLabelText('名称');
		await fireEvent.input(nameInput, { target: { value: '改名触发器' } });
		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		expect(get(triggers)[0].name).toBe('改名触发器');
		expect(get(logs).some(e => e.message === '已保存触发器: 改名触发器')).toBe(true);

		// 删除取消
		const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(false);
		await fireEvent.click(screen.getByRole('button', { name: '删除' }));
		expect(confirmSpy).toHaveBeenCalledWith(expect.stringContaining('改名触发器'));
		expect(get(triggers)).toHaveLength(1);

		// 删除确认
		confirmSpy.mockReturnValue(true);
		await fireEvent.click(screen.getByRole('button', { name: '删除' }));
		expect(get(triggers)).toHaveLength(0);
		expect(get(logs).some(e => e.message === '已删除触发器')).toBe(true);
		expect(screen.getByText('从左侧选择一个触发器进行编辑')).toBeInTheDocument();
	});

	it('pixel_changed 的拾取按钮打开取色弹窗并回填坐标', async () => {
		installCanvasStub();
		triggers.set([makeTrigger({ condition: { type: 'pixel_changed', value: '', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 } })]);
		render(Page);
		await fireEvent.click(screen.getByText('回血触发器'));

		await fireEvent.click(screen.getByTitle('从屏幕拾取像素坐标'));
		expect(await screen.findByRole('dialog')).toBeInTheDocument();
		await readyPickerModal();

		const surface = screen.getByRole('img', { name: '屏幕截图，点击拾取颜色' });
		await fireEvent.pointerDown(surface, { clientX: 12.8, clientY: 7.2, pointerId: 1 });
		await fireEvent.click(await screen.findByRole('button', { name: '确定' }));

		// value 回填屏幕绝对坐标
		expect((screen.getByPlaceholderText('100,200') as HTMLInputElement).value).toBe('128,72');
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});

	it('click 动作的拾取坐标弹窗回填 X/Y', async () => {
		installCanvasStub();
		triggers.set([makeTrigger()]);
		render(Page);
		await fireEvent.click(screen.getByText('回血触发器'));

		await fireEvent.click(screen.getByRole('button', { name: '+ 添加动作' }));
		await fireEvent.change(document.querySelector('.action-card select')!, { target: { value: 'click' } });
		await fireEvent.click(screen.getByTitle('从屏幕拾取点击坐标'));

		await readyPickerModal();
		const surface = screen.getByRole('img', { name: '屏幕截图，点击拾取颜色' });
		await fireEvent.pointerDown(surface, { clientX: 5, clientY: 2.5, pointerId: 1 });
		await fireEvent.click(await screen.findByRole('button', { name: '确定' }));

		const actionNumbers = document.querySelectorAll('.action-card input[type="number"]');
		expect(actionNumbers[0]).toHaveValue(50);
		expect(actionNumbers[1]).toHaveValue(25);
		expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
	});

	it('高级选项：仅触发一次与冷却时间', async () => {
		triggers.set([makeTrigger({ oneShot: true, cooldown: 500 })]);
		render(Page);
		await fireEvent.click(screen.getByText('回血触发器'));

		const oneShot = screen.getByRole('checkbox', { name: '仅触发一次' });
		expect(oneShot).toBeChecked();
		expect(screen.getByLabelText('冷却时间 (ms)')).toHaveValue(500);
	});
});
