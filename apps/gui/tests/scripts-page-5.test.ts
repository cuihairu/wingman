import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { scriptFiles, type ScriptFileEntry } from '$lib/stores/script-files';

/** scripts 页第五组：加载时长显示、无名脚本的回退、批量停止无目标、目录条目交互、未选中运行。 */

function makeScript(extra: Partial<ScriptInfo> = {}): ScriptInfo {
	return {
		id: 's1',
		name: 'farm.lua',
		path: 'scripts/farm.lua',
		size: 1024,
		is_running: false,
		state: 'stopped',
		error: '',
		loaded_at: 0,
		...extra,
	} as ScriptInfo;
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	scripts.set([]);
	logs.clear();
	await scriptFiles.setRoot('');
	scriptFiles.set([]);
	await connection.connect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('scripts 页：加载时长与无名回退', () => {
	it('loaded_at 有值时显示运行时长', async () => {
		scripts.set([makeScript({ state: 'running', is_running: true, loaded_at: Date.now() - 5000 })]);
		render(Page);
		const uptime = document.querySelector('.uptime') as HTMLElement;
		await waitFor(() => {
			expect(uptime.textContent).toContain('⏱');
		});
		expect(uptime.textContent).not.toContain('-');
		expect(uptime.getAttribute('title')).toBe('自最近一次加载起的时长');
	});

	it('name 与 path 均为空时缩写与标题回退 id', async () => {
		scripts.set([makeScript({ id: 'ghost', name: '', path: '' })]);
		render(Page);
		// 缩写 fileName(name||path) 均空 → 空串；标题回退 fileName('')
		expect(document.querySelector('.file-badge')?.textContent?.trim()).toBe('');
		expect(screen.getByText('ID ghost')).toBeInTheDocument();
	});
});

describe('scripts 页：批量操作无目标', () => {
	it('全部停止时无运行/暂停脚本写警告日志', async () => {
		scripts.set([makeScript({ state: 'stopped' })]);
		render(Page);
		const stopBtn = screen.getByRole('button', { name: /全部停止/ });
		await fireEvent.click(stopBtn);
		await waitFor(() => {
			expect(get(logs).some(e => e.message === '没有符合条件的脚本' && e.type === 'warning')).toBe(true);
		});
	});
});

describe('scripts 页：目录条目', () => {
	it('目录条目不显示大小并可点击不选中', async () => {
		const dirEntry = { path: 'scripts', name: 'scripts', is_dir: true, size: 0, modified: Date.now() } as ScriptFileEntry;
		scriptFiles.set([dirEntry]);
		render(Page);
		await waitFor(() => {
			expect(screen.getByText('scripts')).toBeInTheDocument();
		});
		const row = screen.getByText('scripts').closest('button') as HTMLButtonElement;
		// 目录条目 file-meta 只显示修改时间，不显示大小
		expect(row.querySelector('.file-meta')?.textContent).not.toMatch(/\d+(\.\d+)?\s*(B|KB)/);
		// disabled 按钮上派发点击事件仍进入 handler：is_dir 早退，不产生选中
		await fireEvent.click(row);
		await new Promise(r => setTimeout(r, 50));
		expect(row.getAttribute('aria-pressed')).toBeNull();
	});
});

describe('scripts 页：未选中运行', () => {
	it('未选中文件时预览面板显示空态、无启动按钮', async () => {
		scripts.set([]);
		render(Page);
		expect(screen.getByText('未选择文件')).toBeInTheDocument();
		expect(document.querySelector('.preview-toolbar')).toBeNull();
	});
});
