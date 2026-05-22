import { writable, derived } from 'svelte/store';

interface ConnectionState {
	connected: boolean;
	version: string;
	paused: boolean;
	wsUrl: string;
}

function createConnectionStore() {
	const store = writable<ConnectionState>({
		connected: false,
		version: '-',
		paused: false,
		wsUrl: 'ws://127.0.0.1:8080/ws',
	});

	const invoke = (window as any).__TAURI_INVOKE__;

	return {
		subscribe: store.subscribe,
		get connected() {
			let val: boolean = false;
			store.update(s => { val = s.connected; return s; });
			return val;
		},
		setConnected(connected: boolean) {
			store.update(s => ({ ...s, connected }));
		},
		setVersion(version: string) {
			store.update(s => ({ ...s, version }));
		},
		setPaused(paused: boolean) {
			store.update(s => ({ ...s, paused }));
		},
		setWsUrl(url: string) {
			store.update(s => ({ ...s, wsUrl: url }));
		},
		async connect(url?: string) {
			if (!invoke) {
				store.update(s => ({ ...s, connected: true, version: 'wingman 0.1.0 (dev)' }));
				return;
			}
			const wsUrl = url || 'ws://127.0.0.1:8080/ws';
			await invoke('connect_websocket', { url: wsUrl });
			store.update(s => ({ ...s, connected: true, wsUrl }));
		},
		async disconnect() {
			if (!invoke) {
				store.update(s => ({ ...s, connected: false }));
				return;
			}
			await invoke('disconnect_websocket');
			store.update(s => ({ ...s, connected: false }));
		},
		async togglePause() {
			if (!invoke) {
				store.update(s => ({ ...s, paused: !s.paused }));
				return;
			}
			const paused = await invoke('toggle_pause');
			store.update(s => ({ ...s, paused }));
		},
		async refresh() {
			if (!invoke) return;
			try {
				const status = await invoke('get_system_status');
				store.update(s => ({
					...s,
					paused: status.paused,
				}));
			} catch { /* ignore */ }
		},
	};
}

export const connection = createConnectionStore();
