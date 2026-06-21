import { writable } from 'svelte/store';
import { logs } from './logs';

/// 宏录制状态与操作（封装 Tauri 命令 → runtime macro RPC）。
///
/// 录制流程：record（装钩子+消息泵线程捕获输入）→ stop（返回事件数）→
/// 可 save（JSON）/ load / play（回放，含事件间 sleep）。

export interface MacroStatus {
	recording: boolean;
	paused: boolean;
	eventCount: number;
}

function createMacrosStore() {
	const invoke = (window as any).__TAURI_INVOKE__;
	const store = writable<MacroStatus>({ recording: false, paused: false, eventCount: 0 });

	const refresh = async () => {
		if (!invoke) return;
		try {
			const status = (await invoke('macro_status')) as MacroStatus;
			if (status) store.set(status);
		} catch { /* ignore */ }
	};

	return {
		subscribe: store.subscribe,
		refresh,
		async record() {
			if (!invoke) return;
			try {
				await invoke('macro_record');
				store.update(s => ({ ...s, recording: true }));
				logs.add('开始录制宏', 'info');
			} catch (e: any) {
				logs.add(`录制启动失败: ${e}`, 'error');
			}
		},
		async stop() {
			if (!invoke) return 0;
			try {
				const result = (await invoke('macro_stop')) as { eventCount?: number };
				const count = result?.eventCount ?? 0;
				store.update(s => ({ ...s, recording: false, eventCount: count }));
				logs.add(`停止录制，捕获 ${count} 个事件`, 'success');
				return count;
			} catch (e: any) {
				logs.add(`停止录制失败: ${e}`, 'error');
				return 0;
			}
		},
		async play(speed = 100, repeat = 1) {
			if (!invoke) return;
			try {
				logs.add(`回放宏（速度 ${speed}%，重复 ${repeat}）`, 'info');
				await invoke('macro_play', { speed, repeat });
				logs.add('宏回放完成', 'success');
			} catch (e: any) {
				logs.add(`宏回放失败: ${e}`, 'error');
			}
		},
		async save(path: string) {
			if (!invoke) return false;
			try {
				await invoke('macro_save', { path });
				logs.add(`宏已保存: ${path}`, 'success');
				return true;
			} catch (e: any) {
				logs.add(`保存宏失败: ${e}`, 'error');
				return false;
			}
		},
		async load(path: string) {
			if (!invoke) return false;
			try {
				const result = (await invoke('macro_load', { path })) as { eventCount?: number };
				store.update(s => ({ ...s, eventCount: result?.eventCount ?? 0 }));
				logs.add(`宏已载入: ${path}`, 'success');
				return true;
			} catch (e: any) {
				logs.add(`载入宏失败: ${e}`, 'error');
				return false;
			}
		},
		async clear() {
			if (!invoke) return;
			try {
				await invoke('macro_clear');
				store.update(s => ({ ...s, eventCount: 0 }));
				logs.add('已清空录制', 'info');
			} catch (e: any) {
				logs.add(`清空失败: ${e}`, 'error');
			}
		},
	};
}

export const macros = createMacrosStore();
