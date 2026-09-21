import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/settings/+page.svelte';
import { logs } from '$lib/stores/logs';
import { connection } from '$lib/stores/connection';

/**
 * 设置页：远程注册配置区块（config.getRemote / config.setRemote 的 GUI 入口）。
 * 前端校验 + Tauri command 调用 + 状态提示。
 */

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	localStorage.removeItem('wingman-settings');
	logs.clear();
	await connection.disconnect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

function remoteInputs(): HTMLInputElement[] {
	// 页面 inputs 顺序：0 IPC 端点 / 1 Orchestrator / 2 Server 地址 / 3 端口 / 4 令牌；
	// bind:value 监听 input 事件，输入用 fireEvent.input 而非 change
	const inputs = document.querySelectorAll<HTMLInputElement>('input.form-input');
	return [inputs[2], inputs[3], inputs[4]];
}

function installInvoke(handler: (cmd: string, args?: Record<string, unknown>) => unknown) {
	(window as any).__TAURI_INVOKE__ = vi.fn(handler);
}

describe('设置页：远程注册配置区块', () => {
	it('默认渲染：三个输入、读取与保存按钮、无状态提示', () => {
		render(Page);
		expect(screen.getByText('远程注册配置')).toBeInTheDocument();
		expect(screen.getByText('Server 地址')).toBeInTheDocument();
		expect(screen.getByText('端口')).toBeInTheDocument();
		expect(screen.getByText('注册令牌（A3-P1）')).toBeInTheDocument();
		expect(screen.getByRole('button', { name: '读取当前配置' })).toBeEnabled();
		expect(screen.getByRole('button', { name: '保存并应用' })).toBeEnabled();
		expect(screen.getByRole('button', { name: '显示' })).toBeInTheDocument();
		expect(screen.queryByText(/读取失败|已读取|保存失败|已应用/)).not.toBeInTheDocument();
	});

	it('未连接时读取与保存均提示 IPC 未连接', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '读取当前配置' }));
		expect(await screen.findByText('读取失败: 本地 IPC 未连接')).toBeInTheDocument();
		await fireEvent.click(screen.getByRole('button', { name: '保存并应用' }));
		expect(screen.getByText('保存失败: 本地 IPC 未连接')).toBeInTheDocument();
	});

	it('前端校验：空地址与非整数端口不发起调用', async () => {
		const invoke = vi.fn();
		installInvoke(invoke);
		render(Page);
		const [serverIp, serverPort] = remoteInputs();

		await fireEvent.click(screen.getByRole('button', { name: '保存并应用' }));
		expect(screen.getByText('Server 地址不能为空')).toBeInTheDocument();

		await fireEvent.input(serverIp, { target: { value: '10.0.0.8' } });
		await fireEvent.input(serverPort, { target: { value: '0' } });
		await fireEvent.click(screen.getByRole('button', { name: '保存并应用' }));
		expect(screen.getByText('端口必须为 1-65535 的整数')).toBeInTheDocument();

		expect(invoke).not.toHaveBeenCalled();
	});

	it('读取填充字段并提示成功', async () => {
		installInvoke((cmd) => {
			if (cmd === 'get_remote_config') {
				return { serverIp: '192.168.1.10', serverPort: 9527, registerToken: 'tok-123' };
			}
			throw new Error('unexpected ' + cmd);
		});
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '读取当前配置' }));

		await waitFor(() => {
			expect(screen.getByText('已读取 runtime 当前生效的远程配置')).toBeInTheDocument();
		});
		const [serverIp, serverPort, token] = remoteInputs();
		expect(serverIp.value).toBe('192.168.1.10');
		expect(serverPort.value).toBe('9527');
		expect(token.value).toBe('tok-123');
		expect(token).toHaveAttribute('type', 'password');
	});

	it('保存调用 set_remote_config 并按返回值刷新 + 写日志', async () => {
		const invoke = vi.fn((cmd: string, args?: Record<string, unknown>) => {
			if (cmd === 'set_remote_config') {
				expect(args).toEqual({
					serverIp: '10.0.0.8',
					serverPort: 9527,
					registerToken: 'new-tok',
				});
				return { serverIp: '10.0.0.8', serverPort: 9527, registerToken: 'new-tok' };
			}
			throw new Error('unexpected ' + cmd);
		});
		installInvoke(invoke);
		render(Page);
		const [serverIp, serverPort, token] = remoteInputs();
		await fireEvent.input(serverIp, { target: { value: ' 10.0.0.8 ' } });
		await fireEvent.input(serverPort, { target: { value: '9527' } });
		await fireEvent.input(token, { target: { value: 'new-tok' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存并应用' }));
		await waitFor(() => {
			expect(screen.getByText('已应用并写回 runtime 配置文件')).toBeInTheDocument();
		});
		expect(get(logs).map(e => e.message)).toContain('远程注册配置已更新: 10.0.0.8:9527');
		// 保存成功后字段按 runtime 返回的生效值刷新
		expect(serverIp.value).toBe('10.0.0.8');
		expect(invoke).toHaveBeenCalledTimes(1);
	});

	it('runtime 拒绝时展示错误并记日志', async () => {
		// Tauri command 的 Err(String) 在 JS 侧 reject 裸字符串（非 Error 实例）
		installInvoke((cmd) => {
			if (cmd === 'set_remote_config') return Promise.reject('serverPort 超出范围 (1-65535)');
			return Promise.reject('unexpected ' + cmd);
		});
		render(Page);
		const [serverIp, serverPort] = remoteInputs();
		await fireEvent.input(serverIp, { target: { value: '10.0.0.8' } });
		await fireEvent.input(serverPort, { target: { value: '80' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存并应用' }));
		await waitFor(() => {
			expect(screen.getByText('保存失败: serverPort 超出范围 (1-65535)')).toBeInTheDocument();
		});
		expect(get(logs).map(e => e.message)).toContain('远程注册配置保存失败: serverPort 超出范围 (1-65535)');
	});

	it('令牌显示/隐藏切换', async () => {
		installInvoke(() => { throw new Error('unreachable'); });
		render(Page);
		const token = remoteInputs()[2];
		expect(token).toHaveAttribute('type', 'password');
		await fireEvent.click(screen.getByRole('button', { name: '显示' }));
		expect(token).toHaveAttribute('type', 'text');
		expect(screen.getByRole('button', { name: '隐藏' })).toBeInTheDocument();
	});
});
