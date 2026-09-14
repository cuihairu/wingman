import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Sidebar from '../src/lib/components/layout/Sidebar.svelte';
import TopBar from '../src/lib/components/layout/TopBar.svelte';
import { router } from '$lib/router.svelte';
import { connection } from '$lib/stores/connection';
import { profiles } from '$lib/stores/profiles';
import { logs } from '$lib/stores/logs';
import { theme, toggleTheme } from '$lib/stores/theme';

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	delete (window as any).__TAURI_INTERNALS__;
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	logs.clear();
	router.navigate('dashboard');
	// 复位 dev 连接状态（connection 是模块级单例）
	await connection.disconnect();
	// profiles 未暴露 set，逐个删除清空列表
	for (const p of get(profiles)) {
		await profiles.remove(p.id);
	}
});

/// invoke 模式 TopBar 测试见 tests/topbar-invoke.test.ts
/// （需要整体重置模块注册表，与 Svelte 运行时单实例冲突）

describe('Sidebar', () => {
	it('渲染全部导航项与托盘按钮', () => {
		render(Sidebar);
		for (const label of ['仪表板', '脚本管理', '屏幕预览', '日志', '触发器', '宏录制', '设置']) {
			expect(screen.getByTitle(label)).toBeInTheDocument();
		}
		expect(screen.getByTitle('最小化到托盘')).toBeInTheDocument();
	});

	it('点击导航项更新路由并标记 active/aria-current', async () => {
		render(Sidebar);
		const scriptsBtn = screen.getByTitle('脚本管理');
		expect(scriptsBtn).not.toHaveClass('active');
		expect(scriptsBtn).not.toHaveAttribute('aria-current');

		await fireEvent.click(scriptsBtn);
		expect(get(router.current)).toBe('scripts');
		expect(scriptsBtn).toHaveClass('active');
		expect(scriptsBtn).toHaveAttribute('aria-current', 'page');
		// 其余项不带 active
		expect(screen.getByTitle('仪表板')).not.toHaveClass('active');
	});

	it('dev 模式点击托盘按钮记录提示日志', async () => {
		render(Sidebar);
		await fireEvent.click(screen.getByTitle('最小化到托盘'));
		expect(get(logs).some(e => e.message === '开发模式下无法最小化到托盘')).toBe(true);
	});

	it('invoke 模式点击托盘按钮调用窗口 hide', async () => {
		const internalsInvoke = vi.fn(async () => null);
		(window as any).__TAURI_INTERNALS__ = {
			metadata: { currentWindow: { label: 'main' } },
			invoke: internalsInvoke,
		};
		(window as any).__TAURI_INVOKE__ = vi.fn();
		render(Sidebar);
		await fireEvent.click(screen.getByTitle('最小化到托盘'));
		await waitFor(() => {
			expect(internalsInvoke).toHaveBeenCalledWith('plugin:window|hide', { label: 'main' }, undefined);
		});
		expect(get(logs)).toHaveLength(0);
	});

	it('invoke 模式 hide 失败记录错误日志', async () => {
		const internalsInvoke = vi.fn(async () => {
			throw new Error('boom');
		});
		(window as any).__TAURI_INTERNALS__ = {
			metadata: { currentWindow: { label: 'main' } },
			invoke: internalsInvoke,
		};
		(window as any).__TAURI_INVOKE__ = vi.fn();
		render(Sidebar);
		await fireEvent.click(screen.getByTitle('最小化到托盘'));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'error' && e.message.includes('最小化到托盘失败'))).toBe(true);
		});
	});
});

describe('TopBar（dev 模式）', () => {
	it('未连接时显示状态与版本，无操作按钮', () => {
		render(TopBar);
		expect(screen.getByText('未连接')).toBeInTheDocument();
		expect(screen.getByText('-')).toBeInTheDocument(); // 默认版本
		expect(screen.getByText('选择配置')).toBeInTheDocument();
		expect(screen.queryByTitle('急停')).not.toBeInTheDocument();
		expect(screen.queryByTitle('暂停全部')).not.toBeInTheDocument();
		// 主题与刷新按钮始终存在
		expect(screen.getByTitle('切换到亮色')).toBeInTheDocument();
		expect(screen.getByTitle('刷新')).toBeInTheDocument();
	});

	it('已连接后显示操作按钮；暂停切换文案与图标', async () => {
		await connection.connect();
		render(TopBar);
		expect(screen.getByText('已连接')).toBeInTheDocument();
		expect(screen.getByTitle('急停')).toBeInTheDocument();

		expect(screen.getByTitle('暂停全部')).toBeInTheDocument();
		await fireEvent.click(screen.getByTitle('暂停全部'));
		expect(get(connection).paused).toBe(true);
		expect(screen.getByTitle('恢复运行')).toBeInTheDocument();
	});

	it('急停/启动配置/停止配置写日志', async () => {
		await connection.connect();
		render(TopBar);
		await fireEvent.click(screen.getByTitle('急停'));
		await fireEvent.click(screen.getByTitle('启动当前配置'));
		await fireEvent.click(screen.getByTitle('停止当前配置'));
		const messages = get(logs).map(e => e.message);
		expect(messages).toContain('已急停脚本: 0');
		expect(messages).toContain('已启动当前配置脚本: 0');
		expect(messages).toContain('已停止当前配置脚本: 0');
		const types = get(logs).map(e => e.type);
		expect(types).toEqual(['warning', 'success', 'info']);
	});

	it('刷新按钮仅在已连接时调用 refresh', async () => {
		render(TopBar);
		const refreshSpy = vi.spyOn(connection, 'refresh');
		await fireEvent.click(screen.getByTitle('刷新'));
		expect(refreshSpy).not.toHaveBeenCalled();

		await connection.connect();
		await fireEvent.click(screen.getByTitle('刷新'));
		expect(refreshSpy).toHaveBeenCalledTimes(1);
	});

	it('远程徽标按状态渲染文案', async () => {
		await connection.connect();
		const { container } = render(TopBar);

		connection.setRemoteState('connected', '');
		await waitFor(() => {
			expect(screen.getByText('远程在线')).toBeInTheDocument();
		});
		expect(container.querySelector('.remote-connected')).toBeInTheDocument();

		connection.setRemoteState('reconnecting', 'heartbeat lost');
		await waitFor(() => {
			expect(screen.getByText('远程重连')).toBeInTheDocument();
		});
		expect(container.querySelector('.remote-reconnecting')).toBeInTheDocument();

		connection.setRemoteState('error', 'boom');
		await waitFor(() => {
			expect(screen.getByText('远程异常')).toBeInTheDocument();
		});

		connection.setRemoteState('disconnected', '');
		await waitFor(() => {
			expect(screen.getByText('远程离线')).toBeInTheDocument();
		});
	});

	it('配置下拉：空列表、选择与管理入口', async () => {
		profiles.loadDevData();
		render(TopBar);

		// 初始显示激活配置名
		expect(screen.getByText('默认配置')).toBeInTheDocument();

		await fireEvent.click(screen.getByRole('button', { name: /默认配置/ }));
		expect(screen.getByText('管理配置...')).toBeInTheDocument();
		expect(screen.getByText('游戏配置 1')).toBeInTheDocument();

		await fireEvent.click(screen.getByText('游戏配置 1'));
		expect(get(profiles.activeId)).toBe('game1');
		expect(screen.queryByText('管理配置...')).not.toBeInTheDocument(); // 选择后收起

		await fireEvent.click(screen.getByRole('button', { name: /游戏配置 1/ }));
		await fireEvent.click(screen.getByText('管理配置...'));
		expect(get(router.current)).toBe('settings');
	});

	it('空配置列表显示占位', async () => {
		render(TopBar);
		await fireEvent.click(screen.getByRole('button', { name: /选择配置/ }));
		expect(screen.getByText('暂无配置')).toBeInTheDocument();
	});

	it('主题切换按钮翻转主题', async () => {
		render(TopBar);
		expect(get(theme)).toBe('dark');
		await fireEvent.click(screen.getByTitle('切换到亮色'));
		expect(get(theme)).toBe('light');
		expect(screen.getByTitle('切换到暗色')).toBeInTheDocument();
		// 直接调用一次保证 toggleTheme 覆盖
		toggleTheme();
		expect(get(theme)).toBe('dark');
	});
});

