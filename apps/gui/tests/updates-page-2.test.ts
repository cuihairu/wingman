import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

/**
 * 更新页面 invoke 模式：已启用状态渲染、发现新版本（事件回投路径）、
 * 点击检查/下载调用对应 Tauri 命令并回填状态。
 * dev 模式（离线/未启用提示）见 updates-page。
 * 与其他 invoke 模式文件一致：整体重置模块注册表后动态加载全部依赖
 * （含 @testing-library/svelte，避免双 svelte runtime 实例导致 effect_orphan）。
 */

type Handler = (cmd: string, args?: Record<string, unknown>) => unknown;

type Rtl = typeof import('@testing-library/svelte');

const EMPTY_CANDIDATE = {
	version: '', url: '', sha256: '', size: 0, notes: '', mandatory: false, fileName: '',
};

function status(overrides: Record<string, unknown> = {}): Record<string, unknown> {
	return {
		enabled: true,
		available: false,
		downloading: false,
		downloaded: false,
		localVersion: '0.1.2',
		candidate: { ...EMPTY_CANDIDATE },
		downloadPath: '',
		lastCheckMs: 0,
		lastError: '',
		...overrides,
	};
}

let rtl: Rtl;
let get: typeof import('svelte/store').get;

async function setup(handler: Handler) {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
	const [testingLib, svelteStore, pageMod, connectionMod, updatesMod, logsMod] = await Promise.all([
		import('@testing-library/svelte'),
		import('svelte/store'),
		import('../src/routes/updates/+page.svelte'),
		import('$lib/stores/connection'),
		import('$lib/stores/updates'),
		import('$lib/stores/logs'),
	]);
	rtl = testingLib as unknown as Rtl;
	get = svelteStore.get;
	await connectionMod.connection.connect();
	return {
		Page: pageMod.default as any,
		connection: connectionMod.connection,
		updates: updatesMod.updates,
		logs: logsMod.logs,
	};
}

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
});

afterEach(() => {
	rtl?.cleanup();
	vi.restoreAllMocks();
	delete (window as any).__TAURI_INVOKE__;
});

describe('更新页面（invoke 模式）', () => {
	it('已启用：渲染指标卡与已是最新版本', async () => {
		const { Page, connection } = await setup((cmd) => {
			if (cmd === 'get_update_status') return status({});
			return { started: false, status: status({}) };
		});
		try {
			rtl.render(Page);
			await rtl.waitFor(() => {
				// 状态徽标、「状态」指标卡与横幅三处同文
				expect(rtl.screen.getAllByText('已是最新版本').length).toBe(3);
			});
			expect(rtl.screen.getByText('更新状态')).toBeInTheDocument();
			expect(rtl.screen.getByText('当前版本')).toBeInTheDocument();
			expect(rtl.screen.getByText('0.1.2')).toBeInTheDocument();
			expect(rtl.screen.getByRole('button', { name: '检查更新' })).toBeInTheDocument();
		} finally {
			await connection.disconnect();
		}
	});

	it('发现新版本（事件回投）后可触发下载', async () => {
		const { Page, connection, updates } = await setup((cmd) => {
			if (cmd === 'get_update_status') return status({});
			return { started: false, status: status({}) };
		});
		try {
			rtl.render(Page);
			await rtl.waitFor(() => {
				expect(rtl.screen.getAllByText('已是最新版本').length).toBe(3);
			});
			updates.applyStatus(
				status({
					available: true,
					candidate: { ...EMPTY_CANDIDATE, version: '0.2.0', fileName: 'wingman-0.2.0-setup.exe' },
				})
			);
			await rtl.waitFor(() => {
				expect(rtl.screen.getByText('发现新版本 0.2.0')).toBeInTheDocument();
			});
			expect(rtl.screen.getAllByText('0.2.0').length).toBeGreaterThan(0);
			expect(rtl.screen.getByRole('button', { name: '下载更新' })).toBeInTheDocument();
		} finally {
			await connection.disconnect();
		}
	});

	it('点击检查更新调用 check_update 并记录日志', async () => {
		const { Page, connection, logs } = await setup((cmd) => {
			if (cmd === 'get_update_status') return status({});
			if (cmd === 'check_update') return { started: true, status: status({}) };
			return { started: false, status: status({}) };
		});
		try {
			rtl.render(Page);
			await rtl.waitFor(() => {
				expect(rtl.screen.getAllByText('已是最新版本').length).toBe(3);
			});
			await rtl.fireEvent.click(rtl.screen.getByRole('button', { name: '检查更新' }));
			const calls = ((window as any).__TAURI_INVOKE__ as ReturnType<typeof vi.fn>).mock.calls;
			expect(calls.some((c: unknown[]) => c[0] === 'check_update')).toBe(true);
			expect(get(logs).some(e => e.message.includes('正在检查更新'))).toBe(true);
		} finally {
			await connection.disconnect();
		}
	});

	it('点击下载更新调用 download_update 并显示下载中', async () => {
		const { Page, connection, updates } = await setup((cmd) => {
			if (cmd === 'get_update_status') return status({});
			if (cmd === 'download_update') return { started: true, status: status({ downloading: true }) };
			return { started: false, status: status({}) };
		});
		try {
			rtl.render(Page);
			await rtl.waitFor(() => {
				expect(rtl.screen.getAllByText('已是最新版本').length).toBe(3);
			});
			updates.applyStatus(
				status({
					available: true,
					candidate: { ...EMPTY_CANDIDATE, version: '0.2.0', fileName: 'wingman-0.2.0-setup.exe' },
				})
			);
			await rtl.waitFor(() => {
				expect(rtl.screen.getByRole('button', { name: '下载更新' })).toBeInTheDocument();
			});
			await rtl.fireEvent.click(rtl.screen.getByRole('button', { name: '下载更新' }));
			const calls = ((window as any).__TAURI_INVOKE__ as ReturnType<typeof vi.fn>).mock.calls;
			expect(calls.some((c: unknown[]) => c[0] === 'download_update')).toBe(true);
			await rtl.waitFor(() => {
				// 状态徽标与「状态」指标卡同文
				expect(rtl.screen.getAllByText('正在下载更新').length).toBe(2);
				expect(rtl.screen.getByRole('button', { name: '下载中…' })).toBeDisabled();
			});
		} finally {
			await connection.disconnect();
		}
	});
});
