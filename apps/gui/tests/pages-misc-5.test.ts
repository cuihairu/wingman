import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor, cleanup } from '@testing-library/svelte';
import App from '../src/App.svelte';
import ColorPicker from '$lib/components/ColorPicker.svelte';
import DashboardPage from '../src/routes/dashboard/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { profiles } from '$lib/stores/profiles';
import { installCanvasStub, installTolerantStyleAttrStub, uninstallTolerantStyleAttrStub } from './helpers/test-utils';

/** 杂项第五组：App 路由切换、心跳断线告警（invoke 模式假时钟）、仪表板状态字段回退、取色器非法色。 */

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	installTolerantStyleAttrStub();
	installCanvasStub();
	logs.clear();
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.connect();
});

afterEach(() => {
	// 静态 RTL 实例的 render 必须显式清理：auto-cleanup 关闭时残留组件会
	// 在后续测试期间响应 store 更新抛 unhandled error
	cleanup();
	uninstallTolerantStyleAttrStub();
	vi.useRealTimers();
	vi.restoreAllMocks();
});

describe('App：路由切换', () => {
	it('侧栏进入触发器页与屏幕页', async () => {
		const { container } = render(App);
		expect(container.querySelector('.content-area')).toBeTruthy();
		await fireEvent.click(screen.getByRole('button', { name: /触发器/ }));
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '添加触发器' })).toBeInTheDocument();
		});
		await fireEvent.click(screen.getByRole('button', { name: /屏幕预览|屏幕/ }));
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '刷新截图' })).toBeInTheDocument();
		});
	});
});

describe('App：心跳断线告警（invoke 模式）', () => {
	it('心跳 refresh 失败写连接中断日志', async () => {
		vi.resetModules();
		localStorage.removeItem('wingman-settings');
		localStorage.removeItem('wingman.theme');
		let statusCalls = 0;
		(window as any).__TAURI_INVOKE__ = vi.fn((cmd: string) => {
			if (cmd === 'connect_ipc') return {};
			if (cmd === 'get_system_status') {
				statusCalls += 1;
				if (statusCalls === 1) {
					return { server: 'wingman', version: '1.0', uptime: 0, running_scripts: 0, paused: false };
				}
				throw new Error('ipc lost');
			}
			// 前一测试导航后 hash 停在 #screen：重载后的 App 会渲染 screen 页，
			// 截图/显示器命令须返回合法 shape，否则渲染期读取 region.x 抛 unhandled error
			if (cmd === 'capture_screenshot') {
				return {
					image: 'data:image/png;base64,', width: 800, height: 600, timestamp: 0,
					region: { x: 0, y: 0, width: 800, height: 600 },
				};
			}
			if (cmd === 'list_monitors') return [];
			return {};
		});
		// 拦截 setInterval：捕获 App 心跳（5000ms）回调，其余 interval 不真正排期
		let heartbeat: (() => Promise<void>) | null = null;
		const origInterval = window.setInterval.bind(window);
		const origClear = window.clearInterval.bind(window);
		(window as any).setInterval = vi.fn((cb: any, ms?: number) => {
			if (ms === 5000) {
				heartbeat = cb;
				return 9999;
			}
			return 7777;
		});
		(window as any).clearInterval = vi.fn();

		try {
			const appMod = await import('../src/App.svelte');
			const connMod = await import('$lib/stores/connection');
			const logsMod = await import('$lib/stores/logs');
			const testingLib = await import('@testing-library/svelte');
			testingLib.render(appMod.default as any);
			// App 自动连接用掉第 1 次（成功）的 get_system_status
			await testingLib.waitFor(() => {
				expect(get(connMod.connection).connected).toBe(true);
				expect(heartbeat).toBeTruthy();
			});
			await heartbeat!();
			const messages = get(logsMod.logs).map(e => e.message);
			expect(messages.some(m => m.includes('与本地 runtime 的连接中断'))).toBe(true);
		} finally {
			(window as any).setInterval = origInterval;
			(window as any).clearInterval = origClear;
			// 正常卸载组件（销毁 effect），粗暴清 DOM 会让残留 effect 在
			// 后续测试期间响应 store 更新并抛错（unhandled error 噪音）
			(await import('@testing-library/svelte')).cleanup();
		}
	});
});

describe('仪表板：状态字段回退', () => {
	it('refresh 返回空 server/version 时回退默认值', async () => {
		vi.spyOn(connection, 'refresh').mockResolvedValue({
			server: '',
			version: '',
			uptime: 0,
			running_scripts: 0,
			paused: false,
		} as any);
		render(DashboardPage);
		await waitFor(() => {
			// server 为空串 → 回退 'wingman'
			expect(screen.getByText('server wingman')).toBeInTheDocument();
		});
	});
});

describe('ColorPicker：非法色值', () => {
	it('非法色值时预览块回退黑色', async () => {
		const { container } = render(ColorPicker, { value: 'not-a-color' });
		const preview = container.querySelector('.color-preview') as HTMLElement;
		expect(preview.getAttribute('style')).toContain('rgb(0, 0, 0)');
		// 输入框标记 invalid
		expect(container.querySelector('.color-input')?.className).toContain('invalid');
	});
});
