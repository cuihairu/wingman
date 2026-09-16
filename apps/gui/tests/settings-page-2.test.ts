import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/settings/+page.svelte';
import { connection } from '$lib/stores/connection';
import { settings } from '$lib/stores/settings';
import { logs } from '$lib/stores/logs';
import { profiles, type GameProfile } from '$lib/stores/profiles';

/** 设置页第二组：编辑面板非空分区、复选框与连接失败路径（基础见 settings-page.test.ts） */

function fullProfile(id: string): GameProfile {
	return {
		id,
		name: `配置${id}`,
		version: '1.0',
		description: '完整配置',
		window: { title: '游戏窗口', className: '', processName: 'game.exe', exactMatch: false, fullscreen: true },
		colors: [{ name: '血条红', r: 255, g: 0, b: 0, tolerance: 12 }],
		images: [{ name: '按钮图', path: 'assets/button.png' }],
		triggers: [{ name: '掉血触发', type: 'color_found', enabled: true }],
		scripts: [{ name: '主脚本', path: 'scripts/main.lua', autoStart: true, restartOnCrash: false, priority: 0 }],
		hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
		settings: {},
	};
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	// profiles 为模块级单例，清空上一用例注入的配置
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.connect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('设置页：配置编辑面板', () => {
	it('非空资源分区渲染颜色/图像/触发器/脚本明细', async () => {
		await profiles.importFromJson(JSON.stringify(fullProfile('p1')));
		render(Page);

		await fireEvent.click(screen.getByText('配置p1'));
		await waitFor(() => {
			expect(screen.getByText('编辑: 配置p1')).toBeInTheDocument();
		});

		// 颜色：名称 + 容差
		expect(screen.getByText('血条红')).toBeInTheDocument();
		expect(screen.getByText('容差 12')).toBeInTheDocument();
		// 图像：名称 + 路径
		expect(screen.getByText('按钮图')).toBeInTheDocument();
		expect(screen.getByText('assets/button.png')).toBeInTheDocument();
		// 触发器：类型与启用态
		expect(screen.getByText('掉血触发')).toBeInTheDocument();
		expect(screen.getByText('color_found · 启用')).toBeInTheDocument();
		// 脚本：路径
		expect(screen.getByText('主脚本')).toBeInTheDocument();
		expect(screen.getByText('scripts/main.lua')).toBeInTheDocument();

		// 空态提示不再出现
		expect(screen.queryByText('暂无颜色配置')).not.toBeInTheDocument();
		expect(screen.queryByText('暂无图像配置')).not.toBeInTheDocument();
		expect(screen.queryByText('暂无触发器')).not.toBeInTheDocument();
		expect(screen.queryByText('暂无脚本')).not.toBeInTheDocument();
	});

	it('窗口绑定字段编辑：标题/进程名/精确匹配/全屏切换', async () => {
		await profiles.importFromJson(JSON.stringify(fullProfile('p1')));
		render(Page);
		await fireEvent.click(screen.getByText('配置p1'));
		await waitFor(() => {
			expect(screen.getByText('编辑: 配置p1')).toBeInTheDocument();
		});

		await fireEvent.input(screen.getByPlaceholderText('窗口标题（模糊匹配）'), { target: { value: '新标题' } });
		await fireEvent.input(screen.getByPlaceholderText('game.exe'), { target: { value: 'new.exe' } });

		const exact = screen.getByText('精确匹配').closest('label')!.querySelector('input') as HTMLInputElement;
		expect(exact.checked).toBe(false);
		await fireEvent.click(exact);
		expect(exact.checked).toBe(true);

		const fullscreen = screen.getByText('全屏模式').closest('label')!.querySelector('input') as HTMLInputElement;
		expect(fullscreen.checked).toBe(true); // fullProfile 默认全屏
		await fireEvent.click(fullscreen);
		expect(fullscreen.checked).toBe(false);

		// 保存持久化全部编辑
		await fireEvent.click(screen.getByRole('button', { name: '保存' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.message === '已保存配置: 配置p1')).toBe(true);
		});
		const saved = get(profiles)[0];
		expect(saved.window.title).toBe('新标题');
		expect(saved.window.processName).toBe('new.exe');
		expect(saved.window.exactMatch).toBe(true);
		expect(saved.window.fullscreen).toBe(false);
	});

	it('描述与热键编辑后保存', async () => {
		await profiles.importFromJson(JSON.stringify(fullProfile('p1')));
		render(Page);
		await fireEvent.click(screen.getByText('配置p1'));
		await waitFor(() => {
			expect(screen.getByText('编辑: 配置p1')).toBeInTheDocument();
		});

		const descInput = screen.getByDisplayValue('完整配置');
		await fireEvent.input(descInput, { target: { value: '新描述' } });

		// 急停热键：小写+多余空格 → 大写去空白
		const emergencyInput = screen.getByDisplayValue('F12');
		await fireEvent.input(emergencyInput, { target: { value: 'ctrl+shift+x, f9' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存' }));
		await waitFor(() => {
			const saved = get(profiles)[0];
			expect(saved.description).toBe('新描述');
			expect(saved.hotkeys.emergencyStop).toEqual(['CTRL+SHIFT+X', 'F9']);
		});
	});

	it('设为当前按钮激活配置', async () => {
		await profiles.importFromJson(JSON.stringify(fullProfile('p1')));
		await profiles.importFromJson(JSON.stringify(fullProfile('p2')));
		render(Page);
		await fireEvent.click(screen.getByText('配置p1'));
		await waitFor(() => {
			expect(screen.getByText('编辑: 配置p1')).toBeInTheDocument();
		});
		await fireEvent.click(screen.getByRole('button', { name: '设为当前' }));
		await waitFor(() => {
			expect(get(profiles).find(p => p.id === 'p1')?.id).toBeTruthy();
		});
		// 激活徽标出现在列表
		await waitFor(() => {
			expect(screen.getByText('当前')).toBeInTheDocument();
		});
	});
});

describe('设置页：应用设置边界', () => {
	it('orchestrator 地址清空后回退默认值', async () => {
		render(Page);
		const input = screen.getByPlaceholderText('http://localhost:9527') as HTMLInputElement;
		await fireEvent.input(input, { target: { value: 'http://elsewhere:1' } });
		await fireEvent.change(input, { target: { value: '   ' } });
		await waitFor(() => {
			expect(get(settings).orchestratorUrl).toBe('http://localhost:9527');
		});
	});

	it('日志级别切换持久化', async () => {
		render(Page);
		const select = screen.getByDisplayValue('info') as HTMLSelectElement;
		await fireEvent.change(select, { target: { value: 'warn' } });
		await waitFor(() => {
			expect(get(settings).logLevel).toBe('warn');
		});
	});

	it('连接失败记录错误日志', async () => {
		await connection.disconnect();
		vi.spyOn(connection, 'connect').mockRejectedValue(new Error('endpoint missing'));
		render(Page);

		// 未连接时按钮为「连接」，点击走 connect 失败分支
		await fireEvent.click(screen.getByRole('button', { name: '连接', exact: true }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message === '连接失败: Error: endpoint missing')).toBe(true);
		});
	});
});
