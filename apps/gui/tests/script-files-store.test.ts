import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { scriptFiles } from '$lib/stores/script-files';

/**
 * script-files store：dev（无 invoke）与 invoke 两种模式。
 * store 在模块求值时捕获 __TAURI_INVOKE__，invoke 模式用例整体重置注册表后动态加载。
 *
 * 该 store 暴露的 subscribe 是 {entries,root,...} 的订阅函数集合，
 * 需用 read() 辅助读取单值（svelte 的 get() 只接受完整 store 对象）。
 */

/// 读取单个订阅函数的当前值
function read<T>(sub: (fn: (v: T) => void) => () => void): T {
	let value!: T;
	const unsub = sub(v => {
		value = v;
	});
	unsub();
	return value;
}

type ScriptFilesStore = Awaited<typeof import('$lib/stores/script-files')>['scriptFiles'];

const ENTRY = { path: 'a.lua', name: 'a.lua', is_dir: false, size: 10, modified: 111 };

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	scriptFiles.set([]);
	scriptFiles.setSelected(null);
});

async function fresh(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	vi.resetModules();
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const mod = await import('$lib/stores/script-files');
	return mod.scriptFiles as ScriptFilesStore;
}

afterEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

describe('script-files store（dev 模式）', () => {
	it('set 注入并归一化缺省字段，truncated 可选', () => {
		scriptFiles.set([
			{ path: 'dir', name: '', is_dir: true, size: 0, modified: 5 },
			{ path: 'x/b.lua', name: '', is_dir: false, size: undefined as any, modified: NaN },
		], true);

		const items = read(scriptFiles.subscribe.entries);
		expect(items[0]).toEqual({ path: 'dir', name: 'dir', is_dir: true, size: 0, modified: 5 }); // name 回落 path
		expect(items[1].size).toBe(0);
		expect(Number.isNaN(items[1].modified)).toBe(false);
		expect(items[1].modified).toBe(0);
		expect(read(scriptFiles.subscribe.truncated)).toBe(true);

		scriptFiles.set([ENTRY]);
		expect(read(scriptFiles.subscribe.truncated)).toBe(false);
	});

	it('loadRoot/load 注入演示数据；setRoot 空值恢复默认', async () => {
		expect(scriptFiles.currentRoot()).toBe('');
		await scriptFiles.loadRoot();
		expect(scriptFiles.currentRoot()).toBe('/dev/demo/scripts-root');

		await scriptFiles.load();
		expect(read(scriptFiles.subscribe.entries).map(e => e.path)).toEqual([
			'config', 'config/triggers.lua', 'scripts', 'scripts/example.lua', 'scripts/example.py',
		]);

		await scriptFiles.setRoot('D:/custom');
		expect(read(scriptFiles.subscribe.root)).toEqual({ root: 'D:/custom', source: 'setting' });

		await scriptFiles.setRoot('');
		expect(read(scriptFiles.subscribe.root)).toEqual({ root: '/dev/demo/scripts-root', source: 'default' });
	});

	it('read 命中条目生成预览文本；未命中抛错', async () => {
		scriptFiles.set([ENTRY]);
		const content = await scriptFiles.read('a.lua');
		expect(content).toEqual({ path: 'a.lua', content: '-- dev 预览: a.lua\na.lua (10 B)\n', size: 10, modified: 111 });

		await expect(scriptFiles.read('missing.lua')).rejects.toThrow('文件不存在');
	});

	it('removeDev 本地删除并联动选中项', async () => {
		scriptFiles.set([ENTRY, { ...ENTRY, path: 'b.lua', name: 'b.lua' }]);
		scriptFiles.setSelected('a.lua');
		await scriptFiles.removeDev('a.lua');
		expect(read(scriptFiles.subscribe.entries).map(e => e.path)).toEqual(['b.lua']);
		expect(read(scriptFiles.subscribe.selected)).toBeNull();

		scriptFiles.setSelected('other');
		await scriptFiles.removeDev('b.lua');
		expect(read(scriptFiles.subscribe.selected)).toBe('other');
	});
});

describe('script-files store（invoke 模式）', () => {
	it('loadRoot 与 setRoot 走 IPC 并联动 load', async () => {
		const calls: Array<[string, unknown]> = [];
		const store = await fresh((cmd, args) => {
			calls.push([cmd, args]);
			if (cmd === 'get_scripts_root') return { root: 'C:/scripts', source: 'env' };
			if (cmd === 'set_scripts_root') return { root: 'C:/scripts2', source: 'setting' };
			if (cmd === 'list_script_files') return { entries: [ENTRY], truncated: true };
			return null;
		});

		await store.loadRoot();
		expect(read(store.subscribe.root)).toEqual({ root: 'C:/scripts', source: 'env' });

		await store.setSelected('old.lua');
		await store.setRoot('C:/scripts2');
		expect(read(store.subscribe.root)).toEqual({ root: 'C:/scripts2', source: 'setting' });
		expect(read(store.subscribe.selected)).toBeNull(); // setRoot 清空选中
		expect(calls.some(([c]) => c === 'list_script_files')).toBe(true);
	});

	it('load 归一化 IPC 结果并维护 loading 状态', async () => {
		const store = await fresh((cmd) => {
			if (cmd === 'list_script_files') {
				return { entries: [{ path: 'z.py', name: '', is_dir: 1, size: '9', modified: '7' }], truncated: 1 };
			}
			return null;
		});

		const loadingSamples: boolean[] = [];
		const unsub = store.subscribe.loading(v => loadingSamples.push(v));
		await store.load('sub');
		unsub();

		expect(loadingSamples[0]).toBe(false);
		expect(loadingSamples[loadingSamples.length - 1]).toBe(false);
		expect(loadingSamples.includes(true)).toBe(true);
		expect(read(store.subscribe.entries)[0]).toEqual({ path: 'z.py', name: 'z.py', is_dir: true, size: 9, modified: 7 });
		expect(read(store.subscribe.truncated)).toBe(true);
	});

	it('read/create/write/remove 走 IPC，删除联动选中并刷新列表', async () => {
		const store = await fresh((cmd, args) => {
			if (cmd === 'read_script_file') return { path: args?.path, content: 'x', size: 1, modified: 2 };
			if (cmd === 'delete_script_file') return null;
			return null;
		});

		await expect(store.read('f.lua')).resolves.toEqual({ path: 'f.lua', content: 'x', size: 1, modified: 2 });

		await store.create('new.lua');
		await store.write('new.lua', 'code');
		expect((window as any).__TAURI_INVOKE__).toHaveBeenCalledWith('write_script_file', { path: 'new.lua', content: '' });
		expect((window as any).__TAURI_INVOKE__).toHaveBeenCalledWith('write_script_file', { path: 'new.lua', content: 'code' });

		store.setSelected('new.lua');
		await store.remove('new.lua');
		expect((window as any).__TAURI_INVOKE__).toHaveBeenCalledWith('delete_script_file', { path: 'new.lua' });
		expect(read(store.subscribe.selected)).toBeNull();

		// 删除非选中项不影响选中
		store.setSelected('keep.lua');
		await store.remove('other.lua');
		expect(read(store.subscribe.selected)).toBe('keep.lua');

		// removeDev 在 invoke 模式下委托 remove
		await store.removeDev('g.lua');
		expect((window as any).__TAURI_INVOKE__).toHaveBeenCalledWith('delete_script_file', { path: 'g.lua' });
	});

	it('loadRoot IPC 失败静默忽略', async () => {
		const store = await fresh((cmd) => {
			if (cmd === 'get_scripts_root') throw new Error('boom');
			return null;
		});
		await expect(store.loadRoot()).resolves.toBeUndefined();
		expect(store.currentRoot()).toBe('');
	});
});
