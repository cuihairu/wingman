import { writable } from 'svelte/store';

interface Settings {
	ipcEndpoint: string;
	orchestratorUrl: string;
	autoReconnect: boolean;
	autoStart: boolean;
	minimizeOnStart: boolean;
	logLevel: 'debug' | 'info' | 'warn' | 'error';
	theme: 'dark';
}

const defaultSettings: Settings = {
	ipcEndpoint: 'wingman',
	orchestratorUrl: 'http://localhost:9527',
	autoReconnect: true,
	autoStart: false,
	minimizeOnStart: false,
	logLevel: 'info',
	theme: 'dark',
};

function loadSettings(): Settings {
	try {
		const saved = localStorage.getItem('wingman-settings');
		if (saved) return { ...defaultSettings, ...JSON.parse(saved) };
	} catch { /* ignore */ }
	return defaultSettings;
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
