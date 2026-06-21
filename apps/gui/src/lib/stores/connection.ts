import { writable, derived } from 'svelte/store';

interface RemoteLinkState {
	state: 'connected' | 'connecting' | 'disconnected' | 'reconnecting' | 'error';
	message: string;
}

interface ConnectionState {
	connected: boolean;
	version: string;
	paused: boolean;
	ipcEndpoint: string;
	/** runtime → Go server 远程链路状态（由 connection.state_changed 事件驱动） */
	remote: RemoteLinkState | null;
}

export interface SystemStatus {
	server: string;
	version: string;
	uptime: number;
	running_scripts: number;
	paused: boolean;
}

function createConnectionStore() {
	const store = writable<ConnectionState>({
		connected: false,
		version: '-',
		paused: false,
		ipcEndpoint: 'wingman',
		remote: null,
	});
	let currentState: ConnectionState = {
		connected: false,
		version: '-',
		paused: false,
		ipcEndpoint: 'wingman',
		remote: null,
	};

	store.subscribe(value => {
		currentState = value;
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
		setIpcEndpoint(endpoint: string) {
			store.update(s => ({ ...s, ipcEndpoint: endpoint }));
		},
		setRemoteState(state: RemoteLinkState['state'], message: string) {
			store.update(s => ({ ...s, remote: { state, message } }));
		},
		async connect(endpoint?: string) {
			if (!invoke) {
				store.update(s => ({ ...s, connected: true, version: 'wingman 0.1.0 (dev)' }));
				return;
			}
			const ipcEndpoint = endpoint || 'wingman';
			await invoke('connect_ipc', { endpoint: ipcEndpoint });
			const status = await invoke('get_system_status') as SystemStatus;
			store.update(s => ({
				...s,
				connected: true,
				ipcEndpoint,
				version: status.version || s.version,
				paused: status.paused,
			}));
		},
		async disconnect() {
			if (!invoke) {
				store.update(s => ({ ...s, connected: false }));
				return;
			}
			await invoke('disconnect_ipc');
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
		async stopAll() {
			if (!invoke) return 0;
			const stopped = await invoke('stop_all');
			store.update(s => ({ ...s, paused: false }));
			return Number(stopped || 0);
		},
		async startActiveProfile() {
			if (!invoke) return 0;
			const started = await invoke('start_active_profile_scripts');
			store.update(s => ({ ...s, paused: false }));
			return Array.isArray(started) ? started.length : 0;
		},
		async stopActiveProfile() {
			if (!invoke) return 0;
			const stopped = await invoke('stop_active_profile_scripts');
			store.update(s => ({ ...s, paused: false }));
			return Number(stopped || 0);
		},
		async refresh(): Promise<SystemStatus | null> {
			if (!invoke) {
				return {
					server: 'wingman',
					version: 'wingman 0.1.0 (dev)',
					uptime: 0,
					running_scripts: 0,
					paused: currentState.paused,
				};
			}
			try {
				const status = await invoke('get_system_status') as SystemStatus;
				store.update(s => ({
					...s,
					version: status.version || s.version,
					paused: status.paused,
				}));
				return status;
			} catch {
				store.update(s => ({ ...s, connected: false }));
				return null;
			}
		},
	};
}

export const connection = createConnectionStore();
