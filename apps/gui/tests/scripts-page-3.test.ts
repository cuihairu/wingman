import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, within, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/scripts/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { scripts, type ScriptInfo } from '$lib/stores/scripts';
import { scriptFiles } from '$lib/stores/script-files';

/**
 * scripts 页第三组：运行时长秒档与零值、列表启动/卸载操作、目录条目点击守卫。
 * 运行时长档位/批量操作/文件管理见 scripts-page-2.test.ts。
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

function logMessages(): string[] {
	return get(logs).map(e => e.message);
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

describe('scripts 页：运行时长边界', () => {
	it('秒档显示 Ns，loaded_at 为 0 时不渲染时长徽标', async () => {
		scripts.set([
			makeScript('secs', 'running', { loaded_at: Date.now() - 5_000 }),
			makeScript('zero', 'running', { loaded_at: 0 }),
		]);
		render(Page);

		const secsItem = itemOf('secs.lua');
		const zeroItem = itemOf('zero.lua');
		// 5s 内（页面 nowTick 每秒推进，边界内取整为 4-5s）
		expect(within(secsItem).getByText(/^⏱ /).textContent).toMatch(/^⏱ [1-9]s$/);
		// loaded_at=0 → {#if script.loaded_at} 守卫：徽标不渲染
		expect(within(zeroItem).queryByText(/^⏱ /)).not.toBeInTheDocument();
	});
});

describe('scripts 页：单条目操作', () => {
	it('停止条目的启动操作成功并停止条目的卸载操作', async () => {
		scripts.set([makeScript('idle', 'stopped')]);
		render(Page);

		await fireEvent.click(within(itemOf('idle.lua')).getByRole('button', { name: '启动' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '已启动脚本: idle.lua')).toBe(true);
		});
		expect(get(scripts)[0].state).toBe('running');

		// 运行中条目支持停止后卸载
		await fireEvent.click(within(itemOf('idle.lua')).getByRole('button', { name: '停止' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '已停止脚本: idle.lua')).toBe(true);
		});
		await fireEvent.click(within(itemOf('idle.lua')).getByRole('button', { name: '卸载' }));
		await waitFor(() => {
			expect(logMessages().some(m => m === '已卸载脚本: idle.lua')).toBe(true);
		});
		expect(get(scripts)).toHaveLength(0);
	});
});

describe('scripts 页：文件管理守卫', () => {
	it('目录条目点击不进入预览', async () => {
		render(Page);
		scriptFiles.set([
			{ path: 'pack', name: 'pack', is_dir: true, size: 0, modified: Date.now() },
			{ path: 'pack/inner.lua', name: 'inner.lua', is_dir: false, size: 10, modified: Date.now() },
		]);
		await waitFor(() => {
			expect(screen.getByText('pack', { selector: '.file-name' })).toBeInTheDocument();
		});

		await fireEvent.click(screen.getByText('pack', { selector: '.file-name' }));
		await new Promise(r => setTimeout(r, 50));
		// 目录不触发读取：预览仍为空态
		expect(screen.getByText('未选择文件')).toBeInTheDocument();
	});
});
