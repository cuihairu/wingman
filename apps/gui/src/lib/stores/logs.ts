import { writable, get } from 'svelte/store';
import { settings } from './settings';

export type LogLevel = 'debug' | 'info' | 'success' | 'warning' | 'error';

export type LogSource = 'gui' | 'runtime';

export interface LogEntry {
	time: string;
	message: string;
	type: LogLevel;
	/** 日志来源：GUI 本地操作 / runtime 事件流 */
	source: LogSource;
}

/// 缓冲区上限，防止运行时日志高频下发时无限增长。
const MAX_LOG_ENTRIES = 1000;

/// 级别权重（用于按 settings.logLevel 过滤 runtime 日志）
const LEVEL_WEIGHT: Record<LogLevel, number> = {
	debug: 0,
	info: 1,
	success: 1,
	warning: 2,
	error: 3,
};

const SETTINGS_LEVEL: Record<string, LogLevel> = {
	debug: 'debug',
	info: 'info',
	warn: 'warning',
	error: 'error',
};

function createLogsStore() {
	const store = writable<LogEntry[]>([]);
	/// runtime 有界缓冲累计丢弃的事件数（EventBuffer 溢出时递增）
	let droppedCount = 0;
	const dropped = writable(0);

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

	/// runtime 日志是否达到 GUI 设定的显示级别
	function passesLevelFilter(level: LogLevel): boolean {
		const configured = SETTINGS_LEVEL[get(settings).logLevel] ?? 'info';
		return LEVEL_WEIGHT[level] >= LEVEL_WEIGHT[configured];
	}

	return {
		subscribe: store.subscribe,
		dropped: { subscribe: dropped.subscribe },
		add(message: string, type: LogLevel = 'info') {
			append({ time: getCurrentTime(), message, type, source: 'gui' });
		},
		/// 接收 runtime 推送的日志事件（来自 events.drain）。
		/// 低于 settings.logLevel 的条目在入口处丢弃。
		addRuntime(message: string, type: LogLevel = 'info', ts?: number) {
			if (!passesLevelFilter(type)) return;
			append({ time: formatTimestamp(ts), message, type, source: 'runtime' });
		},
		/// runtime EventBuffer 溢出丢弃计数（累计值，只增不减）
		notifyDropped(total: number) {
			if (typeof total === 'number' && total > droppedCount) {
				droppedCount = total;
				dropped.set(total);
			}
		},
		clear() {
			store.set([]);
			droppedCount = 0;
			dropped.set(0);
		},
	};
}

export const logs = createLogsStore();
