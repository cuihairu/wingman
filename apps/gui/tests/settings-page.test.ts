import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/settings/+page.svelte';
import { connection } from '$lib/stores/connection';
import { settings } from '$lib/stores/settings';
import { profiles, activeProfile } from '$lib/stores/profiles';
import { logs } from '$lib/stores/logs';
import { installClipboardStub } from './helpers/test-utils';

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	localStorage.removeItem('wingman-settings');
	logs.clear();
	await connection.disconnect();
	// 清空 profiles 单例（无 set，逐个 remove）
	for (const p of get(profiles)) await profiles.remove(p.id);
	profiles.activeId.set('');
});

afterEach(() => {
	vi.restoreAllMocks();
});

function messages(): string[] {
	return get(logs).map(e => e.message);
}

describe('设置页：连接区块', () => {
	it('未连接默认渲染与连接信息', () => {
		const { container } = render(Page);
		expect(screen.getByText('连接设置')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '连接' })).toBeEnabled();
		const status = container.querySelector('.ipc-meta strong')!;
		expect(status.textContent).toBe('未连接');
		expect(status).toHaveClass('ipc-bad');
		expect(screen.getByText('runtime 视角: 未知（IPC 未连接）')).toBeInTheDocument();
		expect(screen.getByText('远程链路: 未知（等待状态同步）')).toBeInTheDocument();
		// 非 error/reconnecting 无立即重试
		expect(screen.queryByRole('button', { name: '立即重试' })).not.toBeInTheDocument();
		// 无错误消息
		expect(container.querySelector('.ipc-error')).toBeNull();
		expect(screen.getByText('暂无配置，点击"新建配置"创建')).toBeInTheDocument();
		expect(screen.getByText('从左侧选择一个配置进行编辑')).toBeInTheDocument();
	});

	it('连接与断开切换并写日志', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '连接' }));
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '断开' })).toBeInTheDocument();
		});
		expect(messages()).toContain('已连接到本地 runtime IPC');

		await fireEvent.click(screen.getByRole('button', { name: '断开' }));
		await waitFor(() => {
			expect(screen.getByRole('button', { name: '连接' })).toBeInTheDocument();
		});
		expect(messages()).toContain('已断开连接');
	});

	it('端点输入同步 settings 并持久化 localStorage', async () => {
		render(Page);
		// 表单标签是 span 而非 label 元素，直接按 class 定位文本输入
		//（0 端点 / 1 Orchestrator / 2-4 远程注册卡片的地址、端口、令牌）
		const inputs = document.querySelectorAll<HTMLInputElement>('input.form-input');
		expect(inputs.length).toBe(5);
		await fireEvent.change(inputs[0], { target: { value: 'custom_pipe' } });
		expect(get(settings).ipcEndpoint).toBe('custom_pipe');
		expect(JSON.parse(localStorage.getItem('wingman-settings')!).ipcEndpoint).toBe('custom_pipe');

		// Orchestrator 地址：空值回落默认
		await fireEvent.change(inputs[1], { target: { value: '  ' } });
		expect(get(settings).orchestratorUrl).toBe('http://localhost:9527');
		await fireEvent.change(inputs[1], { target: { value: 'http://10.0.0.8:9527' } });
		expect(get(settings).orchestratorUrl).toBe('http://10.0.0.8:9527');
	});

	it('应用选项切换与恢复默认', async () => {
		render(Page);
		const checkboxes = screen.getAllByRole('checkbox');
		expect(checkboxes).toHaveLength(3); // 最小化 / 自动重连 / 自动启动
		await fireEvent.click(checkboxes[0]); // minimizeOnStart → true
		await fireEvent.click(checkboxes[1]); // autoReconnect → false
		await fireEvent.click(checkboxes[2]); // autoStart → true

		const level = screen.getByDisplayValue('info');
		await fireEvent.change(level, { target: { value: 'debug' } });

		expect(get(settings)).toMatchObject({ minimizeOnStart: true, autoReconnect: false, autoStart: true, logLevel: 'debug' });

		await fireEvent.click(screen.getByRole('button', { name: '恢复默认' }));
		expect(get(settings)).toMatchObject({ minimizeOnStart: false, autoReconnect: true, autoStart: false, logLevel: 'info', ipcEndpoint: 'wingman' });
		expect(messages()).toContain('已恢复默认应用设置');
	});
});

describe('设置页：配置管理', () => {
	it('新建配置：空名不创建，取消关闭，成功创建并进入编辑', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '新建配置' }));
		expect(screen.getByPlaceholderText('配置名称')).toBeInTheDocument();

		// 空名创建被忽略
		await fireEvent.click(screen.getByRole('button', { name: '创建' }));
		expect(get(profiles)).toHaveLength(0);

		// 取消
		await fireEvent.click(screen.getByRole('button', { name: '取消' }));
		expect(screen.queryByPlaceholderText('配置名称')).not.toBeInTheDocument();

		// 输入名称创建
		await fireEvent.click(screen.getByRole('button', { name: '新建配置' }));
		await fireEvent.input(screen.getByPlaceholderText('配置名称'), { target: { value: '我的配置' } });
		await fireEvent.click(screen.getByRole('button', { name: '创建' }));
		expect(get(profiles)).toHaveLength(1);
		expect(messages()).toContain('已创建配置: 我的配置');
		expect(screen.getByText('编辑: 我的配置')).toBeInTheDocument();
		expect(screen.queryByPlaceholderText('配置名称')).not.toBeInTheDocument();
	});

	it('编辑面板渲染四个资源分区与空提示', async () => {
		profiles.loadDevData();
		render(Page);
		// 选中带颜色的 game1
		await fireEvent.click(screen.getByText('游戏配置 1'));

		expect(screen.getByText('颜色配置 (1)')).toBeInTheDocument();
		expect(screen.getByText('HP')).toBeInTheDocument();
		expect(screen.getByText('容差 10')).toBeInTheDocument();
		expect(screen.getByText('暂无图像配置')).toBeInTheDocument();
		expect(screen.getByText('暂无触发器')).toBeInTheDocument();
		expect(screen.getByText('暂无脚本')).toBeInTheDocument();
		// 窗口绑定字段回填
		expect(screen.getByDisplayValue('MyGame')).toBeInTheDocument();
		expect(screen.getByDisplayValue('mygame.exe')).toBeInTheDocument();
	});

	it('编辑字段与保存（含热键大小写与去空白）', async () => {
		profiles.loadDevData();
		render(Page);
		await fireEvent.click(screen.getByText('游戏配置 1'));

		const inputs = document.querySelectorAll<HTMLInputElement>('.editor-form input.form-input');
		// 0 名称 / 1 描述 / 2 窗口标题 / 3 进程名
		await fireEvent.input(inputs[0], { target: { value: '改名游戏' } });
		await fireEvent.input(inputs[1], { target: { value: '新描述' } });
		await fireEvent.input(inputs[2], { target: { value: 'GameWindow' } });

		// 热键输入：带空白与小写
		const hotkeyInputs = [...document.querySelectorAll<HTMLInputElement>('.editor-form .form-group')]
			.filter(g => ['启动', '停止', '暂停/恢复', '急停'].includes(g.querySelector('.form-label')!.textContent!))
			.map(g => g.querySelector('input')!);
		await fireEvent.input(hotkeyInputs[0], { target: { value: ' f5 , ctrl+shift+p ' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存', exact: true }));
		await waitFor(() => {
			expect(messages()).toContain('已保存配置: 改名游戏');
		});
		const saved = get(profiles).find(p => p.name === '改名游戏')!;
		expect(saved.description).toBe('新描述');
		expect(saved.window.title).toBe('GameWindow');
		expect(saved.hotkeys.start).toEqual(['F5', 'CTRL+SHIFT+P']);
	});

	it('激活配置显示当前徽标', async () => {
		profiles.loadDevData();
		render(Page);

		const game1Card = screen.getByText('游戏配置 1').closest('.profile-card')!;
		await fireEvent.click(game1Card.querySelector('[title="激活"]')!);
		expect(get(activeProfile)?.id).toBe('game1');
		expect(game1Card.querySelector('.active-badge')?.textContent).toBe('当前');
		expect(game1Card.querySelector('[title="激活"]')).toBeNull();
	});

	it('导出到剪贴板并从剪贴板导入', async () => {
		const clip = installClipboardStub();
		profiles.loadDevData();
		render(Page);

		// 导出默认配置
		const defaultCard = screen.getByText('默认配置').closest('.profile-card')!;
		await fireEvent.click(defaultCard.querySelector('[title="导出"]')!);
		await waitFor(() => {
			expect(clip.written).toHaveLength(1);
		});
		const exported = JSON.parse(clip.written[0]);
		expect(exported.id).toBe('default');
		expect(messages()).toContain('配置已复制到剪贴板');

		// 导入：合法 JSON（改 id 与名称）→ 成功
		clip.setNextRead(JSON.stringify({ ...exported, id: 'imported', name: '导入配置' }));
		await fireEvent.click(screen.getByRole('button', { name: '导入' }));
		await waitFor(() => {
			expect(messages()).toContain('导入成功');
		});
		expect(get(profiles).some(p => p.id === 'imported' && p.name === '导入配置')).toBe(true);

		// 导入非法 JSON → 失败
		clip.setNextRead('{broken json');
		await fireEvent.click(screen.getByRole('button', { name: '导入' }));
		await waitFor(() => {
			expect(messages()).toContain('导入失败');
		});
	});

	it('删除配置：确认与取消两条路径，编辑面板联动复位', async () => {
		const confirmSpy = vi.spyOn(window, 'confirm');
		profiles.loadDevData();
		render(Page);

		// 选中默认配置再删除 → 编辑面板复位
		await fireEvent.click(screen.getByText('默认配置'));
		expect(screen.getByText('编辑: 默认配置')).toBeInTheDocument();

		const defaultCard = screen.getByText('默认配置').closest('.profile-card')!;
		const delBtn = defaultCard.querySelector('[title="删除"]')!;

		// 取消
		confirmSpy.mockReturnValue(false);
		await fireEvent.click(delBtn);
		expect(confirmSpy).toHaveBeenCalledWith(expect.stringContaining('默认配置'));
		expect(get(profiles)).toHaveLength(2);
		expect(screen.getByText('编辑: 默认配置')).toBeInTheDocument();

		// 确认
		confirmSpy.mockReturnValue(true);
		await fireEvent.click(delBtn);
		expect(get(profiles).map(p => p.id)).toEqual(['game1']);
		expect(messages()).toContain('已删除配置: 默认配置');
		expect(screen.getByText('从左侧选择一个配置进行编辑')).toBeInTheDocument();
	});
});
