import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, within, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { scriptFiles } from '$lib/stores/script-files';

/**
 * scripts 页第四组：暂停/恢复/重启操作链、路径边界（无目录/空路径）、
 * error 状态样式、零大小脚本的 size 隐藏。
 * 批量操作与文件管理见 scripts-page-2 / scripts-page-3。
 */

function makeScript(id: string, state: ScriptInfo['state'], extra: Partial<ScriptInfo> = {}): ScriptInfo {
	return {
		id,
		name: `${id}.lua`,
		path: `scripts/${id}.lua`,
		size: 1024,
		is_running: state === 'running',
		state,
		error: '',
		loaded_at: 0,
		...extra,
	};
}

function itemOf(name: string): HTMLElement {
	return screen.getByText(name).closest('.script-item') as HTMLElement;
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

describe('scripts 页：暂停/恢复/重启链', () => {
	it('运行中脚本可暂停，暂停后可恢复与重启', async () => {
		scripts.set([makeScript('run', 'running')]);
		render(Page);

		// 暂停（success 日志 + 状态切换）
		await fireEvent.click(within(itemOf('run.lua')).getByRole('button', { name: '暂停' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已暂停脚本: run.lua')).toBe(true);
		});
		await waitFor(() => {
			expect(get(scripts)[0].state).toBe('paused');
		});

		// 恢复
		await fireEvent.click(within(itemOf('run.lua')).getByRole('button', { name: '恢复' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已恢复脚本: run.lua')).toBe(true);
		});

		// 重启（running 条目直接支持）
		await fireEvent.click(within(itemOf('run.lua')).getByRole('button', { name: '重启' }));
		await waitFor(() => {
			expect(get(logs).some(e => e.type === 'success' && e.message === '已重启脚本: run.lua')).toBe(true);
		});
		expect(get(scripts)[0].state).toBe('running');
	});
});

describe('scripts 页：路径与状态渲染边界', () => {
	it('无目录路径的父目录显示为当前目录（.）', async () => {
		scripts.set([{ ...makeScript('flat', 'stopped'), path: 'flat.lua' }]);
		render(Page);
		// parentPath('flat.lua') = '.' → meta 显示 ./flat.lua
		expect(screen.getByText('./flat.lua')).toBeInTheDocument();
	});

	it('空路径脚本显示 ./ 与 ID', async () => {
		scripts.set([{ ...makeScript('nop', 'stopped'), path: '', name: 'nop' }]);
		render(Page);
		expect(screen.getByText('./')).toBeInTheDocument();
		expect(screen.getByText('ID nop')).toBeInTheDocument();
	});

	it('error 状态脚本带 error 样式，size 为 0 时不显示体积', async () => {
		scripts.set([
			makeScript('bad', 'error', { error: 'boom', size: 0 }),
			makeScript('ok', 'stopped'),
		]);
		render(Page);

		const badItem = itemOf('bad.lua');
		expect(badItem.className).toContain('error');
		expect(within(badItem).getByText('boom')).toBeInTheDocument();
		// size 0：无体积文本；正常脚本显示 1.0 KB
		expect(within(badItem).queryByText(/KB/)).not.toBeInTheDocument();
		expect(within(itemOf('ok.lua')).getByText(/1\.0 KB/)).toBeInTheDocument();
	});
});
