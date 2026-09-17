import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor, cleanup } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { scriptFiles, type ScriptFileEntry } from '$lib/stores/script-files';

/**
 * scripts 页第六组：未加载脚本的时长占位（scriptUptime '-' 分支）、
 * 路径/新建输入框非 Enter 键盘事件、预览读取失败错误面板、
 * 删除非选中文件（selectedPath 保持）与空格键进入待确认。
 */

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

function makeEntry(path: string): ScriptFileEntry {
	return {
		path,
		name: path.split('/').pop()!,
		is_dir: false,
		size: 10,
		modified: Date.now(),
	} as ScriptFileEntry;
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
	// vitest globals 未开启时 RTL auto-cleanup 关闭，需显式清理
	// （残留实例会让 getByTitle 匹配到多个元素）
	cleanup();
	vi.restoreAllMocks();
});

describe('scripts 页：未加载脚本时长占位', () => {
	it('loaded_at 为 0 时 uptime 显示 -', () => {
		scripts.set([makeScript()]);
		render(Page);
		const uptime = document.querySelector('.uptime') as HTMLElement;
		expect(uptime.textContent).toBe('⏱ -');
	});
});

describe('scripts 页：键盘事件', () => {
	it('路径输入框按非 Enter 键不触发启动', async () => {
		render(Page);
		const input = screen.getByPlaceholderText('scripts/example.lua 或绝对路径');
		await fireEvent.input(input, { target: { value: 'scripts/x.lua' } });
		await fireEvent.keyDown(input, { key: 'Escape' });
		expect(get(logs).some(e => /已启动/.test(e.message))).toBe(false);
	});

	it('新建文件输入框按非 Enter 键不触发创建', async () => {
		render(Page);
		const input = screen.getByPlaceholderText('新建文件相对路径，如 scripts/new.lua');
		await fireEvent.input(input, { target: { value: 'scripts/n.lua' } });
		await fireEvent.keyDown(input, { key: 'Escape' });
		expect(get(logs).some(e => /已创建/.test(e.message))).toBe(false);
		// dev 模式条目由 load 后 upsert：Escape 不创建即列表不含新路径
		expect(screen.queryByTitle('scripts/n.lua')).toBeNull();
	});

	it('文件删除按钮按空格键进入待确认，确认后删除非选中文件不改变选中', async () => {
		render(Page);
		// 挂载时 dev load() 会覆盖注入，等其完成（文件行出现）后再注入自定义条目
		await waitFor(() => {
			expect(document.querySelector('button.file-row[title="scripts/example.lua"]')).toBeTruthy();
		});
		scriptFiles.set([makeEntry('scripts/a.lua'), makeEntry('scripts/b.lua')]);
		const rowB = await waitFor(() => {
			const el = document.querySelector('button.file-row[title="scripts/b.lua"]');
			expect(el).toBeTruthy();
			return el as HTMLElement;
		});
		// 选中 b → 预览工具栏出现
		await fireEvent.click(rowB);
		await waitFor(() => {
			expect(document.querySelector('.preview-toolbar')).toBeTruthy();
		});

		// 对 a 的删除按钮按空格 → 两段式确认第一步
		const delA = document.querySelector('button.file-row[title="scripts/a.lua"]')!.querySelector('[title="删除文件"]') as HTMLElement;
		await fireEvent.keyDown(delA, { key: ' ' });
		expect(delA.getAttribute('title')).toBe('再次点击确认删除');

		// 再点击确认删除 a：a 非当前选中（selectedPath === 'scripts/b.lua'）→ 选中保持
		await fireEvent.click(delA);
		await waitFor(() => {
			expect(get(logs).some(e => e.message === '已删除脚本文件: scripts/a.lua')).toBe(true);
		});
		expect(document.querySelector('button.file-row[title="scripts/b.lua"]')).toBeTruthy();
		expect(document.querySelector('.preview-toolbar')).toBeTruthy();
	});
});

describe('scripts 页：预览读取失败', () => {
	it('read 抛错时显示错误面板', async () => {
		render(Page);
		await waitFor(() => {
			expect(document.querySelector('button.file-row[title="scripts/example.lua"]')).toBeTruthy();
		});
		scriptFiles.set([makeEntry('scripts/bad.lua')]);
		await waitFor(() => {
			expect(document.querySelector('button.file-row[title="scripts/bad.lua"]')).toBeTruthy();
		});
		vi.spyOn(scriptFiles, 'read').mockRejectedValue(new Error('磁盘错误'));
		await fireEvent.click(document.querySelector('button.file-row[title="scripts/bad.lua"]')!);
		await waitFor(() => {
			expect(document.querySelector('.preview-error')?.textContent).toContain('磁盘错误');
		});
	});
});

describe('scripts 页：删除按钮非确认键', () => {
	it('按非 Enter/空格键不进入待确认', async () => {
		render(Page);
		await waitFor(() => {
			expect(document.querySelector('button.file-row[title="scripts/example.lua"]')).toBeTruthy();
		});
		const del = document.querySelector('button.file-row[title="scripts/example.lua"]')!
			.querySelector('[title="删除文件"]') as HTMLElement;
		await fireEvent.keyDown(del, { key: 'Escape' });
		expect(del.getAttribute('title')).toBe('删除文件');
	});
});
