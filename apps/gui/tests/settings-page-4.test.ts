import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/settings/+page.svelte';
import { connection } from '$lib/stores/connection';
import { settings } from '$lib/stores/settings';
import { logs } from '$lib/stores/logs';
import { profiles, type GameProfile } from '$lib/stores/profiles';

/**
 * 设置页第四组：远程链路状态 switch 全类型、编辑面板非空集合渲染、
 * 日志级别切换写路径。
 * 编辑面板空态见 settings-page.test.ts，invoke 状态分支见 settings-invoke。
 */

function fullProfile(id: string, extra: Partial<GameProfile> = {}): GameProfile {
	return {
		id,
		name: `配置${id}`,
		version: '1.0',
		description: '演示描述',
		window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
		colors: [
			{ name: '血条红', r: 255, g: 0, b: 0, tolerance: 12 },
			{ name: '魔法蓝', r: 0, g: 120, b: 255, tolerance: 8 },
		],
		images: [{ name: '主界面 logo', path: 'images/logo.png' }],
		triggers: [
			{ name: '回血', type: 'color_found', enabled: true },
			{ name: '报警', type: 'pixel_changed', enabled: false },
		],
		scripts: [{ name: 'farm', path: 'scripts/farm.lua' }],
		hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
		settings: {},
		...extra,
	} as GameProfile;
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	settings.update({ logLevel: 'info' });
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.disconnect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('设置页：远程链路状态视图', () => {
	it.each([
		['connected', '已连接'],
		['connecting', '连接中'],
		['reconnecting', '重连中'],
		['error', '连接错误'],
		['disconnected', '未连接'],
	])('远程状态 %s 显示 %s', async (state, text) => {
		connection.setRemoteState(state as any, '');
		render(Page);
		expect(screen.getByText(new RegExp(`远程链路: ${text}`))).toBeInTheDocument();
	});
});

describe('设置页：编辑面板非空集合', () => {
	it('颜色/图像/触发器/脚本配置逐项渲染', async () => {
		await profiles.importFromJson(JSON.stringify(fullProfile('rich')));
		render(Page);
		await fireEvent.click(screen.getByText('配置rich'));
		await waitFor(() => {
			expect(screen.getByText('编辑: 配置rich')).toBeInTheDocument();
		});

		// 颜色：名称 + 容差
		expect(screen.getByText('血条红')).toBeInTheDocument();
		expect(screen.getByText('魔法蓝')).toBeInTheDocument();
		expect(screen.getByText('容差 12')).toBeInTheDocument();

		// 图像：名称 + 路径
		expect(screen.getByText('主界面 logo')).toBeInTheDocument();
		expect(screen.getByText('images/logo.png')).toBeInTheDocument();

		// 触发器：启用/禁用两侧（item-detail 显示 type 字段）
		expect(screen.getByText(/color_found · 启用/)).toBeInTheDocument();
		expect(screen.getByText(/pixel_changed · 禁用/)).toBeInTheDocument();

		// 脚本（非空集合渲染，无空态提示）
		expect(screen.queryByText('暂无脚本')).not.toBeInTheDocument();
		expect(screen.getByText('farm')).toBeInTheDocument();
	});
});

describe('设置页：日志级别切换', () => {
	it('切换 select 更新 settings.logLevel', async () => {
		render(Page);
		const select = screen.getByDisplayValue('info') as HTMLSelectElement;
		await fireEvent.change(select, { target: { value: 'warn' } });
		await waitFor(() => {
			expect(get(settings).logLevel).toBe('warn');
		});
	});
});
