import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/dashboard/+page.svelte';
import { router } from '$lib/router.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { triggers } from '$lib/stores/triggers';
import { profiles, type GameProfile } from '$lib/stores/profiles';

/**
 * 仪表板第二组：快速操作失败路径、刷新异常、3s 自动刷新回调、
 * 详情面板导航入口与 conditionLabel 未知类型回退。
 * 骨架/成功路径见 dashboard-page.test.ts。
 */

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	triggers.set([]);
	await connection.disconnect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

function logMessages(): string[] {
	return get(logs).map(e => e.message);
}

function fullProfile(id: string): GameProfile {
	return {
		id,
		name: `配置${id}`,
		version: '1.0',
		description: '描述',
		window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
		colors: [], images: [], triggers: [],
		scripts: [{ name: 'main.lua', path: 'scripts/main.lua', autoStart: false, restartOnCrash: false, priority: 0 }],
		hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
		settings: {},
	};
}

describe('仪表板：快速操作失败路径', () => {
	it('四个快速操作失败分别记录错误日志', async () => {
		await connection.connect();
		// 启动操作需要已激活配置（否则提前 warning 返回）
		await profiles.importFromJson(JSON.stringify(fullProfile('p1')));
		await profiles.setActive('p1');
		vi.spyOn(connection, 'startActiveProfile').mockRejectedValue(new Error('start refused'));
		vi.spyOn(connection, 'stopActiveProfile').mockRejectedValue(new Error('stop refused'));
		vi.spyOn(connection, 'stopAll').mockRejectedValue(new Error('all refused'));
		vi.spyOn(connection, 'togglePause').mockRejectedValue(new Error('pause refused'));
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: /启动当前配置/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '启动当前配置失败: Error: start refused')).toBe(true);
		});

		await fireEvent.click(screen.getByRole('button', { name: /停止当前配置/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '停止当前配置失败: Error: stop refused')).toBe(true);
		});

		await fireEvent.click(screen.getByRole('button', { name: /停止全部/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '停止全部失败: Error: all refused')).toBe(true);
		});

		await fireEvent.click(screen.getByRole('button', { name: /暂停全部|恢复全部/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '切换暂停状态失败: Error: pause refused')).toBe(true);
		});
	});
});

describe('仪表板：刷新异常与自动刷新', () => {
	it('refresh 抛出异常记录错误日志（防御分支）', async () => {
		await connection.connect();
		vi.spyOn(connection, 'refresh').mockRejectedValue(new Error('boom'));
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: /刷新/ }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '刷新仪表板失败: Error: boom')).toBe(true);
		});
		// 刷新中状态复位
		await waitFor(() => {
			expect(screen.getByRole('button', { name: /刷新/ })).not.toBeDisabled();
		});
	});

	it('3s 自动刷新推进最近刷新时间', async () => {
		await connection.connect();
		render(Page);
		// 首刷（挂载 effect）完成后读取时间戳，等待 3s interval 回调再次推进
		await waitFor(() => {
			expect(screen.getByText(/最近刷新/).textContent).not.toMatch(/最近刷新 -$/);
		});
		const first = screen.getByText(/最近刷新/).textContent!;
		await waitFor(() => {
			expect(screen.getByText(/最近刷新/).textContent).not.toBe(first);
		}, { timeout: 4500 });
	});
});

describe('仪表板：详情面板导航与标签回退', () => {
	it('脚本/触发器/日志三个「打开」入口导航', async () => {
		await connection.connect();
		render(Page);

		const panels = screen.getAllByRole('button', { name: '打开' });
		expect(panels).toHaveLength(3);

		await fireEvent.click(panels[0]);
		expect(get(router).current).toBe('scripts');
		await fireEvent.click(screen.getAllByRole('button', { name: '打开' })[1]);
		expect(get(router).current).toBe('triggers');
		await fireEvent.click(screen.getAllByRole('button', { name: '打开' })[2]);
		expect(get(router).current).toBe('logs');
	});

	it('conditionLabel 未收录的类型原样显示（window_opened）', async () => {
		await connection.connect();
		triggers.set([{
			id: 'w1',
			name: '窗口监视',
			enabled: true,
			condition: { type: 'window_opened', value: 'x', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 0, interval: 0 } as any,
			actions: [],
		}]);
		render(Page);
		expect(screen.getByText('窗口监视')).toBeInTheDocument();
		// labels 表未收录 window_opened → 显示原始类型名
		expect(screen.getByText(/window_opened · 0 个动作/)).toBeInTheDocument();
	});
});
