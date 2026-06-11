import { writable, derived } from 'svelte/store';

export interface GameWindowConfig {
	title: string;
	className: string;
	processName: string;
	exactMatch: boolean;
	fullscreen: boolean;
}

export interface ColorConfig {
	name: string;
	r: number;
	g: number;
	b: number;
	tolerance: number;
}

export interface ImageConfig {
	name: string;
	path: string;
	threshold: number;
	preload: boolean;
}

export interface GameTriggerConfig {
	name: string;
	type: string;
	action: string;
	target: string;
	interval: number;
	enabled: boolean;
}

export interface GameScriptConfig {
	name: string;
	path: string;
	autoStart: boolean;
	restartOnCrash: boolean;
	priority: number;
}

export interface HotkeyConfig {
	start: string[];
	stop: string[];
	pause: string[];
	emergencyStop: string[];
}

export interface GameProfile {
	id: string;
	name: string;
	version: string;
	description: string;
	window: GameWindowConfig;
	colors: ColorConfig[];
	images: ImageConfig[];
	triggers: GameTriggerConfig[];
	scripts: GameScriptConfig[];
	hotkeys: HotkeyConfig;
	settings: Record<string, string>;
}

function createProfilesStore() {
	const store = writable<GameProfile[]>([]);
	const activeId = writable<string>('');
	const invoke = (window as any).__TAURI_INVOKE__;
	let currentProfiles: GameProfile[] = [];
	let currentActiveId = '';

	store.subscribe(value => {
		currentProfiles = value;
	});
	activeId.subscribe(value => {
		currentActiveId = value;
	});

	const withDefaultHotkeys = (profile: any): GameProfile => ({
		...profile,
		hotkeys: {
			start: profile?.hotkeys?.start ?? ['F5'],
			stop: profile?.hotkeys?.stop ?? ['F6'],
			pause: profile?.hotkeys?.pause ?? ['F7'],
			emergencyStop: profile?.hotkeys?.emergencyStop ?? ['F12'],
		},
	});

	return {
		subscribe: store.subscribe,
		activeId,
		async load() {
			if (!invoke) return;
			try {
				const result = await invoke('get_profiles');
				store.set(((result || []) as any[]).map(withDefaultHotkeys));
			} catch { /* ignore */ }
		},
		async loadActive() {
			if (!invoke) return;
			try {
				const result = await invoke('get_active_profile');
				if (result?.id) activeId.set(result.id);
			} catch { /* ignore */ }
		},
		async setActive(id: string) {
			if (!invoke) {
				activeId.set(id);
				return;
			}
			try {
				await invoke('set_active_profile', { id });
				activeId.set(id);
			} catch { /* ignore */ }
		},
		async create(name: string) {
			if (!invoke) {
				const id = `profile_${Date.now()}`;
				const profile = withDefaultHotkeys({
					id,
					name,
					version: '1.0',
					description: '',
					window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
					colors: [],
					images: [],
					triggers: [],
					scripts: [],
					settings: {},
				});
				store.update(items => [...items, profile]);
				if (!currentActiveId) activeId.set(id);
				return id;
			}
			try {
				const id = await invoke('create_profile', { name });
				await this.load();
				return id;
			} catch { return ''; }
		},
		async remove(id: string) {
			if (!invoke) {
				store.update(items => items.filter(profile => profile.id !== id));
				if (currentActiveId === id) {
					activeId.set(currentProfiles.find(profile => profile.id !== id)?.id || '');
				}
				return;
			}
			try {
				await invoke('delete_profile', { id });
				await this.load();
			} catch { /* ignore */ }
		},
		async update(profile: GameProfile) {
			if (!invoke) {
				const normalized = withDefaultHotkeys(profile);
				store.update(items => items.map(item => item.id === normalized.id ? normalized : item));
				return;
			}
			try {
				await invoke('update_profile', { profile });
				await this.load();
			} catch { /* ignore */ }
		},
		async exportToJson(id: string) {
			if (!invoke) {
				const profile = currentProfiles.find(item => item.id === id);
				return profile ? JSON.stringify(profile, null, 2) : '';
			}
			try {
				return await invoke('export_profile_json', { id });
			} catch { return ''; }
		},
		async importFromJson(json: string) {
			if (!invoke) {
				try {
					const profile = withDefaultHotkeys(JSON.parse(json));
					store.update(items => {
						const exists = items.some(item => item.id === profile.id);
						return exists
							? items.map(item => item.id === profile.id ? profile : item)
							: [...items, profile];
					});
					return true;
				} catch {
					return false;
				}
			}
			try {
				await invoke('import_profile_json', { json });
				await this.load();
				return true;
			} catch { return false; }
		},
		loadDevData() {
			store.set([
				{
					id: 'default',
					name: '默认配置',
					version: '1.0',
					description: '默认游戏配置',
					window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
					colors: [],
					images: [],
					triggers: [],
					scripts: [],
					hotkeys: {
						start: ['F5'],
						stop: ['F6'],
						pause: ['F7'],
						emergencyStop: ['F12'],
					},
					settings: {},
				},
				{
					id: 'game1',
					name: '游戏配置 1',
					version: '1.0',
					description: '示例游戏配置',
					window: { title: 'MyGame', className: '', processName: 'mygame.exe', exactMatch: false, fullscreen: true },
					colors: [{ name: 'HP', r: 255, g: 0, b: 0, tolerance: 10 }],
					images: [],
					triggers: [],
					scripts: [],
					hotkeys: {
						start: ['F5'],
						stop: ['F6'],
						pause: ['F7'],
						emergencyStop: ['F12'],
					},
					settings: {},
				},
			]);
			activeId.set('default');
		},
	};
}

export const profiles = createProfilesStore();

export const activeProfile = derived(
	[profiles, profiles.activeId],
	([$profiles, $activeId]) => $profiles.find(p => p.id === $activeId) || null
);
