import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/dashboard/+page.svelte';
import { connection } from '$lib/stores/connection';
import { profiles, activeProfile } from '$lib/stores/profiles';
import { logs } from '$lib/stores/logs';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { triggers } from '$lib/stores/triggers';
import { router } from '$lib/router.svelte';

function makeScript(id: string, running: boolean, size = 1024): ScriptInfo {
	return { id, name: `${id}.lua`, path: `scripts/${id}.lua`, size, is_running: running, state: running ? 'running' : 'stopped', error: '', loaded_at: 0 };
}

const baseTrigger = { enabled: true, condition: { type: 'color_found', value: '', region: { x: 0, y: 0, width: 0, height: 0 }, tolerance: 10, interval: 1000 }, actions: [] };

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	router.navigate('dashboard');
	await connection.disconnect();
	scripts.set([]);
	triggers.set([]);
	for (const p of get(profiles)) {
		await profiles.remove(p.id);
	}
});

describe('仪表板页面', () => {
	it('未连接：渲染骨架，快速操作禁用', () => {
		render(Page);
		expect(screen.getByText('仪表板')).toBeInTheDocument();
		expect(screen.getByText('未连接')).toBeInTheDocument(); // 系统状态
		expect(screen.getByText('未选择配置')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '启动当前配置' })).toBeDisabled();
		expect(screen.getByRole('button', { name: '停止当前配置' })).toBeDisabled();
		expect(screen.getByRole('button', { name: '暂停全部' })).toBeDisabled();
		expect(screen.getByRole('button', { name: '停止全部' })).toBeDisabled();
		expect(screen.getByText('暂无脚本')).toBeInTheDocument();
		expect(screen.getByText('暂无触发器')).toBeInTheDocument();
		expect(screen.getByText('暂无日志')).toBeInTheDocument();
	});

	it('未连接时点击快速操作记录错误日志（jsdom 不拦截 disabled 按钮）', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '启动当前配置' }));
		await fireEvent.click(screen.getByRole('button', { name: '停止全部' }));
		await fireEvent.click(screen.getByRole('button', { name: '暂停全部' }));
		await fireEvent.click(screen.getByRole('button', { name: '停止当前配置' }));
		const messages = get(logs).map(e => e.message);
		expect(messages.filter(m => m === '未连接到 runtime')).toHaveLength(4);
	});

	it('运行时间格式化为 HH:MM:SS', async () => {
		await connection.connect(); // dev refresh 返回 uptime 0 → 00:00:00
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('00:00:00')).toBeInTheDocument();
		});
	});

	it('脚本列表渲染尺寸格式与运行标记', async () => {
		await connection.connect();
		scripts.set([
			makeScript('tiny', false, 512),
			makeScript('kb', true, 2048),
			makeScript('mb', false, 3 * 1024 * 1024),
		]);
		render(Page);
		// 尺寸与路径在同一文本节点内（{path} · {size}），需整行匹配
		expect(screen.getByText('scripts/tiny.lua · 512 B')).toBeInTheDocument();
		expect(screen.getByText('scripts/kb.lua · 2.0 KB')).toBeInTheDocument();
		expect(screen.getByText('scripts/mb.lua · 3.0 MB')).toBeInTheDocument();
		expect(screen.getByText('已停止 2')).toBeInTheDocument();
		// 运行脚本指标优先取 status.running_scripts（dev 快照为 0 时回退本地统计 1）
		expect(screen.getByText('1')).toBeInTheDocument();
		// 「运行中/已停止」文案与系统状态面板 statusLabel() 重叠，断言限定在脚本面板内
		const scriptsPanel = screen.getByText('脚本').closest('section') as HTMLElement;
		expect(within(scriptsPanel).getAllByText('运行中')).toHaveLength(1);
		expect(within(scriptsPanel).getAllByText('已停止')).toHaveLength(2);
	});

	it('触发器列表渲染条件中文标签，未知类型归一化为 color_found', async () => {
		await connection.connect();
		triggers.set([
			{ id: '1', name: '血条', ...baseTrigger } as any,
			{ id: '2', name: '定时', ...baseTrigger, condition: { ...baseTrigger.condition, type: 'time_elapsed' } } as any,
			{ id: '3', name: '神秘', ...baseTrigger, enabled: false, condition: { ...baseTrigger.condition, type: 'mystery' } } as any,
		]);
		render(Page);
		// 未知类型经 normalizeConditionType 归一化为 color_found（血条 + 神秘 各一行）
		expect(screen.getAllByText('颜色找到 · 0 个动作')).toHaveLength(2);
		expect(screen.getByText('定时触发 · 0 个动作')).toBeInTheDocument();
		expect(screen.getByText('2/3')).toBeInTheDocument(); // 启用数/总数
		expect(screen.getByText('最近命中 0')).toBeInTheDocument();
		expect(screen.getByText('停用')).toBeInTheDocument();
	});

	it('激活配置显示名称、统计与热键标签', async () => {
		await connection.connect();
		profiles.loadDevData(); // 示例配置不带 scripts/triggers，仅验证概要绑定
		render(Page);
		expect(screen.getByText('默认配置')).toBeInTheDocument();
		expect(screen.getByText('0 个脚本')).toBeInTheDocument();
		expect(screen.getByText('0 个触发器')).toBeInTheDocument();
		expect(screen.getByText('启动 F5')).toBeInTheDocument();
		expect(screen.getByText('急停 F12')).toBeInTheDocument();
	});

	it('刷新按钮写日志并更新最近刷新时间', async () => {
		await connection.connect();
		render(Page);
		// mount 触发首次刷新，进行中的按钮显示「刷新中」，等待其回落
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '刷新' })).toBeInTheDocument();
		});
		await fireEvent.click(screen.getByRole('button', { name: '刷新' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.message === '仪表板已刷新')).toBe(true);
		});
	});

	it('管理入口导航到设置页', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '管理' }));
		expect(get(router.current)).toBe('settings');
	});

	it('已连接无配置：启动提示警告；有配置：四个快速操作成功路径', async () => {
		await connection.connect();
		render(Page);

		// 无激活配置
		await fireEvent.click(screen.getByRole('button', { name: '启动当前配置' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'warning' && e.message === '未选择当前配置')).toBe(true);
		});

		profiles.loadDevData();
		await fireEvent.click(screen.getByRole('button', { name: '启动当前配置' }));
		await fireEvent.click(screen.getByRole('button', { name: '停止当前配置' }));
		await fireEvent.click(screen.getByRole('button', { name: '停止全部' }));
		await waitFor(() => {
			const messages = get(logs).map(e => e.message);
			expect(messages).toContain('已启动当前配置脚本: 0');
			expect(messages).toContain('已停止当前配置脚本: 0');
			expect(messages).toContain('已停止全部脚本: 0');
		});
	});

	it('暂停全部切换状态并记录日志', async () => {
		await connection.connect();
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '暂停全部' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.message === '全部脚本已暂停')).toBe(true);
		});
		expect(get(connection).paused).toBe(true);
		expect(screen.getByRole('button', { name: '恢复全部' })).toBeInTheDocument();
	});
});
