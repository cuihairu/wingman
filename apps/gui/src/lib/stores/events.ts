import { logs, type LogEntry } from './logs';
import { triggers } from './triggers';
import { connection } from './connection';
import { scripts } from './scripts';

/// runtime 事件轮询器。
///
/// 设计说明：runtime 通过 RPC `events.drain` 暴露缓冲事件（log.line / trigger.fired /
/// screenshot.frame）。GUI 在此轮询拉取并分发到对应 store。
///
/// 采用 pull 而非 push 模型，避免 Rust IPC 客户端为 Windows 阻塞 IO 引入异步读取循环
/// 造成请求/响应帧错位（参见 docs/architecture-decisions.md 的 IPC 边界约束）。
const POLL_INTERVAL_MS = 500;

type RuntimeEvent = {
	method: string;
	payload: Record<string, any>;
	timestamp: number;
};

function mapLevel(level: string | undefined): LogEntry['type'] {
	switch ((level || '').toLowerCase()) {
		case 'trace':
		case 'debug':
			return 'debug';
		case 'err':
		case 'error':
		case 'critical':
			return 'error';
		case 'warn':
		case 'warning':
			return 'warning';
		case 'info':
			return 'info';
		case 'success':
			return 'success';
		default:
			return 'info';
	}
}

function dispatch(event: RuntimeEvent) {
	switch (event.method) {
		case 'log.line': {
			const message = event.payload?.message;
			if (typeof message === 'string') {
				logs.addRuntime(message, mapLevel(event.payload?.level), event.timestamp);
			}
			break;
		}
		case 'trigger.fired': {
			const id = event.payload?.id;
			const name = event.payload?.name;
			triggers.markFired(id, name, event.timestamp);
			if (typeof name === 'string') {
				logs.addRuntime(`触发器命中: ${name}`, 'success', event.timestamp);
			}
			break;
		}
		case 'script.state_changed': {
			const id = event.payload?.id;
			const state = event.payload?.state;
			if (typeof id === 'string' && typeof state === 'string') {
				// 实时联动脚本状态（低延迟，无需等下一次 load）
				const error = typeof event.payload?.error === 'string' ? event.payload.error : undefined;
				scripts.applyState(id, state, error);
				logs.addRuntime(`脚本状态变更: ${id} → ${state}`, 'info', event.timestamp);
			}
			break;
		}
		case 'script.output': {
			const id = event.payload?.id;
			const output = event.payload?.output;
			if (typeof output === 'string' && output.length > 0) {
				const prefix = typeof id === 'string' && id ? `[${id}] ` : '';
				logs.addRuntime(`${prefix}${output}`, 'info', event.timestamp);
			}
			break;
		}
		case 'connection.state_changed': {
			const state = event.payload?.state;
			const message = event.payload?.message;
			if (typeof state === 'string') {
				const msg = typeof message === 'string' ? message : '';
				connection.setRemoteState(state as any, msg);
				const level = state === 'connected' ? 'success' : state === 'error' ? 'error' : 'info';
				logs.addRuntime(`远程连接: ${state}${msg ? ' — ' + msg : ''}`, level as any, event.timestamp);
			}
			break;
		}
		// runtime 视角的本地 IPC 客户端连/断（诊断：确认 runtime 检测到 GUI 会话变化）
		case 'connection.ipc_client': {
			const state = event.payload?.state;
			if (typeof state === 'string') {
				const level = state === 'connected' ? 'success' : 'warning';
				logs.addRuntime(`runtime IPC 客户端${state === 'connected' ? '上线' : '下线'}`, level as any, event.timestamp);
			}
			break;
		}
		// screenshot.frame 预留：runtime 侧 ScreenshotReporter 未接入，待后续连线
		case 'screenshot.frame':
			break;
	}
}

export function createEventPoller() {
	let timer: ReturnType<typeof setInterval> | undefined;
	let running = false;

	async function tick() {
		const invoke = (window as any).__TAURI_INVOKE__;
		if (!invoke) return;
		try {
			const result = (await invoke('drain_events')) as {
				events?: RuntimeEvent[];
				dropped?: number;
			} | undefined;
			if (result && typeof result.dropped === 'number') {
				logs.notifyDropped(result.dropped);
			}
			const events = result?.events;
			if (Array.isArray(events)) {
				for (const event of events) {
					try {
						dispatch(event);
					} catch {
						// 单条事件分发失败不影响整体
					}
				}
			}
		} catch {
			// 瞬时错误（如 runtime 暂时不可用），下一轮重试
		}
	}

	return {
		start() {
			if (running) return;
			running = true;
			tick(); // 立即拉一次
			timer = setInterval(tick, POLL_INTERVAL_MS);
		},
		stop() {
			running = false;
			if (timer !== undefined) {
				clearInterval(timer);
				timer = undefined;
			}
		},
	};
}
