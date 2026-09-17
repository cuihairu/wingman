import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { logs } from '$lib/stores/logs';
import { settings } from '$lib/stores/settings';
import { triggers, serializeForRuntime } from '$lib/stores/triggers';
import { scriptFiles } from '$lib/stores/script-files';

/**
 * stores 第三组：归一化边界与缺省回退。
 * serializeForRuntime/normalizeTrigger 主链路见 triggers-store*，时间戳边界见 stores-misc-2。
 */

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('serializeForRuntime 缺省值写路径', () => {
	it('pixel_changed 的 value 非法时坐标回退 0', () => {
		const out = serializeForRuntime({
			condition: { type: 'pixel_changed', value: '5,abc', region: { x: 0, y: 0, width: 3, height: 4 } } as any,
		});
		// y 解析 NaN → 0；region 展开保留 w/h
		expect(out.condition.region).toMatchObject({ x: 5, y: 0, width: 3, height: 4 });
	});

	it('pixel_changed 的 value 为空串时全 0 坐标', () => {
		const out = serializeForRuntime({
			condition: { type: 'pixel_changed', value: '', region: { x: 1, y: 2, width: 0, height: 0 } } as any,
		});
		expect(out.condition.region).toMatchObject({ x: 0, y: 0 });
	});

	it('hotkey_pressed 的 value 为空串时原样透传', () => {
		const out = serializeForRuntime({
			condition: { type: 'hotkey_pressed', value: '' } as any,
		});
		expect(out.condition.value).toBe('');
	});
});

describe('normalizeTrigger 的 pixel 显示值', () => {
	it('region 坐标缺省（undefined）时显示 0', () => {
		triggers.set([{
			condition: { type: 'pixel_changed', value: '', region: { y: 3, width: 1, height: 1 } as any },
		} as any]);
		expect(get(triggers)[0].condition.value).toBe('0,3');
	});

	it('无 region 时保留传入 value', () => {
		triggers.set([{
			condition: { type: 'pixel_changed', value: '9,9' } as any,
		} as any]);
		// region 为 undefined → 走 displayValue 分支保留原值
		expect(get(triggers)[0].condition.value).toBe('9,9');
	});
});

describe('triggers store：更新与命中的不匹配路径', () => {
	it('update 目标 id 不存在时列表不变', async () => {
		triggers.set([{ id: 'a', name: 'A', enabled: true, actions: [] } as any]);
		await triggers.update('missing', { name: 'X' } as any);
		expect(get(triggers)).toHaveLength(1);
		expect(get(triggers)[0].name).toBe('A');
	});

	it('markFired 省略时间戳时取当前时间', async () => {
		triggers.set([{ id: 'a', name: 'A', enabled: true, actions: [] } as any]);
		const before = Date.now();
		triggers.markFired('a');
		const entry = get(triggers)[0];
		expect(entry.last_triggered).toBe(true);
		expect(entry.last_triggered_at!).toBeGreaterThanOrEqual(before);
	});
});

describe('settings.logLevel 非法值回退 info', () => {
	it('非法级别按 info 过滤', () => {
		settings.update({ logLevel: 'garbage' as any });
		// debug 低于 info → 被丢弃；warning 高于 info → 保留
		logs.addRuntime('低级别', 'debug');
		logs.addRuntime('高级别', 'warning');
		const messages = get(logs).map(e => e.message);
		expect(messages).not.toContain('低级别');
		expect(messages).toContain('高级别');
		settings.update({ logLevel: 'info' });
	});
});

describe('script-files 归一化缺省', () => {
	it('list_script_files 条目缺 name 时用 path 兜底', async () => {
		vi.resetModules();
		(window as any).__TAURI_INVOKE__ = vi.fn(async (cmd: string) => {
			if (cmd === 'list_script_files') {
				return { entries: [{ path: 'x/y.lua', is_dir: false, size: 10, modified: 1 }], truncated: false };
			}
			if (cmd === 'get_scripts_root') return {};
			return {};
		});
		const sf = await import('$lib/stores/script-files');
		await sf.scriptFiles.load();
		let list: any[] = [];
		(sf.scriptFiles.subscribe as any).entries((s: any) => { list = s; })();
		expect(list[0].name).toBe('x/y.lua');
		delete (window as any).__TAURI_INVOKE__;
	});

	it('get_scripts_root 空响应回退默认根', async () => {
		vi.resetModules();
		(window as any).__TAURI_INVOKE__ = vi.fn(async (cmd: string) => {
			if (cmd === 'get_scripts_root') return {};
			if (cmd === 'list_script_files') return { entries: [], truncated: false };
			return {};
		});
		const sf = await import('$lib/stores/script-files');
		await sf.scriptFiles.loadRoot();
		let root: any = null;
		(sf.scriptFiles.subscribe as any).root((r: any) => { root = r; })();
		expect(root.root).toBe('');
		expect(root.source).toBe('default');
		delete (window as any).__TAURI_INVOKE__;
	});
});
