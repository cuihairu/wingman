import { writable } from 'svelte/store';

interface Settings {
	wsUrl: string;
	autoReconnect: boolean;
	minimizeOnStart: boolean;
	theme: 'dark';
}

function loadSettings(): Settings {
	try {
		const saved = localStorage.getItem('wingman-settings');
		if (saved) return JSON.parse(saved);
	} catch { /* ignore */ }
	return {
		wsUrl: 'ws://127.0.0.1:8080/ws',
		autoReconnect: true,
		minimizeOnStart: false,
		theme: 'dark',
	};
}

function createSettingsStore() {
	const store = writable<Settings>(loadSettings());

	store.subscribe(value => {
		try {
			localStorage.setItem('wingman-settings', JSON.stringify(value));
		} catch { /* ignore */ }
	});

	return {
		subscribe: store.subscribe,
		update(partial: Partial<Settings>) {
			store.update(s => ({ ...s, ...partial }));
		},
	};
}

export const settings = createSettingsStore();
