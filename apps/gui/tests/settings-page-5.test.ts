import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/settings/+page.svelte';
import { connection } from '$lib/stores/connection';
import { settings } from '$lib/stores/settings';
import { logs } from '$lib/stores/logs';
import { profiles } from '$lib/stores/profiles';
import { installClipboardStub } from './helpers/test-utils';

/**
 * 设置页第五组：删除未编辑配置（editingProfile 复位守卫的 else 侧）、
 * 导出失败（exportToJson 返回 null）不写剪贴板不写日志。
 */

function messages(): string[] {
	return get(logs).map(e => e.message);
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	settings.update({ logLevel: 'info' });
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.disconnect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('设置页：删除未编辑的配置', () => {
	it('未打开编辑面板时删除配置，空态保持不变', async () => {
		vi.spyOn(window, 'confirm').mockReturnValue(true);
		profiles.loadDevData();
		render(Page);
		// 未选中任何配置：编辑面板为空态
		expect(screen.getByText('从左侧选择一个配置进行编辑')).toBeInTheDocument();

		const game1Card = screen.getByText('游戏配置 1').closest('.profile-card') as HTMLElement;
		await fireEvent.click(game1Card.querySelector('[title="删除"]')!);
		await waitFor(() => {
			expect(messages()).toContain('已删除配置: 游戏配置 1');
		});
		// editingProfile 本为 null：复位守卫走 else，面板仍为空态
		expect(screen.getByText('从左侧选择一个配置进行编辑')).toBeInTheDocument();
		expect(get(profiles).map(p => p.id)).toEqual(['default']);
	});
});

describe('设置页：导出失败', () => {
	it('exportToJson 返回 null 时不写剪贴板也不写日志', async () => {
		const clip = installClipboardStub();
		profiles.loadDevData();
		vi.spyOn(profiles, 'exportToJson').mockResolvedValue(null);
		render(Page);

		const defaultCard = screen.getByText('默认配置').closest('.profile-card') as HTMLElement;
		await fireEvent.click(defaultCard.querySelector('[title="导出"]')!);
		await new Promise(r => setTimeout(r, 50));
		expect(clip.written).toHaveLength(0);
		expect(messages()).not.toContain('配置已复制到剪贴板');
	});
});
