import { writable } from 'svelte/store';

export interface ScriptInfo {
	id: string;
	name: string;
	path: string;
	size: number;
	is_running: boolean;
}

function createScriptsStore() {
	const store = writable<ScriptInfo[]>([]);
	const invoke = (window as any).__TAURI_INVOKE__;

	return {
		subscribe: store.subscribe,
		set(scripts: ScriptInfo[]) {
			store.set(scripts);
		},
		async load() {
			if (!invoke) return;
			try {
				const scripts = await invoke('get_scripts');
				store.set(scripts);
			} catch { /* ignore */ }
		},
		async start(id: string) {
			if (!invoke) return;
			await invoke('start_script', { id });
			await this.load();
		},
		async stop(scriptId: string) {
			if (!invoke) return;
			await invoke('stop_script', { scriptId });
			await this.load();
		},
		loadDevData() {
			store.set([
				{ id: 'example', name: 'example.lua', path: 'scripts/example.lua', size: 1024, is_running: true },
				{ id: 'test', name: 'test.lua', path: 'scripts/test.lua', size: 512, is_running: false },
			]);
		},
	};
}

export const scripts = createScriptsStore();
