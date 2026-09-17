import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import { logs } from '$lib/stores/logs';

/** 触发器页第六组（invoke 模式）：add_trigger 失败回退本地 id、无名脚本操作的 id 回退。 */

beforeEach(() => {
	vi.resetModules();
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
});

afterEach(() => {
	vi.restoreAllMocks();
	delete (window as any).__TAURI_INVOKE__;
	document.body.innerHTML = '';
});

describe('触发器页：新建失败的本地回退（invoke 模式）', () => {
	it('add_trigger 失败时用本地 fallbackId 进入空编辑态', async () => {
		(window as any).__TAURI_INVOKE__ = vi.fn((cmd: string) => {
			if (cmd === 'add_trigger') throw new Error('trigger quota');
			if (cmd === 'get_triggers') return [];
			return {};
		});
		const pageMod = await import('../src/routes/triggers/+page.svelte');
		const logsMod = await import('$lib/stores/logs');
		const testingLib = await import('@testing-library/svelte');
		testingLib.render(pageMod.default as any);
		await testingLib.fireEvent.click(testingLib.screen.getByRole('button', { name: '添加触发器' }));
		// add 失败返回 '' → || fallbackId → selectTrigger(不在列表) → 空编辑态
		await testingLib.waitFor(() => {
			expect(testingLib.screen.getByText('选择一个触发器')).toBeInTheDocument();
		});
		await testingLib.waitFor(() => {
			expect(get(logsMod.logs).some(e => e.message === '已创建触发器')).toBe(true);
		});
	});
});

describe('scripts 页：无名脚本操作日志', () => {
	it('name 与 path 均空的脚本暂停日志回退 id', async () => {
		delete (window as any).__TAURI_INVOKE__;
		const pageMod = await import('../src/routes/scripts/+page.svelte');
		const logsMod = await import('$lib/stores/logs');
		const scriptsMod = await import('$lib/stores/scripts');
		const connMod = await import('$lib/stores/connection');
		const testingLib = await import('@testing-library/svelte');
		await connMod.connection.connect();
		scriptsMod.scripts.set([{ id: 'ghost', name: '', path: '', state: 'running', is_running: true, size: 0, error: '', loaded_at: 0 } as any]);
		testingLib.render(pageMod.default as any);
		await testingLib.waitFor(() => {
			expect(testingLib.screen.getByText('ID ghost')).toBeInTheDocument();
		});
		const item = testingLib.screen.getByText('ID ghost').closest('.script-item') as HTMLElement;
		const pauseBtn = [...item.querySelectorAll('button')].find(b => /暂停/.test(b.textContent || b.title || '')) as HTMLButtonElement;
		await testingLib.fireEvent.click(pauseBtn);
		await testingLib.waitFor(() => {
			expect(get(logsMod.logs).some(e => e.message === '已暂停脚本: ghost')).toBe(true);
		});
	});
});
