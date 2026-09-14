import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * script-files store 补充测试（核心链路见 script-files-store.test.ts）。
 * store 在模块求值时捕获 __TAURI_INVOKE__，invoke 用例先 resetModules
 * 再动态加载；subscribe 为复合订阅形态，用 read() 读取单值。
 */

function read<T>(sub: (fn: (v: T) => void) => () => void): T {
	let value!: T;
	const unsub = sub(v => { value = v; });
	unsub();
	return value;
}

let storeMod: { scriptFiles: any };

async function fresh(handler?: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	if (handler) {
		(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	} else {
		delete (window as any).__TAURI_INVOKE__;
	}
	storeMod = await import('$lib/stores/script-files');
	return storeMod.scriptFiles;
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

describe('script-files store（invoke 模式）', () => {
	it('load 拉取列表并规范条目字段', async () => {
		const invoke = vi.fn((cmd: string) => {
			if (cmd === 'list_script_files') {
				return {
					entries: [
						{ path: 'scripts', name: 'scripts', is_dir: true, size: 0, modified: 123 },
						{ path: 'scripts/a.lua', name: 'a.lua', is_dir: false, size: 42, modified: 456 },
					],
					truncated: true,
				};
			}
			return null;
		});
		const store = await fresh(invoke);

		await store.load();

		expect(invoke).toHaveBeenCalledWith('list_script_files', { subDir: null });
		const entries = read(store.subscribe.entries);
		expect(entries).toHaveLength(2);
		expect(entries[0]).toMatchObject({ path: 'scripts', is_dir: true, size: 0 });
		expect(entries[1]).toMatchObject({ path: 'scripts/a.lua', size: 42, modified: 456 });
		expect(read(store.subscribe.truncated)).toBe(true);
		expect(read(store.subscribe.loading)).toBe(false);
	});

	it('load 支持子目录参数并在结束后复位 loading', async () => {
		const invoke = vi.fn((cmd: string) => {
			if (cmd === 'list_script_files') return { entries: [], truncated: false };
			return null;
		});
		const store = await fresh(invoke);

		await store.load('scripts');

		expect(invoke).toHaveBeenCalledWith('list_script_files', { subDir: 'scripts' });
		expect(read(store.subscribe.loading)).toBe(false);
	});

	it('loadRoot 读取脚本根目录信息', async () => {
		const store = await fresh((cmd: string) => {
			if (cmd === 'get_scripts_root') return { root: '/opt/wingman/scripts', source: 'env' };
			return null;
		});

		await store.loadRoot();

		expect(read(store.subscribe.root)).toEqual({ root: '/opt/wingman/scripts', source: 'env' });
	});

	it('setRoot 调用后端、清空选中并刷新列表', async () => {
		const invoke = vi.fn((cmd: string) => {
			if (cmd === 'set_scripts_root') return { root: '/new/root', source: 'setting' };
			if (cmd === 'list_script_files') return { entries: [], truncated: false };
			return null;
		});
		const store = await fresh(invoke);
		store.setSelected('old.lua');

		await store.setRoot('/new/root');

		expect(invoke).toHaveBeenCalledWith('set_scripts_root', { path: '/new/root' });
		expect(invoke).toHaveBeenCalledWith('list_script_files', { subDir: null });
		expect(read(store.subscribe.root)).toEqual({ root: '/new/root', source: 'setting' });
		expect(read(store.subscribe.selected)).toBeNull();
	});

	it('create 走 write_script_file 空内容并刷新', async () => {
		const invoke = vi.fn((cmd: string) => {
			if (cmd === 'write_script_file') return { path: 'scripts/new.lua', name: 'new.lua', is_dir: false, size: 0, modified: 1 };
			if (cmd === 'list_script_files') {
				return { entries: [{ path: 'scripts/new.lua', name: 'new.lua', is_dir: false, size: 0, modified: 1 }], truncated: false };
			}
			return null;
		});
		const store = await fresh(invoke);

		await store.create('scripts/new.lua');

		expect(invoke).toHaveBeenCalledWith('write_script_file', { path: 'scripts/new.lua', content: '' });
		expect(read(store.subscribe.entries).map(e => e.path)).toContain('scripts/new.lua');
	});

	it('remove 删除后联动清空选中项', async () => {
		const invoke = vi.fn(() => null);
		const store = await fresh(invoke);
		store.setSelected('scripts/old.lua');

		await store.remove('scripts/old.lua');

		expect(invoke).toHaveBeenCalledWith('delete_script_file', { path: 'scripts/old.lua' });
		expect(read(store.subscribe.selected)).toBeNull();
	});

	it('read 返回后端内容', async () => {
		const store = await fresh((cmd: string) => {
			if (cmd === 'read_script_file') return { path: 'scripts/a.lua', content: '-- hi', size: 5, modified: 7 };
			return null;
		});

		const content = await store.read('scripts/a.lua');
		expect(content).toMatchObject({ path: 'scripts/a.lua', content: '-- hi', size: 5 });
	});
});

describe('script-files store（dev 模式，无 Tauri）', () => {
	it('load/loadRoot 填充演示数据供页面冒烟', async () => {
		const store = await fresh();

		await store.loadRoot();
		await store.load();

		expect(read(store.subscribe.entries).length).toBeGreaterThan(0);
		expect(store.currentRoot()).not.toBe('');
	});

	it('removeDev 本地移除条目并联动选中', async () => {
		const store = await fresh();
		await store.load();
		const before = read(store.subscribe.entries).length;
		store.setSelected('scripts/example.lua');

		await store.removeDev('scripts/example.lua');

		const after = read(store.subscribe.entries);
		expect(after).toHaveLength(before - 1);
		expect(after.some(e => e.path === 'scripts/example.lua')).toBe(false);
		expect(read(store.subscribe.selected)).toBeNull();
	});

	it('read 对存在的演示文件返回预览、对缺失文件与目录报错', async () => {
		const store = await fresh();
		await store.load();

		const preview = await store.read('scripts/example.lua');
		expect(preview.content).toContain('-- dev 预览: scripts/example.lua');

		await expect(store.read('scripts/missing.lua')).rejects.toThrow('文件不存在');
		await expect(store.read('scripts')).rejects.toThrow('文件不存在');
	});

	it('set 注入条目并做字段规范化', async () => {
		const store = await fresh();

		store.set([
			{ path: 'x.lua', name: 'x.lua', is_dir: false, size: 3, modified: 9 },
			{ path: 'y.lua', name: '', is_dir: undefined as any, size: '12' as any, modified: null as any },
		], true);

		const entries = read(store.subscribe.entries);
		expect(entries[0]).toEqual({ path: 'x.lua', name: 'x.lua', is_dir: false, size: 3, modified: 9 });
		// 缺失字段回退：name 用 path，is_dir false，数值 0
		expect(entries[1]).toEqual({ path: 'y.lua', name: 'y.lua', is_dir: false, size: 12, modified: 0 });
		expect(read(store.subscribe.truncated)).toBe(true);
	});
});
