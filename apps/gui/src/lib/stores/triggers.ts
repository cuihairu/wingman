import { writable } from 'svelte/store';

export type TriggerConditionType =
	| 'color_found'
	| 'color_lost'
	| 'image_found'
	| 'image_lost'
	| 'pixel_changed'
	| 'window_opened'
	| 'window_closed'
	| 'process_started'
	| 'process_stopped'
	| 'time_elapsed'
	| 'hotkey_pressed';

export interface TriggerCondition {
	type: TriggerConditionType;
	value: string;
	region: { x: number; y: number; width: number; height: number };
	tolerance: number;
	interval: number;
}

export type TriggerActionType =
	| 'run_script'
	| 'stop_script'
	| 'pause_script'
	| 'click'
	| 'key_press'
	| 'type'
	| 'delay'
	| 'show_message'
	| 'play_audio'
	| 'log';

export interface TriggerAction {
	type: TriggerActionType;
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
	last_triggered_at?: number;
}

function normalizeConditionType(type: unknown): TriggerConditionType {
	const value = String(type || '').toLowerCase();
	if (value === 'colorfound' || value === 'color_found' || value === 'pixel') return 'color_found';
	if (value === 'colorlost' || value === 'color_lost') return 'color_lost';
	if (value === 'imagefound' || value === 'image_found' || value === 'image') return 'image_found';
	if (value === 'imagelost' || value === 'image_lost') return 'image_lost';
	if (value === 'pixelchanged' || value === 'pixel_changed') return 'pixel_changed';
	if (value === 'windowopened' || value === 'window_opened') return 'window_opened';
	if (value === 'windowclosed' || value === 'window_closed') return 'window_closed';
	if (value === 'processstarted' || value === 'process_started') return 'process_started';
	if (value === 'processstopped' || value === 'process_stopped') return 'process_stopped';
	if (value === 'timeelapsed' || value === 'time_elapsed') return 'time_elapsed';
	if (value === 'hotkeypressed' || value === 'hotkey_pressed') return 'hotkey_pressed';
	return 'color_found';
}

function normalizeActionType(type: unknown): TriggerActionType {
	if (typeof type === 'number') {
		if (type === 0) return 'run_script';
		if (type === 1) return 'click';
		if (type === 2) return 'key_press';
		if (type === 3) return 'type';
		if (type === 4) return 'stop_script';
		if (type === 5) return 'pause_script';
		if (type === 6) return 'show_message';
		if (type === 7) return 'play_audio';
		if (type === 9) return 'delay';
		return 'log';
	}
	const value = String(type || '').toLowerCase();
	if (value === 'runscript' || value === 'run_script' || value === 'macro') return 'run_script';
	if (value === 'stopscript' || value === 'stop_script') return 'stop_script';
	if (value === 'pausescript' || value === 'pause_script') return 'pause_script';
	if (value === 'click') return 'click';
	if (value === 'keypress' || value === 'key_press' || value === 'key') return 'key_press';
	if (value === 'type') return 'type';
	if (value === 'showmessage' || value === 'show_message') return 'show_message';
	if (value === 'playaudio' || value === 'play_audio') return 'play_audio';
	if (value === 'delay') return 'delay';
	return 'log';
}

// ---- GUI ⇄ runtime 值格式转换 ----
//
// runtime 端（win32_trigger.cpp / posix_trigger.cpp）的取值约定：
// - ColorFound/ColorLost: std::stoul(value, nullptr, 0)，需要 "0xFF0000" 或十进制
// - HotkeyPressed: std::stoi(value)，需要虚拟键码数字（单键）
// - PixelChanged: 监测点取 cond.region.x/y（不读 value）
// GUI 内部统一使用 "#rrggbb" 颜色、键名 "F9"、坐标串 "x,y"，在 IPC 边界转换。

function buildVkMap(): Record<string, number> {
	const map: Record<string, number> = {};
	for (let i = 1; i <= 12; i++) map[`F${i}`] = 0x70 + (i - 1);
	for (let i = 0; i <= 9; i++) map[String(i)] = 0x30 + i;
	for (let i = 0; i < 26; i++) map[String.fromCharCode(0x41 + i)] = 0x41 + i;
	map['SPACE'] = 0x20;
	map['ESC'] = 0x1b;
	map['TAB'] = 0x09;
	map['ENTER'] = 0x0d;
	return map;
}

const VK_MAP = buildVkMap();
const NAME_BY_VK: Record<number, string> = Object.fromEntries(
	Object.entries(VK_MAP).map(([name, code]) => [code, name])
);

/// "#ff0000" → "0xFF0000"；无法识别时原样返回（兼容已是 0x…/十进制的值）
export function hexColorToRuntime(value: string): string {
	const m = /^#?([0-9a-fA-F]{6})$/.exec(value.trim());
	if (m) return `0x${m[1].toUpperCase()}`;
	return value;
}

/// "0xFF0000" | "16711680" | "#ff0000" → "#ff0000"；无法识别时原样返回
function runtimeColorToHex(value: string): string {
	const v = String(value || '').trim();
	let m = /^#?([0-9a-fA-F]{6})$/.exec(v);
	if (m) return `#${m[1].toLowerCase()}`;
	m = /^0x([0-9a-fA-F]{1,6})$/.exec(v);
	if (m) return `#${parseInt(m[1], 16).toString(16).padStart(6, '0')}`;
	if (/^\d+$/.test(v)) {
		const num = parseInt(v, 10);
		if (num >= 0 && num <= 0xffffff) return `#${num.toString(16).padStart(6, '0')}`;
	}
	return v;
}

/// "F9" → "120"（VK 码十进制串）；无法识别时原样返回
function hotkeyToVk(value: string): string {
	const key = value.trim().toUpperCase();
	if (VK_MAP[key] !== undefined) return String(VK_MAP[key]);
	if (/^\d+$/.test(value.trim())) return value.trim(); // 已是 VK 码
	return value;
}

/// "120" → "F9"；无法识别时原样返回
export function vkToHotkey(value: string): string {
	const v = String(value || '').trim();
	if (/^\d+$/.test(v)) {
		const code = parseInt(v, 10);
		if (NAME_BY_VK[code]) return NAME_BY_VK[code];
	}
	return v;
}

function parsePoint(value: string): { x: number; y: number } {
	const [x, y] = String(value || '').split(',').map(v => parseInt(v.trim()));
	return { x: Number.isFinite(x) ? x : 0, y: Number.isFinite(y) ? y : 0 };
}

/// GUI 格式 → runtime IPC 格式（add/update 前调用）
export function serializeForRuntime(config: Partial<TriggerConfig>): any {
	const c = structuredClone(config) as any;
	const cond = c?.condition;
	if (!cond) return c;
	if (cond.type === 'color_found' || cond.type === 'color_lost') {
		cond.value = hexColorToRuntime(String(cond.value || ''));
	} else if (cond.type === 'hotkey_pressed') {
		cond.value = hotkeyToVk(String(cond.value || ''));
	} else if (cond.type === 'pixel_changed') {
		const { x, y } = parsePoint(String(cond.value || ''));
		cond.region = { ...(cond.region || {}), x, y };
	}
	return c;
}

function normalizeTrigger(input: any): TriggerConfig {
	const condition = input?.condition || {};
	const runtimeType = input?.trigger_type || input?.type || condition?.type;
	const type = normalizeConditionType(runtimeType);

	// GUI 显示格式：颜色用 #rrggbb，热键用键名，像素坐标用 "x,y"（取自 region）
	let displayValue = String(condition?.value || '');
	if (type === 'color_found' || type === 'color_lost') {
		displayValue = runtimeColorToHex(displayValue);
	} else if (type === 'hotkey_pressed') {
		displayValue = vkToHotkey(displayValue);
	} else if (type === 'pixel_changed') {
		const region = condition?.region;
		displayValue = region ? `${region.x ?? 0},${region.y ?? 0}` : displayValue;
	}

	return {
		id: String(input?.id || crypto.randomUUID()),
		name: String(input?.name || '未命名触发器'),
		enabled: input?.enabled ?? true,
		condition: {
			type,
			value: displayValue,
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
		last_triggered_at: input?.last_triggered_at ?? input?.lastTriggerTime ?? undefined,
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
				const id = await invoke('add_trigger', { config: serializeForRuntime(config) });
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
				await invoke('update_trigger', { id, config: serializeForRuntime(config) });
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
		/// 标记触发器最近命中（由 runtime trigger.fired 事件驱动）。
		/// 按 id 或 name 匹配；命中时置 last_triggered=true，并记录命中时间戳（毫秒）。
		markFired(id: string | number, name?: string, timestamp?: number) {
			const idStr = String(id ?? '');
			const nameStr = name ?? '';
			const ts = timestamp ?? Date.now();
			let matched = false;
			store.update(items => items.map(trigger => {
				const idMatch = idStr && String(trigger.id) === idStr;
				const nameMatch = !!nameStr && trigger.name === nameStr;
				if (idMatch || nameMatch) {
					matched = true;
					return { ...trigger, last_triggered: true, last_triggered_at: ts };
				}
				return trigger;
			}));
			return matched;
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
