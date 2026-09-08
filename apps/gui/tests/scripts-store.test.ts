import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';

type ScriptsStore = Awaited<typeof import('$lib/stores/scripts')>['scripts'];

const INVOKE_KEY = '__TAURI_INVOKE__';

/// 构造 runtime 侧原始脚本对象（未经规范化）
function raw(id: string, state = 'stopped', extra: Record<string, unknown> = {}) {
	return {
		id,
		name: `${id}.lua`,
		path: `scripts/${id}.lua`,
		size: 1024,
		state,
		error: '',
		loaded_at: 1000,
		...extra,
	};
}

/// 重新加载 store 模块，保证 __TAURI_INVOKE__ 在模块求值时生效
async function freshStore(): Promise<ScriptsStore> {
	vi.resetModules();
	const mod = await import('$lib/stores/scripts');
	return mod.scripts;
}

describe('scripts store（dev 模式，无 __TAURI_INVOKE__）', () => {
	let store: ScriptsStore;

	beforeEach(async () => {
		delete (window as any)[INVOKE_KEY];
		store = await freshStore();
	});

	describe('set() 规范化', () => {
		it('未知/缺失 state 回退 unknown，is_running 由 state 派生而非信任输入', () => {
			store.set([
				raw('a', 'RUNNING', { is_running: false }),
				raw('b', 'bogus', { is_running: true }),
				{ id: 'c' },
			]);
			const list = get(store);
			expect(list[0]).toMatchObject({ id: 'a', state: 'running', is_running: true });
			expect(list[1]).toMatchObject({ state: 'unknown', is_running: false });
			expect(list[2]).toMatchObject({
				name: '',
				path: '',
				size: 0,
				error: '',
				loaded_at: 0,
				state: 'unknown',
				is_running: false,
			});
		});

		it('name 缺失时回退 path，loaded_at 兼容 loadedAt 驼峰命名', () => {
			store.set([{ id: 'x', path: 'scripts/x.lua', state: 'stopped', loadedAt: 456 }]);
			expect(get(store)[0]).toMatchObject({ name: 'scripts/x.lua', loaded_at: 456 });
		});
	});

	describe('生命周期操作（本地状态切换）', () => {
		it('start/pause/resume/restart/stop 切换状态并同步派生 is_running', async () => {
			store.set([raw('a', 'stopped')]);

			await store.start('a');
			expect(get(store)[0]).toMatchObject({ state: 'running', is_running: true });

			await store.pause('a');
			expect(get(store)[0]).toMatchObject({ state: 'paused', is_running: false });

			await store.resume('a');
			expect(get(store)[0]).toMatchObject({ state: 'running', is_running: true });

			await store.restart('a');
			expect(get(store)[0]).toMatchObject({ state: 'running', is_running: true });

			await store.stop('a');
			expect(get(store)[0]).toMatchObject({ state: 'stopped', is_running: false });
		});

		it('unload 从列表中移除脚本', async () => {
			store.set([raw('a'), raw('b')]);
			await store.unload('a');
			expect(get(store).map(s => s.id)).toEqual(['b']);
		});

		it('操作未加载的 id 不影响其它脚本', async () => {
			store.set([raw('a', 'running')]);
			await store.pause('missing');
			expect(get(store)[0].state).toBe('running');
		});
	});

	describe('批量操作（页面 bulkOp 的 store 侧语义）', () => {
		it('批量暂停：仅 running 脚本被暂停', async () => {
			store.set([
				raw('r1', 'running'),
				raw('r2', 'running'),
				raw('p', 'paused'),
				raw('s', 'stopped'),
			]);
			for (const s of get(store).filter(x => x.state === 'running')) {
				await store.pause(s.id);
			}
			expect(Object.fromEntries(get(store).map(s => [s.id, s.state]))).toEqual({
				r1: 'paused',
				r2: 'paused',
				p: 'paused',
				s: 'stopped',
			});
		});

		it('批量恢复：仅 paused 脚本恢复运行', async () => {
			store.set([
				raw('p1', 'paused'),
				raw('p2', 'paused'),
				raw('s', 'stopped'),
				raw('e', 'error'),
			]);
			for (const s of get(store).filter(x => x.state === 'paused')) {
				await store.resume(s.id);
			}
			expect(get(store).filter(x => x.state === 'running').map(x => x.id)).toEqual(['p1', 'p2']);
		});

		it('批量停止：running 与 paused 全部停止，其余不动', async () => {
			store.set([
				raw('r', 'running'),
				raw('p', 'paused'),
				raw('l', 'loaded'),
			]);
			for (const s of get(store).filter(x => x.state === 'running' || x.state === 'paused')) {
				await store.stop(s.id);
			}
			expect(Object.fromEntries(get(store).map(s => [s.id, s.state]))).toEqual({
				r: 'stopped',
				p: 'stopped',
				l: 'loaded',
			});
		});
	});

	describe('applyState（runtime script.state_changed 事件联动）', () => {
		it('匹配 id 时更新状态、派生 is_running 并返回 true', () => {
			store.set([raw('a', 'running')]);
			expect(store.applyState('a', 'paused')).toBe(true);
			expect(get(store)[0]).toMatchObject({ state: 'paused', is_running: false });
			expect(store.applyState('a', 'running')).toBe(true);
			expect(get(store)[0]).toMatchObject({ state: 'running', is_running: true });
		});

		it('事件携带 error 时更新错误信息，未携带时保留原值', () => {
			store.set([raw('a', 'running')]);
			store.applyState('a', 'error', 'boom');
			expect(get(store)[0].error).toBe('boom');
			store.applyState('a', 'stopped');
			expect(get(store)[0].error).toBe('boom');
		});

		it('id 不匹配（事件先于列表加载）返回 false 且不改动列表', () => {
			store.set([raw('a', 'running')]);
			expect(store.applyState('ghost', 'paused')).toBe(false);
			expect(get(store)[0].state).toBe('running');
		});

		it('非法/未知状态字符串被忽略', () => {
			store.set([raw('a', 'running')]);
			expect(store.applyState('a', 'exploded')).toBe(false);
			expect(store.applyState('a', '')).toBe(false);
			expect(get(store)[0].state).toBe('running');
		});
	});
});

describe('scripts store（invoke 模式，mock __TAURI_INVOKE__）', () => {
	let store: ScriptsStore;
	let invokeMock: ReturnType<typeof vi.fn>;

	beforeEach(async () => {
		invokeMock = vi.fn();
		(window as any)[INVOKE_KEY] = invokeMock;
		store = await freshStore();
	});

	afterEach(() => {
		delete (window as any)[INVOKE_KEY];
	});

	/// 模拟 runtime 后端：get_scripts 返回 list，其余命令记录后返回 null
	function mockBackend(list: () => any[]) {
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'get_scripts') return list();
			return null;
		});
	}

	it('load() 调用 get_scripts 并规范化结果', async () => {
		mockBackend(() => [raw('a', 'running'), raw('b', 'bogus')]);
		await store.load();
		expect(invokeMock).toHaveBeenCalledWith('get_scripts');
		expect(get(store).map(s => s.state)).toEqual(['running', 'unknown']);
	});

	it('load() 失败时静默忽略并保持现有列表', async () => {
		invokeMock.mockRejectedValue(new Error('ipc down'));
		store.set([raw('a', 'running')]);
		await expect(store.load()).resolves.toBeUndefined();
		expect(get(store)).toHaveLength(1);
		expect(get(store)[0].state).toBe('running');
	});

	it('start 传递 id 与 path 并在成功后刷新列表', async () => {
		mockBackend(() => [raw('a', 'running')]);
		await store.start('a', 'scripts/a.lua');
		expect(invokeMock).toHaveBeenCalledWith('start_script', { id: 'a', path: 'scripts/a.lua' });
		expect(get(store)[0].state).toBe('running');
	});

	it('pause/resume/restart 传递 scriptId 并刷新列表', async () => {
		let state = 'running';
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'get_scripts') return [raw('a', state)];
			if (cmd === 'pause_script') state = 'paused';
			if (cmd === 'resume_script' || cmd === 'restart_script') state = 'running';
			return null;
		});

		await store.pause('a');
		expect(invokeMock).toHaveBeenCalledWith('pause_script', { scriptId: 'a' });
		expect(get(store)[0].state).toBe('paused');

		await store.resume('a');
		expect(invokeMock).toHaveBeenCalledWith('resume_script', { scriptId: 'a' });
		expect(get(store)[0].state).toBe('running');

		await store.restart('a');
		expect(invokeMock).toHaveBeenCalledWith('restart_script', { scriptId: 'a' });
		expect(get(store)[0].state).toBe('running');
	});

	it('stop 传递 scriptId 并刷新列表', async () => {
		let state = 'running';
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'get_scripts') return [raw('a', state)];
			if (cmd === 'stop_script') state = 'stopped';
			return null;
		});
		await store.stop('a');
		expect(invokeMock).toHaveBeenCalledWith('stop_script', { scriptId: 'a' });
		expect(get(store)[0]).toMatchObject({ state: 'stopped', is_running: false });
	});

	it('unload 传递 scriptId 并刷新列表', async () => {
		mockBackend(() => []);
		await store.unload('a');
		expect(invokeMock).toHaveBeenCalledWith('unload_script', { scriptId: 'a' });
		expect(get(store)).toHaveLength(0);
	});

	it('操作失败时向上抛出且不刷新列表', async () => {
		invokeMock.mockImplementation(async (cmd: string) => {
			if (cmd === 'pause_script') throw new Error('runtime error');
			return [raw('a', 'running')];
		});
		await expect(store.pause('a')).rejects.toThrow('runtime error');
		expect(invokeMock.mock.calls.filter(([cmd]) => cmd === 'get_scripts')).toHaveLength(0);
	});

	it('批量停止：对每个活跃脚本调用 stop_script 并重新拉取', async () => {
		const backend = [raw('r', 'running'), raw('p', 'paused'), raw('s', 'stopped')];
		mockBackend(() => backend);
		await store.load();

		const targets = get(store).filter(s => s.state === 'running' || s.state === 'paused');
		for (const s of targets) {
			/// 先模拟 runtime 侧状态变更，stop 内部的 load() 才能读到新状态
			const idx = backend.findIndex(x => x.id === s.id);
			backend[idx] = { ...backend[idx], state: 'stopped' };
			await store.stop(s.id);
		}

		const stopCalls = invokeMock.mock.calls.filter(([cmd]) => cmd === 'stop_script');
		expect(stopCalls).toHaveLength(2);
		expect(get(store).every(s => s.state === 'stopped')).toBe(true);
	});
});
