import { writable } from 'svelte/store';

export type ScriptState = 'loaded' | 'running' | 'paused' | 'stopped' | 'error' | 'unknown';

export interface ScriptInfo {
	id: string;
	name: string;
	path: string;
	size: number;
	is_running: boolean;
	/** runtime 脚本状态（is_running 由此派生） */
	state: ScriptState;
	error: string;
	/** 最近一次加载时间（epoch 毫秒） */
	loaded_at: number;
}

function normalizeScript(input: any): ScriptInfo {
	const state = String(input?.state || '').toLowerCase() as ScriptState;
	const known: ScriptState[] = ['loaded', 'running', 'paused', 'stopped', 'error'];
	const safeState: ScriptState = known.includes(state) ? state : 'unknown';
	return {
		id: String(input?.id || ''),
		name: String(input?.name || input?.path || ''),
		path: String(input?.path || ''),
		size: Number(input?.size || 0),
		is_running: safeState === 'running',
		state: safeState,
		error: String(input?.error || ''),
		loaded_at: Number(input?.loaded_at || input?.loadedAt || 0),
	};
}

function createScriptsStore() {
	const store = writable<ScriptInfo[]>([]);
	const invoke = (window as any).__TAURI_INVOKE__;

	/// dev 模式下按状态切换的本地模拟
	function devSetState(id: string, state: ScriptState) {
		store.update(items => items.map(script => (
			script.id === id
				? { ...script, state, is_running: state === 'running' }
				: script
		)));
	}

	return {
		subscribe: store.subscribe,
		set(scripts: ScriptInfo[]) {
			store.set(scripts.map(normalizeScript));
		},
		async load() {
			if (!invoke) return;
			try {
				const scripts = await invoke('get_scripts');
				store.set((scripts || []).map(normalizeScript));
			} catch { /* ignore */ }
		},
		async start(id: string, path?: string) {
			if (!invoke) {
				devSetState(id, 'running');
				return;
			}
			await invoke('start_script', { id, path });
			await this.load();
		},
		async stop(scriptId: string) {
			if (!invoke) {
				devSetState(scriptId, 'stopped');
				return;
			}
			await invoke('stop_script', { scriptId });
			await this.load();
		},
		async pause(scriptId: string) {
			if (!invoke) {
				devSetState(scriptId, 'paused');
				return;
			}
			await invoke('pause_script', { scriptId });
			await this.load();
		},
		async resume(scriptId: string) {
			if (!invoke) {
				devSetState(scriptId, 'running');
				return;
			}
			await invoke('resume_script', { scriptId });
			await this.load();
		},
		async restart(scriptId: string) {
			if (!invoke) {
				devSetState(scriptId, 'running');
				return;
			}
			await invoke('restart_script', { scriptId });
			await this.load();
		},
		async unload(scriptId: string) {
			if (!invoke) {
				store.update(items => items.filter(script => script.id !== scriptId));
				return;
			}
			await invoke('unload_script', { scriptId });
			await this.load();
		},
		/// 由 runtime script.state_changed 事件驱动：低延迟更新单脚本状态。
		/// 匹配 id 失败时（事件先于列表加载）忽略，下次 load 对齐。
		applyState(id: string, state: string, error?: string) {
			const normalized = String(state || '').toLowerCase() as ScriptState;
			const known: ScriptState[] = ['loaded', 'running', 'paused', 'stopped', 'error'];
			if (!known.includes(normalized)) return false;
			let matched = false;
			store.update(items => items.map(script => {
				if (script.id !== id) return script;
				matched = true;
				return {
					...script,
					state: normalized,
					is_running: normalized === 'running',
					error: error !== undefined ? error : script.error,
				};
			}));
			return matched;
		},
		loadDevData() {
			store.set([
				{ id: 'example', name: 'example.lua', path: 'scripts/example.lua', size: 1024, is_running: true, state: 'running', error: '', loaded_at: Date.now() - 83_000 },
				{ id: 'test', name: 'test.lua', path: 'scripts/test.lua', size: 512, is_running: false, state: 'stopped', error: '', loaded_at: Date.now() - 3_600_000 },
			]);
		},
	};
}

export const scripts = createScriptsStore();
