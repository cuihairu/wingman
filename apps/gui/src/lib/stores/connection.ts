import { writable, get } from 'svelte/store';
import { settings } from './settings';
import { logs } from './logs';

/// 本地 IPC 链路状态机：
/// disconnected（初始/手动断开）→ connecting → connected
/// connected → error（心跳失败）→ reconnecting → connecting → ...
export type IpcLinkState = 'disconnected' | 'connecting' | 'connected' | 'reconnecting' | 'error';

interface RemoteLinkState {
	state: 'connected' | 'connecting' | 'disconnected' | 'reconnecting' | 'error';
	message: string;
}

export interface IpcStatus {
	/** 当前链路状态 */
	state: IpcLinkState;
	/** 最近一次错误/状态说明（空串表示无异常） */
	message: string;
	/** 自动重连连续尝试次数（连接成功后清零） */
	attempts: number;
	lastErrorAt: number | null;
	lastConnectedAt: number | null;
}

interface ConnectionState {
	connected: boolean;
	version: string;
	paused: boolean;
	ipcEndpoint: string;
	/** 本地 runtime IPC 链路详情 */
	ipc: IpcStatus;
	/** runtime → Go server 远程链路状态（事件 + 心跳双重来源） */
	remote: RemoteLinkState | null;
	/** runtime 视角状态（来自 system.getStatus，旧 runtime 为 null） */
	runtimeView: { ipcClientConnected: boolean | null };
}

export interface SystemStatus {
	server: string;
	version: string;
	uptime: number;
	running_scripts: number;
	paused: boolean;
	/** runtime → Go server 远程链路（新 runtime 提供，旧版缺省） */
	remote_connected?: boolean;
	remote_state?: string;
	/** runtime 视角：本地 IPC 客户端（本 GUI）是否在线 */
	ipc_client_connected?: boolean;
	mode?: number;
}

/// 自动重连退避参数：1s 起步指数退避，30s 封顶
const RECONNECT_BASE_DELAY_MS = 1000;
const RECONNECT_MAX_DELAY_MS = 30000;
/// 重连失败日志记录频率：第 1 次与每第 5 次记录，避免刷屏
const RECONNECT_LOG_INTERVAL = 5;

function errorMessage(error: unknown): string {
	if (error instanceof Error) return error.message;
	return String(error);
}

function createConnectionStore() {
	const store = writable<ConnectionState>({
		connected: false,
		version: '-',
		paused: false,
		ipcEndpoint: 'wingman',
		ipc: {
			state: 'disconnected',
			message: '',
			attempts: 0,
			lastErrorAt: null,
			lastConnectedAt: null,
		},
		remote: null,
		runtimeView: { ipcClientConnected: null },
	});
	let currentState: ConnectionState = {
		connected: false,
		version: '-',
		paused: false,
		ipcEndpoint: 'wingman',
		ipc: {
			state: 'disconnected',
			message: '',
			attempts: 0,
			lastErrorAt: null,
			lastConnectedAt: null,
		},
		remote: null,
		runtimeView: { ipcClientConnected: null },
	};

	store.subscribe(value => {
		currentState = value;
	});

	const invoke = (window as any).__TAURI_INVOKE__;

	// ---- 自动重连调度器（闭包状态） ----
	let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
	let reconnectAttempt = 0;
	let manualDisconnect = false;
	let connectPromise: Promise<void> | null = null;
	let reconnectedHook: (() => Promise<void>) | null = null;

	/** 合并 system.getStatus 中的远程链路与 runtime 视角状态（事件驱动的补充，修复徽标陈旧） */
	function mergeRemoteFromStatus(status: SystemStatus) {
		store.update(s => ({
			...s,
			...(status.remote_state
				? {
					remote: {
						state: status.remote_state as RemoteLinkState['state'],
						message: s.remote?.message || '',
					} as RemoteLinkState,
				}
				: {}),
			runtimeView: {
				ipcClientConnected:
					typeof status.ipc_client_connected === 'boolean'
						? status.ipc_client_connected
						: null,
			},
		}));
	}

	function patchIpc(patch: Partial<IpcStatus>) {
		store.update(s => {
			const ipc = { ...s.ipc, ...patch };
			return { ...s, ipc, connected: ipc.state === 'connected' };
		});
	}

	function clearReconnectTimer() {
		if (reconnectTimer !== null) {
			clearTimeout(reconnectTimer);
			reconnectTimer = null;
		}
	}

	function reconnectDelayMs(attempt: number): number {
		return Math.min(RECONNECT_BASE_DELAY_MS * 2 ** attempt, RECONNECT_MAX_DELAY_MS);
	}

	/** 注入重连成功后的数据重载逻辑（App 启动时调用一次） */
	function onReconnected(hook: () => Promise<void>) {
		reconnectedHook = hook;
	}

	/** 调度一次自动重连：已调度、手动断开或设置关闭时跳过 */
	function scheduleReconnect() {
		if (!invoke || manualDisconnect || reconnectTimer !== null) return;
		if (!get(settings).autoReconnect) return;

		const delay = reconnectDelayMs(reconnectAttempt);
		reconnectAttempt += 1;
		patchIpc({ state: 'reconnecting', attempts: reconnectAttempt });
		reconnectTimer = setTimeout(() => {
			reconnectTimer = null;
			void attemptReconnect();
		}, delay);
	}

	async function attemptReconnect(): Promise<void> {
		try {
			await connect(get(settings).ipcEndpoint);
		} catch {
			if (reconnectAttempt === 1 || reconnectAttempt % RECONNECT_LOG_INTERVAL === 0) {
				logs.add(
					`自动重连失败（第 ${reconnectAttempt} 次）: ${currentState.ipc.message}`,
					'warning'
				);
			}
			scheduleReconnect();
		}
	}

	/** 心跳失败：标记断线、记录错误并自动调度重连 */
	function handleConnectionLost(error: unknown) {
		patchIpc({
			state: 'error',
			message: errorMessage(error),
			lastErrorAt: Date.now(),
		});
		// IPC 断线后 runtime 远程链路状态未知，清空避免徽标陈旧误导
		store.update(s => ({ ...s, remote: null }));
		scheduleReconnect();
	}

	async function doConnect(endpoint?: string): Promise<void> {
		if (!invoke) {
			// 开发模式：无 Tauri invoke，直接模拟已连接
			patchIpc({
				state: 'connected',
				message: '',
				attempts: 0,
				lastConnectedAt: Date.now(),
			});
			store.update(s => ({
				...s,
				version: s.version === '-' ? 'wingman 0.1.0 (dev)' : s.version,
			}));
			return;
		}

		const ipcEndpoint = endpoint || get(settings).ipcEndpoint || 'wingman';
		clearReconnectTimer();
		manualDisconnect = false;
		patchIpc({ state: 'connecting', message: '' });

		try {
			await invoke('connect_ipc', { endpoint: ipcEndpoint });
			const status = await invoke('get_system_status') as SystemStatus;
			reconnectAttempt = 0;
			patchIpc({
				state: 'connected',
				message: '',
				attempts: 0,
				lastConnectedAt: Date.now(),
			});
			store.update(s => ({
				...s,
				ipcEndpoint,
				version: status.version || s.version,
				paused: status.paused,
			}));
			mergeRemoteFromStatus(status);
			await refreshEndpoint();
			if (reconnectedHook) {
				try {
					await reconnectedHook();
				} catch (hookError) {
					logs.add(`连接后数据加载失败: ${errorMessage(hookError)}`, 'warning');
				}
			}
		} catch (error) {
			patchIpc({
				state: 'error',
				message: errorMessage(error),
				lastErrorAt: Date.now(),
			});
			throw error;
		}
	}

	async function connect(endpoint?: string): Promise<void> {
		if (connectPromise) return connectPromise;
		connectPromise = doConnect(endpoint).finally(() => {
			connectPromise = null;
		});
		return connectPromise;
	}

	/** 连接成功后从 Rust 端读取实际解析的 IPC 端点（如 unix socket 完整路径） */
	async function refreshEndpoint(): Promise<void> {
		if (!invoke) return;
		try {
			const state = await invoke('get_ipc_state') as { connected: boolean; endpoint: string };
			if (state.connected) {
				store.update(s => ({ ...s, ipcEndpoint: state.endpoint }));
			}
		} catch {
			// 仅诊断信息，失败可忽略
		}
	}

	return {
		subscribe: store.subscribe,
		onReconnected,
		scheduleReconnect,
		setRemoteState(state: RemoteLinkState['state'], message: string) {
			store.update(s => ({ ...s, remote: { state, message } }));
		},
		async connect(endpoint?: string) {
			await connect(endpoint);
		},
		/** 手动重试：清除退避计数与手动断开标记后立即连接 */
		async retryNow() {
			clearReconnectTimer();
			reconnectAttempt = 0;
			manualDisconnect = false;
			await connect();
		},
		async disconnect() {
			clearReconnectTimer();
			reconnectAttempt = 0;
			manualDisconnect = true;
			if (invoke) {
				try {
					await invoke('disconnect_ipc');
				} catch {
					// 主动断开失败不阻塞状态复位
				}
			}
			patchIpc({ state: 'disconnected', message: '', attempts: 0 });
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
				mergeRemoteFromStatus(status);
				return status;
			} catch (error) {
				handleConnectionLost(error);
				return null;
			}
		},
	};
}

export const connection = createConnectionStore();
