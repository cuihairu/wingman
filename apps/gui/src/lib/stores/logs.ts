import { writable } from 'svelte/store';

export interface LogEntry {
	time: string;
	message: string;
	type: 'info' | 'success' | 'warning' | 'error';
}

function createLogsStore() {
	const store = writable<LogEntry[]>([]);

	function getCurrentTime(): string {
		const now = new Date();
		return `[${now.toTimeString().split(' ')[0]}]`;
	}

	return {
		subscribe: store.subscribe,
		add(message: string, type: LogEntry['type'] = 'info') {
			store.update(logs => [...logs, { time: getCurrentTime(), message, type }]);
		},
		clear() {
			store.set([]);
		},
	};
}

export const logs = createLogsStore();
