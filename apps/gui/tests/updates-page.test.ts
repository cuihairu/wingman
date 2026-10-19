import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/svelte';
import Page from '../src/routes/updates/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	await connection.disconnect();
});

describe('更新页面（dev 模式）', () => {
	it('未连接显示不可用提示', () => {
		render(Page);
		expect(screen.getByText('更新功能当前不可用')).toBeInTheDocument();
		// 状态徽标与「状态」指标卡同文，断言两处均出现
		expect(screen.getAllByText('runtime IPC 未连接').length).toBe(2);
		expect(screen.queryByRole('button', { name: '检查更新' })).not.toBeInTheDocument();
	});

	it('已连接但更新未启用时给出配置提示', async () => {
		await connection.connect();
		render(Page);
		// 状态徽标、「状态」指标卡与提示卡标题三处同文
		expect(screen.getAllByText('更新检测未启用').length).toBe(3);
		expect(screen.getByText('runtime 的 [update] 段未配置或已关闭。请确认 enabled 与 base_url 均已设置。')).toBeInTheDocument();
	});
});
