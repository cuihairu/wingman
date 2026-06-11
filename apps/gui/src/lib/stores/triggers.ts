import { writable } from 'svelte/store';

export interface TriggerCondition {
	type: 'color_found' | 'color_lost' | 'image_found' | 'image_lost' | 'time_elapsed' | 'hotkey_pressed';
	value: string;
	region: { x: number; y: number; width: number; height: number };
	tolerance: number;
	interval: number;
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
	trigger_type?: string;
	last_triggered?: boolean;
}

function normalizeConditionType(type: unknown): TriggerCondition['type'] {
	const value = String(type || '').toLowerCase();
	if (value === 'colorfound' || value === 'color_found' || value === 'pixel') return 'color_found';
	if (value === 'colorlost' || value === 'color_lost') return 'color_lost';
	if (value === 'imagefound' || value === 'image_found' || value === 'image') return 'image_found';
	if (value === 'imagelost' || value === 'image_lost') return 'image_lost';
	if (value === 'timeelapsed' || value === 'time_elapsed') return 'time_elapsed';
	if (value === 'hotkeypressed' || value === 'hotkey_pressed') return 'hotkey_pressed';
	return 'color_found';
}

function normalizeActionType(type: unknown): TriggerAction['type'] {
	if (typeof type === 'number') {
		if (type === 0) return 'run_script';
		if (type === 1) return 'click';
		if (type === 2) return 'key_press';
		if (type === 3) return 'type';
		if (type === 9) return 'delay';
		return 'log';
	}
	const value = String(type || '').toLowerCase();
	if (value === 'runscript' || value === 'run_script' || value === 'macro') return 'run_script';
	if (value === 'click') return 'click';
	if (value === 'keypress' || value === 'key_press' || value === 'key') return 'key_press';
	if (value === 'type') return 'type';
	if (value === 'delay') return 'delay';
	return 'log';
}

function normalizeTrigger(input: any): TriggerConfig {
	const condition = input?.condition || {};
	const runtimeType = input?.trigger_type || input?.type || condition?.type;

	return {
		id: String(input?.id || crypto.randomUUID()),
		name: String(input?.name || '未命名触发器'),
		enabled: input?.enabled ?? true,
		condition: {
			type: normalizeConditionType(runtimeType),
			value: String(condition?.value || ''),
			region: condition?.region ?? { x: 0, y: 0, width: 0, height: 0 },
			tolerance: condition?.tolerance ?? 10,
			interval: condition?.interval ?? 1000,
		},
		actions: Array.isArray(input?.actions)
			? input.actions.map((action: any) => ({
				type: normalizeActionType(action?.type),
				value: action?.value,
				x: action?.x,
				y: action?.y,
				delay: action?.delay,
			}))
			: [],
		oneShot: input?.oneShot ?? input?.one_shot ?? false,
		cooldown: input?.cooldown ?? 0,
		trigger_type: input?.trigger_type || input?.type,
		last_triggered: input?.last_triggered ?? input?.lastTriggered ?? false,
	};
}

function createTriggersStore() {
	const store = writable<TriggerConfig[]>([]);
	const invoke = (window as any).__TAURI_INVOKE__;

	return {
		subscribe: store.subscribe,
		set(triggers: TriggerConfig[]) {
			store.set(triggers.map(normalizeTrigger));
		},
		async load() {
			if (!invoke) return;
			try {
				const result = await invoke('get_triggers');
				store.set((result || []).map(normalizeTrigger));
			} catch { /* ignore */ }
		},
		async add(config: Partial<TriggerConfig>) {
			if (!invoke) {
				const trigger = normalizeTrigger({ ...config, id: config.id || Date.now().toString() });
				store.update(items => [...items, trigger]);
				return trigger.id;
			}
			try {
				const id = await invoke('add_trigger', { config });
				await this.load();
				return id;
			} catch { return ''; }
		},
		async remove(id: string) {
			if (!invoke) {
				store.update(items => items.filter(trigger => trigger.id !== id));
				return;
			}
			try {
				await invoke('remove_trigger', { id });
				await this.load();
			} catch { /* ignore */ }
		},
		async update(id: string, config: Partial<TriggerConfig>) {
			if (!invoke) {
				store.update(items => items.map(trigger => (
					trigger.id === id ? normalizeTrigger({ ...trigger, ...config }) : trigger
				)));
				return;
			}
			try {
				await invoke('update_trigger', { id, config });
				await this.load();
			} catch { /* ignore */ }
		},
		async toggle(id: string) {
			if (!invoke) {
				let enabled = false;
				store.update(items => items.map(trigger => {
					if (trigger.id !== id) return trigger;
					enabled = !trigger.enabled;
					return { ...trigger, enabled };
				}));
				return enabled;
			}
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
					condition: { type: 'color_found', value: '#ff0000', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 500 },
					actions: [{ type: 'click', x: 100, y: 200 }],
				},
				{
					id: '2',
					name: '定时器-每5秒',
					enabled: false,
					condition: { type: 'time_elapsed', value: '5000', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 5000 },
					actions: [{ type: 'log', value: 'Timer tick' }],
				},
			]);
		},
	};
}

export const triggers = createTriggersStore();
