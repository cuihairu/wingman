import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { get } from 'svelte/store';
import Page from '../src/routes/macros/+page.svelte';
import { connection } from '$lib/stores/connection';
import { macros } from '$lib/stores/macros';
import { logs } from '$lib/stores/logs';

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	await connection.disconnect();
});

describe('宏录制页面（dev 模式）', () => {
	it('未连接显示不可用提示', () => {
		render(Page);
		expect(screen.getByText('宏功能当前不可用')).toBeInTheDocument();
		expect(screen.getByText('runtime IPC 未连接')).toBeInTheDocument();
		expect(screen.queryByRole('button', { name: '开始录制' })).not.toBeInTheDocument();
	});

	it('连接后渲染指标卡与空闲状态', async () => {
		await connection.connect();
		render(Page);
		expect(screen.getByText('准备就绪')).toBeInTheDocument();
		expect(screen.getByText('录制控制')).toBeInTheDocument();
		expect(screen.getByText('回放参数')).toBeInTheDocument();
		expect(screen.getByText('保存与载入')).toBeInTheDocument();
		// 指标：已捕获 0、速度 100%、重复 1、状态 空闲
		expect(screen.getByText('0')).toBeInTheDocument();
		expect(screen.getByText('100%')).toBeInTheDocument();
		expect(screen.getByText('空闲')).toBeInTheDocument();
	});

	it('dev 模式录制为 no-op，保存/载入路径为空时提示', async () => {
		await connection.connect();
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: '开始录制' }));
		expect(screen.getByText('当前未录制')).toBeInTheDocument(); // no-op 后状态不变

		await fireEvent.click(screen.getByRole('button', { name: '保存为 JSON' }));
		await fireEvent.click(screen.getByRole('button', { name: '载入宏文件' }));
		const messages = get(logs).map(e => e.message);
		expect(messages).toContain('请输入保存路径');
		expect(messages).toContain('请输入载入路径');
		expect(messages.every((m, i) => get(logs)[i].type === 'warning')).toBe(true);
	});

	it('dev 模式清空会话按钮在无事件时禁用', async () => {
		await connection.connect();
		render(Page);
		expect(screen.getByRole('button', { name: '清空会话' })).toBeDisabled();
	});
});
