import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type TriggersModule = Awaited<typeof import('$lib/stores/triggers')>;
type TriggerConfig = TriggersModule['TriggerConfig'];

const INVOKE_KEY = '__TAURI_INVOKE__';

async function fresh(): Promise<TriggersModule> {
	vi.resetModules();
	return import('$lib/stores/triggers');
}

const baseCondition = { region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 };

describe('triggers 值转换函数', () => {
	it('hexColorToRuntime：#rrggbb → 0xRRGGBB，无法识别原样返回', async () => {
		const { hexColorToRuntime } = await fresh();
		expect(hexColorToRuntime('#ff0000')).toBe('0xFF0000');
		expect(hexColorToRuntime('ff0000')).toBe('0xFF0000');
		expect(hexColorToRuntime('  #10FF00  '.trim())).toBe('0x10FF00');
		expect(hexColorToRuntime(' 0x10FF00 ')).toBe(' 0x10FF00 '); // 带空白/0x 前缀的值不匹配，原样返回
		expect(hexColorToRuntime('red')).toBe('red');
	});

	it('vkToHotkey：VK 码 → 键名，未知码原样返回', async () => {
		const { vkToHotkey } = await fresh();
		expect(vkToHotkey('120')).toBe('F9');
		expect(vkToHotkey('48')).toBe('0');
		expect(vkToHotkey('65')).toBe('A');
		expect(vkToHotkey('32')).toBe('SPACE');
		expect(vkToHotkey('13')).toBe('ENTER');
		expect(vkToHotkey('99999')).toBe('99999');
		expect(vkToHotkey('F9')).toBe('F9');
		expect(vkToHotkey('')).toBe('');
	});
});

describe('serializeForRuntime（GUI → runtime IPC 值格式）', () => {
	it('无 condition 时原样返回', async () => {
		const { serializeForRuntime } = await fresh();
		expect(serializeForRuntime({})).toEqual({});
	});

	it('color 条件转换 value；hotkey 转换为 VK 码；pixel_changed 取 value 写入 region.x/y', async () => {
		const { serializeForRuntime } = await fresh();
		const color = serializeForRuntime({
			condition: { ...baseCondition, type: 'color_found', value: '#ff0000' },
		} as Partial<TriggerConfig>);
		expect(color.condition.value).toBe('0xFF0000');

		const hotkey = serializeForRuntime({
			condition: { ...baseCondition, type: 'hotkey_pressed', value: 'F9' },
		} as Partial<TriggerConfig>);
		expect(hotkey.condition.value).toBe('120');

		const pixel = serializeForRuntime({
			condition: { ...baseCondition, type: 'pixel_changed', value: '100, 200' },
		} as Partial<TriggerConfig>);
		expect(pixel.condition.value).toBe('100, 200');
		expect(pixel.condition.region).toMatchObject({ x: 100, y: 200 });
	});

	it('其他类型不做值转换', async () => {
		const { serializeForRuntime } = await fresh();
		const out = serializeForRuntime({
			condition: { ...baseCondition, type: 'time_elapsed', value: '5000' },
		} as Partial<TriggerConfig>);
		expect(out.condition.value).toBe('5000');
	});
});

describe('normalizeTrigger（runtime → GUI 显示格式）', () => {
	it('各条件类型别名归一化，未知回退 color_found', async () => {
		const { triggers } = await fresh();
		triggers.set([
			{ id: '1', name: '', enabled: true, condition: { ...baseCondition, type: 'colorfound', value: '' }, actions: [] },
			{ id: '2', name: '', enabled: true, condition: { ...baseCondition, type: 'pixel', value: '' }, actions: [] },
			{ id: '3', name: '', enabled: true, condition: { ...baseCondition, type: 'ImageFound', value: '' }, actions: [] },
			{ id: '4', name: '', enabled: true, condition: { ...baseCondition, type: 'windowclosed', value: '' }, actions: [] },
			{ id: '5', name: '', enabled: true, condition: { ...baseCondition, type: 'process_started', value: '' }, actions: [] },
			{ id: '6', name: '', enabled: true, condition: { ...baseCondition, type: 'timeelapsed', value: '' }, actions: [] },
			{ id: '7', name: '', enabled: true, condition: { ...baseCondition, type: 'HotkeyPressed', value: '' }, actions: [] },
			{ id: '8', name: '', enabled: true, condition: { ...baseCondition, type: 'bogus', value: '' }, actions: [] },
		] as any);
		const types = get(triggers).map(t => t.condition.type);
		expect(types).toEqual([
			'color_found', 'color_found', 'image_found', 'window_closed',
			'process_started', 'time_elapsed', 'hotkey_pressed', 'color_found',
		]);
	});

	it('runtime 值转 GUI 显示：0x 颜色、十进制颜色、VK 码热键、pixel_changed 用 region 坐标', async () => {
		const { triggers } = await fresh();
		triggers.set([
			{ id: '1', name: '', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '0xFF0000' }, actions: [] },
			{ id: '2', name: '', enabled: true, condition: { ...baseCondition, type: 'color_lost', value: '16711680' }, actions: [] },
			{ id: '3', name: '', enabled: true, condition: { ...baseCondition, type: 'color_lost', value: '0xABC' }, actions: [] },
			{ id: '4', name: '', enabled: true, condition: { ...baseCondition, type: 'hotkey_pressed', value: '120' }, actions: [] },
			{
				id: '5', name: '', enabled: true,
				condition: { ...baseCondition, type: 'pixel_changed', value: '', region: { x: 30, y: 40, width: 0, height: 0 } },
				actions: [],
			},
			{ id: '6', name: '', enabled: true, condition: { ...baseCondition, type: 'color_found', value: 'zzz' }, actions: [] },
			// 16777216 = 0x1000000 > 0xffffff：超范围十进制不转 hex，原样返回
			{ id: '7', name: '', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '16777216' }, actions: [] },
		] as any);
		const values = get(triggers).map(t => t.condition.value);
		expect(values).toEqual(['#ff0000', '#ff0000', '#000abc', 'F9', '30,40', 'zzz', '16777216']);
	});

	it('缺失字段补默认值（id/name/enabled/region/tolerance/interval/oneShot/cooldown）', async () => {
		const { triggers } = await fresh();
		triggers.set([{}] as any);
		const t = get(triggers)[0];
		expect(t.id).toMatch(/^[0-9a-f-]{36}$/);
		expect(t.name).toBe('未命名触发器');
		expect(t.enabled).toBe(true);
		expect(t.condition.region).toEqual({ x: 0, y: 0, width: 0, height: 0 });
		expect(t.condition.tolerance).toBe(10);
		expect(t.condition.interval).toBe(1000);
		expect(t.oneShot).toBe(false);
		expect(t.cooldown).toBe(0);
		expect(t.last_triggered).toBe(false);
		expect(t.actions).toEqual([]);
	});

	it('one_shot/lastTriggered/lastTriggerTime 下划线命名兼容，trigger_type 保留原始值', async () => {
		const { triggers } = await fresh();
		triggers.set([{
			id: 'x', name: 'n', enabled: true, type: 'time_elapsed',
			condition: { ...baseCondition, type: 'time_elapsed', value: '1' },
			actions: [{ type: 'KEYPRESS', value: 'F1' }, { type: 0 }, { type: 9 }, { type: 'weird' }],
			one_shot: true, cooldown: 5, lastTriggered: true, lastTriggerTime: 1234,
		}] as any);
		const t = get(triggers)[0];
		expect(t.trigger_type).toBe('time_elapsed');
		expect(t.oneShot).toBe(true);
		expect(t.cooldown).toBe(5);
		expect(t.last_triggered).toBe(true);
		expect(t.last_triggered_at).toBe(1234);
		expect(t.actions.map(a => a.type)).toEqual(['key_press', 'run_script', 'delay', 'log']);
	});

	it('动作类型数字枚举映射（runtime 枚举值 → GUI 类型）', async () => {
		const { triggers } = await fresh();
		triggers.set([{
			id: 'x', name: '', enabled: true,
			condition: { ...baseCondition, type: 'time_elapsed', value: '' },
			actions: [{ type: 1 }, { type: 2 }, { type: 3 }, { type: 4 }, { type: 5 }, { type: 6 }, { type: 7 }, { type: 8 }, {}],
		}] as any);
		expect(get(triggers)[0].actions.map(a => a.type)).toEqual([
			'click', 'key_press', 'type', 'stop_script', 'pause_script', 'show_message', 'play_audio', 'log', 'log',
		]);
	});

	it('动作字符串别名归一化（runscript/macro/keypress/key/showmessage/playaudio）', async () => {
		const { triggers } = await fresh();
		triggers.set([{
			id: 'x', name: '', enabled: true,
			condition: { ...baseCondition, type: 'time_elapsed', value: '' },
			actions: [
				{ type: 'runscript' }, { type: 'macro' }, { type: 'stopscript' }, { type: 'pausescript' },
				{ type: 'click' }, { type: 'keypress' }, { type: 'key' }, { type: 'type' },
				{ type: 'showmessage' }, { type: 'playaudio' }, { type: 'delay' }, { type: 'log' },
			],
		}] as any);
		expect(get(triggers)[0].actions.map(a => a.type)).toEqual([
			'run_script', 'run_script', 'stop_script', 'pause_script',
			'click', 'key_press', 'key_press', 'type',
			'show_message', 'play_audio', 'delay', 'log',
		]);
	});
});

describe('triggers store（dev 模式，无 __TAURI_INVOKE__）', () => {
	beforeEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('add 新增并返回 id，未指定 id 时用时间戳', async () => {
		const { triggers } = await fresh();
		const id = await triggers.add({ name: 'T1', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '#00ff00' }, actions: [] });
		// 断言时不重复取 Date.now()（毫秒跳变会 flaky），只验证 id 是 13 位毫秒时间戳
		expect(id).toMatch(/^\d{13}$/);
		expect(get(triggers)).toHaveLength(1);
		expect(get(triggers)[0].name).toBe('T1');
	});

	it('toggle 返回切换后的 enabled', async () => {
		const { triggers } = await fresh();
		const id = await triggers.add({ name: 'T', enabled: false, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] });
		await expect(triggers.toggle(id)).resolves.toBe(true);
		expect(get(triggers)[0].enabled).toBe(true);
		await expect(triggers.toggle(id)).resolves.toBe(false);
	});

	it('remove 删除指定项', async () => {
		const { triggers } = await fresh();
		const id = await triggers.add({ name: 'T', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] });
		await triggers.remove(id);
		expect(get(triggers)).toHaveLength(0);
	});

	it('update 合并字段', async () => {
		const { triggers } = await fresh();
		const id = await triggers.add({ name: 'T', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] });
		await triggers.update(id, { name: 'T2' });
		expect(get(triggers)[0].name).toBe('T2');
	});

	it('markFired 按 id 或 name 匹配并记录时间戳', async () => {
		const { triggers } = await fresh();
		triggers.set([
			{ id: '1', name: '像素检测', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] },
			{ id: '2', name: '定时器', enabled: true, condition: { ...baseCondition, type: 'time_elapsed', value: '' }, actions: [] },
		]);
		expect(triggers.markFired('1', undefined, 111)).toBe(true);
		expect(triggers.markFired(0, '定时器', 222)).toBe(true); // 数字 id 0 + name 匹配
		expect(triggers.markFired('ghost', 'nope', 333)).toBe(false);

		const items = get(triggers);
		expect(items[0]).toMatchObject({ last_triggered: true, last_triggered_at: 111 });
		expect(items[1]).toMatchObject({ last_triggered: true, last_triggered_at: 222 });

		// 缺省 timestamp 用当前时间
		const before = Date.now();
		triggers.markFired('1');
		expect(get(triggers)[0].last_triggered_at).toBeGreaterThanOrEqual(before);
	});

	it('loadDevData 填充示例触发器', async () => {
		const { triggers } = await fresh();
		triggers.loadDevData();
		expect(get(triggers).map(t => t.name)).toEqual(['像素检测-HP药水', '定时器-每5秒']);
	});
});

describe('triggers store（invoke 模式）', () => {
	let invokeMock: ReturnType<typeof vi.fn>;

	beforeEach(async () => {
		invokeMock = vi.fn();
		(window as any)[INVOKE_KEY] = invokeMock;
	});

	afterEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	it('load 拉取并归一化；失败静默', async () => {
		const { triggers } = await fresh();
		invokeMock.mockResolvedValueOnce([{ id: '1', name: 'T', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '0xff0000' }, actions: [] }]);
		await triggers.load();
		expect(invokeMock).toHaveBeenCalledWith('get_triggers');
		expect(get(triggers)[0].condition.value).toBe('#ff0000');

		invokeMock.mockRejectedValueOnce(new Error('down'));
		await expect(triggers.load()).resolves.toBeUndefined();
	});

	it('add 序列化后下发并刷新；失败返回空串', async () => {
		const { triggers } = await fresh();
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'add_trigger') return 'srv-1';
			if (cmd === 'get_triggers') return [{ id: 'srv-1', name: 'T', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] }];
			return null;
		});
		const id = await triggers.add({ name: 'T', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '#ff0000' }, actions: [] });
		expect(id).toBe('srv-1');
		expect(invokeMock).toHaveBeenCalledWith('add_trigger', {
			config: expect.objectContaining({ condition: expect.objectContaining({ value: '0xFF0000' }) }),
		});
		expect(get(triggers)).toHaveLength(1);

		invokeMock.mockRejectedValue(new Error('fail'));
		await expect(triggers.add({ name: 'X', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] })).resolves.toBe('');
	});

	it('remove/update/toggle 走后端并刷新', async () => {
		const { triggers } = await fresh();
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'get_triggers') {
				return [{ id: '1', name: 'T', enabled: true, condition: { ...baseCondition, type: 'color_found', value: '' }, actions: [] }];
			}
			if (cmd === 'toggle_trigger') return false;
			return null;
		});
		await triggers.load();

		await triggers.remove('1');
		expect(invokeMock).toHaveBeenCalledWith('remove_trigger', { id: '1' });

		await triggers.update('1', { name: 'T2' });
		expect(invokeMock).toHaveBeenCalledWith('update_trigger', { id: '1', config: expect.anything() });

		await expect(triggers.toggle('1')).resolves.toBe(false);
		expect(invokeMock).toHaveBeenCalledWith('toggle_trigger', { id: '1' });
	});

	it('toggle 失败返回 false；remove/update 失败静默', async () => {
		const { triggers } = await fresh();
		invokeMock.mockRejectedValue(new Error('down'));
		await expect(triggers.toggle('1')).resolves.toBe(false);
		await expect(triggers.remove('1')).resolves.toBeUndefined();
		await expect(triggers.update('1', {})).resolves.toBeUndefined();
	});
});
