import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { logs } from '$lib/stores/logs';
import { scriptFiles, type ScriptFileEntry } from '$lib/stores/script-files';

/** stores 第二组：logs 时间戳边界、script-files dev 写入路径、connection 非错误对象消息 */

const TIME_RE = /^\[\d{2}:\d{2}:\d{2}\]$/;

/// scriptFiles 为复合 store（subscribe.entries 等分片），手动快照读取
function entriesOf(): ScriptFileEntry[] {
	let list: ScriptFileEntry[] = [];
	const unsub = (scriptFiles.subscribe as any).entries((state: ScriptFileEntry[]) => {
		list = state;
	});
	unsub();
	return list;
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('logs.addRuntime 时间戳边界', () => {
	it('ts 省略时取当前时间', () => {
		logs.addRuntime('rt-事件');
		const entries = get(logs);
		expect(entries).toHaveLength(1);
		expect(entries[0].source).toBe('runtime');
		expect(entries[0].time).toMatch(TIME_RE);
	});

	it('ts 为 Infinity（Date 无效）时回退当前时间', () => {
		logs.addRuntime('无效时间戳', 'info', Infinity);
		const entry = get(logs).at(-1)!;
		expect(entry.message).toBe('无效时间戳');
		expect(entry.time).toMatch(TIME_RE);
	});

	it('ts 正常时格式化为该时刻的时间', () => {
		// 固定时刻 12:34:56（本地时区）
		const fixed = new Date(2026, 0, 1, 12, 34, 56).getTime();
		logs.addRuntime('固定时刻', 'info', fixed);
		expect(get(logs).at(-1)!.time).toBe('[12:34:56]');
	});
});

describe('script-files dev 写入路径', () => {
	it('create 已存在路径不产生重复条目', async () => {
		await scriptFiles.load();
		const before = entriesOf();
		expect(before.length).toBeGreaterThan(0);

		await scriptFiles.create('scripts/example.lua');
		const after = entriesOf();
		expect(after).toHaveLength(before.length);
		expect(after.filter(e => e.path === 'scripts/example.lua')).toHaveLength(1);
	});

	it('write 新路径在 dev 列表追加条目', async () => {
		await scriptFiles.load();
		const before = entriesOf();

		await scriptFiles.write('scripts/added.lua', 'print(1)');
		const after = entriesOf();
		expect(after).toHaveLength(before.length + 1);
		expect(after.some(e => e.path === 'scripts/added.lua' && !e.is_dir)).toBe(true);
	});
});

describe('connection 非错误对象的错误消息（invoke 模式）', () => {
	it('connect_ipc 抛出字符串时错误面板显示 String 形式', async () => {
		vi.resetModules();
		(window as any).__TAURI_INVOKE__ = vi.fn((cmd: string) => {
			if (cmd === 'connect_ipc') throw 'dial unix: raw failure';
			return {};
		});
		const { connection } = await import('$lib/stores/connection');
		await expect(connection.connect()).rejects.toBe('dial unix: raw failure');

		const state = get(connection);
		expect(state.ipc.state).toBe('error');
		expect(state.ipc.message).toBe('dial unix: raw failure');
		expect(state.connected).toBe(false);
		delete (window as any).__TAURI_INVOKE__;
	});
});
