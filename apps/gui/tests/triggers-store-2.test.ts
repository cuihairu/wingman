import { describe, it, expect, beforeEach } from 'vitest';
import { get } from 'svelte/store';
import { triggers, serializeForRuntime, type TriggerConfig } from '$lib/stores/triggers';

/** 触发器 store 第二组：条件类型别名归一化补全与 hotkeyToVk 边界（见 triggers-store.test.ts） */

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	triggers.set([]);
});

describe('triggers store：条件类型别名归一化（补全）', () => {
	it('image_lost / window_opened / process_stopped 的紧凑别名', () => {
		const mk = (type: string): Partial<TriggerConfig> => ({
			name: '别名项',
			condition: { type, value: 'x', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 0, interval: 0 } as any,
		});
		triggers.set([
			{ id: '1', enabled: true, ...mk('imagelost') } as TriggerConfig,
			{ id: '2', enabled: true, ...mk('windowopened') } as TriggerConfig,
			{ id: '3', enabled: true, ...mk('processstopped') } as TriggerConfig,
		]);
		const items = get(triggers);
		expect(items.map(t => t.condition.type)).toEqual(['image_lost', 'window_opened', 'process_stopped']);
	});
});

describe('serializeForRuntime：hotkey 边界', () => {
	it('纯数字 VK 码原样透传', () => {
		const out = serializeForRuntime({
			condition: { type: 'hotkey_pressed', value: '120', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 0, interval: 0 } as any,
		});
		expect(out.condition.value).toBe('120');
	});

	it('未识别组合键原样返回', () => {
		const out = serializeForRuntime({
			condition: { type: 'hotkey_pressed', value: 'ctrl+s', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 0, interval: 0 } as any,
		});
		expect(out.condition.value).toBe('ctrl+s');
	});

	it('键名转换为 VK 码十进制串', () => {
		const out = serializeForRuntime({
			condition: { type: 'hotkey_pressed', value: 'F9', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 0, interval: 0 } as any,
		});
		expect(out.condition.value).toBe('120'); // 0x78
	});

	it('pixel_changed 的 value 写入 region.x/y', () => {
		const out = serializeForRuntime({
			condition: { type: 'pixel_changed', value: '30,40', region: { x: 0, y: 0, width: 100, height: 50 } } as any,
		});
		expect(out.condition.region).toMatchObject({ x: 30, y: 40, width: 100, height: 50 });
	});

	it('无 condition 原样返回', () => {
		expect(serializeForRuntime({ id: 'x', name: 'n' })).toEqual({ id: 'x', name: 'n' });
	});
});
