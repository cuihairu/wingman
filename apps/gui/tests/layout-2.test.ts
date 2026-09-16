import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import TopBar from '../src/lib/components/layout/TopBar.svelte';
import Sidebar from '../src/lib/components/layout/Sidebar.svelte';
import { router } from '$lib/router.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';

/** 布局第二组：TopBar 急停/启停配置失败路径与已连接重试守卫、Sidebar macros 图标分支 */

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	await connection.disconnect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('TopBar：操作失败路径', () => {
	it('急停/启动配置/停止配置失败分别记录错误日志', async () => {
		await connection.connect();
		vi.spyOn(connection, 'stopAll').mockRejectedValue(new Error('stop dead'));
		vi.spyOn(connection, 'startActiveProfile').mockRejectedValue(new Error('start dead'));
		vi.spyOn(connection, 'stopActiveProfile').mockRejectedValue(new Error('halt dead'));
		render(TopBar);

		await fireEvent.click(screen.getByTitle('急停'));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message === '急停失败: Error: stop dead')).toBe(true);
		});

		await fireEvent.click(screen.getByTitle('启动当前配置'));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message === '启动当前配置失败: Error: start dead')).toBe(true);
		});

		await fireEvent.click(screen.getByTitle('停止当前配置'));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message === '停止当前配置失败: Error: halt dead')).toBe(true);
		});
	});

	it('已连接时点击状态徽章不触发重试（canRetry 守卫）', async () => {
		await connection.connect();
		const retrySpy = vi.spyOn(connection, 'retryNow');
		render(TopBar);

		await fireEvent.click(screen.getByRole('button', { name: /已连接/ }));
		expect(retrySpy).not.toHaveBeenCalled();
		expect(get(logs).some(e => e.message.includes('已重新连接'))).toBe(false);
	});
});

describe('Sidebar：导航图标分支', () => {
	it('macros 页激活时渲染宏图标（icon 分支兜底前的最后一个具名分支）', () => {
		router.navigate('macros');
		const { container } = render(Sidebar);
		const active = container.querySelector('.nav-item.active, [aria-current="page"]');
		expect(active).toBeTruthy();
		expect(active!.textContent).toContain('宏');
	});
});
