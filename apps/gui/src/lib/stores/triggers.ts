import { writable } from 'svelte/store';

export interface TriggerCondition {
	type: 'color_found' | 'color_lost' | 'image_found' | 'image_lost' | 'time_elapsed' | 'hotkey_pressed';
	value: string;
	region?: { x: number; y: number; width: number; height: number };
	tolerance?: number;
	interval?: number;
}

export interface TriggerAction {
	type: 'run_script' | 'click' | 'key_press' | 'type' | 'delay' | 'log';
	value?: string;
	x?: number;
	y?: number;
	delay?: number;
}

export interface TriggerConfig {
	id: string;
	name: string;
	enabled: boolean;
	condition: TriggerCondition;
	actions: TriggerAction[];
	oneShot?: boolean;
	cooldown?: number;
}

function createTriggersStore() {
	const store = writable<TriggerConfig[]>([]);
	const invoke = (window as any).__TAURI_INVOKE__;

	return {
		subscribe: store.subscribe,
		set(triggers: TriggerConfig[]) {
			store.set(triggers);
		},
		async load() {
			if (!invoke) return;
			try {
				const result = await invoke('get_triggers');
				store.set(result || []);
			} catch { /* ignore */ }
		},
		async add(config: Partial<TriggerConfig>) {
			if (!invoke) return '';
			try {
				const id = await invoke('add_trigger', { config });
				await this.load();
				return id;
			} catch { return ''; }
		},
		async remove(id: string) {
			if (!invoke) return;
			try {
				await invoke('remove_trigger', { id });
				await this.load();
			} catch { /* ignore */ }
		},
		async update(id: string, config: Partial<TriggerConfig>) {
			if (!invoke) return;
			try {
				await invoke('update_trigger', { id, config });
				await this.load();
			} catch { /* ignore */ }
		},
		async toggle(id: string) {
			if (!invoke) return false;
			try {
				const enabled = await invoke('toggle_trigger', { id });
				await this.load();
				return enabled;
			} catch { return false; }
		},
		loadDevData() {
			store.set([
				{
					id: '1',
					name: '像素检测-HP药水',
					enabled: true,
					condition: { type: 'color_found', value: '#ff0000', tolerance: 10, interval: 500 },
					actions: [{ type: 'click', x: 100, y: 200 }],
				},
				{
					id: '2',
					name: '定时器-每5秒',
					enabled: false,
					condition: { type: 'time_elapsed', value: '5000' },
					actions: [{ type: 'log', value: 'Timer tick' }],
				},
			]);
		},
	};
}

export const triggers = createTriggersStore();
