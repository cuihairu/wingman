import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/dashboard/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { triggers } from '$lib/stores/triggers';
import { profiles, type GameProfile } from '$lib/stores/profiles';

/**
 * 仪表板第三组：暂停切换成功两侧与状态样式、status 空字段回退、
 * 活动配置渲染边界（空描述/停用脚本/停用触发器/缺省集合）、日志空态。
 * 快速操作失败路径见 dashboard-page-2。
 */

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	triggers.set([]);
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.connect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

function logMessages(): string[] {
	return get(logs).map(e => e.message);
}

describe('仪表板：暂停切换', () => {
	it('togglePause 成功按当前状态记录恢复日志', async () => {
		vi.spyOn(connection, 'togglePause').mockResolvedValue(undefined as any);
		render(Page);
		// dev 连接后 paused=false → 记录「已恢复」
		await fireEvent.click(screen.getByRole('button', { name: '暂停全部' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '全部脚本已恢复')).toBe(true);
		});
	});

	it('togglePause 失败记录错误日志', async () => {
		vi.spyOn(connection, 'togglePause').mockRejectedValue(new Error('busy'));
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '暂停全部' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '切换暂停状态失败: Error: busy')).toBe(true);
		});
	});
});

describe('仪表板：活动配置渲染边界', () => {
	it('空描述回退、停用脚本/触发器状态、缺省集合计 0', async () => {
		// 脚本/触发器 compact 列表来自 store，注入停用条目
		const { scripts } = await import('$lib/stores/scripts');
		scripts.set([{
			id: 'idle', name: 'idle.lua', path: 'scripts/idle.lua', size: 512,
			is_running: false, state: 'stopped', error: '', loaded_at: 0,
		}]);
		triggers.set([{
			id: 'off', name: '停用触发', enabled: false,
			condition: { type: 'color_found', value: '#ff0000', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 },
			actions: [],
		} as any]);

		const bare = {
			id: 'bare',
			name: '裸配置',
			version: '1.0',
			description: '',
			window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [], images: [], triggers: [],
			scripts: [],
			hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
			settings: {},
		} as unknown as GameProfile;
		await profiles.importFromJson(JSON.stringify(bare));
		await profiles.setActive('bare');
		render(Page);

		// 空描述回退
		expect(screen.getByText('未填写描述')).toBeInTheDocument();
		expect(screen.getByText('停用触发')).toBeInTheDocument();
		expect(screen.getByText('已停止')).toBeInTheDocument();
		expect(screen.getByText('停用')).toBeInTheDocument();
	});

	it('配置缺失 scripts/triggers 字段时计数为 0', async () => {
		const lean: Record<string, unknown> = {
			id: 'lean',
			name: '精简配置',
			version: '1.0',
			description: 'd',
			window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [],
			images: [],
			hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
			settings: {},
		};
		await profiles.importFromJson(JSON.stringify(lean));
		await profiles.setActive('lean');
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('精简配置')).toBeInTheDocument();
		});
		expect(screen.getByText('0 个脚本')).toBeInTheDocument();
		expect(screen.getByText('0 个触发器')).toBeInTheDocument();
	});
});

describe('仪表板：日志空态', () => {
	it('清空日志后最近日志显示空态', async () => {
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('仪表板')).toBeInTheDocument();
		});
		logs.clear();
		await waitFor(() => {
			expect(screen.getByText('暂无日志')).toBeInTheDocument();
		});
	});
});

describe('仪表板：刷新结果为空', () => {
	it('refresh 返回 null（IPC 失败被 store 吞掉）时保留 status 仍完成本轮刷新', async () => {
		vi.spyOn(connection, 'refresh').mockResolvedValue(null as any);
		render(Page);
		// result 为 null 走 if 的 else 侧：status 不被覆盖，但 lastUpdated 仍更新（不再是 '-'）
		await waitFor(() => {
			expect(screen.getByText(/本地 runtime 控制台 · 最近刷新 (?!-)/)).toBeInTheDocument();
		});
	});
});
