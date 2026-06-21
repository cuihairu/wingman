import { writable } from 'svelte/store';

export interface LogEntry {
	time: string;
	message: string;
	type: 'info' | 'success' | 'warning' | 'error';
}

/// 缓冲区上限，防止运行时日志高频下发时无限增长。
const MAX_LOG_ENTRIES = 1000;

function createLogsStore() {
	const store = writable<LogEntry[]>([]);

	function getCurrentTime(): string {
		const now = new Date();
		return `[${now.toTimeString().split(' ')[0]}]`;
	}

	function formatTimestamp(ts?: number): string {
		if (!ts) return getCurrentTime();
		const d = new Date(ts);
		if (Number.isNaN(d.getTime())) return getCurrentTime();
		return `[${d.toTimeString().split(' ')[0]}]`;
	}

	function append(entry: LogEntry) {
		store.update(logs => {
			const next = logs.length >= MAX_LOG_ENTRIES
				? logs.slice(logs.length - MAX_LOG_ENTRIES + 1)
				: logs.slice();
			next.push(entry);
			return next;
		});
	}

	return {
		subscribe: store.subscribe,
		add(message: string, type: LogEntry['type'] = 'info') {
			append({ time: getCurrentTime(), message, type });
		},
		/// 接收 runtime 推送的日志事件（来自 events.drain）。
		addRuntime(message: string, type: LogEntry['type'], ts?: number) {
			append({ time: formatTimestamp(ts), message, type });
		},
		clear() {
			store.set([]);
		},
	};
}

export const logs = createLogsStore();
