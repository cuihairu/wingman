import { writable } from 'svelte/store';
import { logs } from './logs';

/// 自动更新状态与操作（封装 Tauri 命令 → runtime update RPC）。
///
/// 状态两个来源：
/// 1. 进入页面时 refresh()（get_update_status，读 poller 当前状态）；
/// 2. runtime 周期轮询 / 手动触发落地后经 events.drain 推来的
///    update.status_changed 事件 → applyStatus()（payload 即完整 UpdateStatus）。
///
/// update.check / update.download 都是异步触发：真正执行在 runtime 的 worker
/// 线程上，结果稍后经事件回投，本 store 不轮询等待。

export interface UpdateManifest {
	version: string;
	url: string;
	sha256: string;
	size: number;
	notes: string;
	mandatory: boolean;
	fileName: string;
}

export interface UpdateStatus {
	enabled: boolean;
	available: boolean;
	downloading: boolean;
	downloaded: boolean;
	localVersion: string;
	candidate: UpdateManifest;
	downloadPath: string;
	lastCheckMs: number;
	lastError: string;
}

function defaultManifest(): UpdateManifest {
	return { version: '', url: '', sha256: '', size: 0, notes: '', mandatory: false, fileName: '' };
}

function defaultStatus(): UpdateStatus {
	return {
		enabled: false,
		available: false,
		downloading: false,
		downloaded: false,
		localVersion: '',
		candidate: defaultManifest(),
		downloadPath: '',
		lastCheckMs: 0,
		lastError: '',
	};
}

/// 浅合并事件/接口回传的状态；null 或非对象载荷忽略。
function merge(payload: unknown): UpdateStatus | null {
	if (payload === null || typeof payload !== 'object') return null;
	return { ...defaultStatus(), ...(payload as Partial<UpdateStatus>) };
}

function createUpdatesStore() {
	const invoke = (window as any).__TAURI_INVOKE__;
	const store = writable<UpdateStatus>(defaultStatus());

	// 最近一次快照，用于事件回投时判断「状态是否切换」以避免刷屏日志
	let cached: UpdateStatus = defaultStatus();
	store.subscribe(v => { cached = v; });
	const storeValue = () => cached;

	const refresh = async () => {
		if (!invoke) return;
		try {
			const status = (await invoke('get_update_status')) as UpdateStatus;
			if (status) store.set(merge(status) ?? defaultStatus());
		} catch { /* ignore */ }
	};

	/** runtime → GUI 事件流（update.status_changed）回投：payload 即 UpdateStatus */
	const applyStatus = (payload: unknown) => {
		const merged = merge(payload);
		if (!merged) return;
		// 仅在状态切换的关键点上打日志，避免轮询周期的重复事件刷屏
		const prev = storeValue();
		if (merged.available && !prev.available) {
			logs.add(`发现新版本 ${merged.candidate.version}`, 'success');
		} else if (merged.downloading && !prev.downloading) {
			logs.add(`正在下载更新 ${merged.candidate.version}`, 'info');
		} else if (merged.downloaded && !prev.downloaded) {
			logs.add(`更新已就绪: ${merged.downloadPath}`, 'success');
		} else if (merged.lastError && merged.lastError !== prev.lastError) {
			logs.add(merged.lastError, 'error');
		}
		store.set(merged);
	};

	return {
		subscribe: store.subscribe,
		refresh,
		applyStatus,
		async check() {
			if (!invoke) return;
			try {
				logs.add('正在检查更新…', 'info');
				const result = (await invoke('check_update')) as { status?: UpdateStatus } | null;
				if (result?.status) store.set(merge(result.status) ?? defaultStatus());
			} catch (e: any) {
				logs.add(`检查更新失败: ${e}`, 'error');
			}
		},
		async download() {
			if (!invoke) return;
			try {
				logs.add(`开始下载更新 ${storeValue().candidate.version}`, 'info');
				const result = (await invoke('download_update')) as { status?: UpdateStatus } | null;
				if (result?.status) store.set(merge(result.status) ?? defaultStatus());
			} catch (e: any) {
				logs.add(`下载更新失败: ${e}`, 'error');
			}
		},
	};
}

export const updates = createUpdatesStore();
