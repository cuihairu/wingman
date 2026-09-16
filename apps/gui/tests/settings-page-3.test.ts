import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import Page from '../src/routes/settings/+page.svelte';
import { connection } from '$lib/stores/connection';
import { settings } from '$lib/stores/settings';
import { logs } from '$lib/stores/logs';
import { profiles, type GameProfile } from '$lib/stores/profiles';
import { installClipboardStub } from './helpers/test-utils';

/**
 * 设置页第三组：热键全字段编辑、新建配置对话框（Enter 提交）、剪贴板导入空内容守卫。
 * 编辑面板/应用设置见 settings-page-2，invoke 状态分支见 settings-invoke。
 */

function fullProfile(id: string): GameProfile {
	return {
		id,
		name: `配置${id}`,
		version: '1.0',
		description: '',
		window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
		colors: [], images: [], triggers: [],
		scripts: [],
		hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
		settings: {},
	};
}

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.connect();
});

afterEach(() => {
	vi.restoreAllMocks();
});

describe('设置页：热键编辑', () => {
	it('启动/停止/暂停/急停四个热键字段编辑后保存', async () => {
		await profiles.importFromJson(JSON.stringify(fullProfile('p1')));
		render(Page);
		await fireEvent.click(screen.getByText('配置p1'));
		await waitFor(() => {
			expect(screen.getByText('编辑: 配置p1')).toBeInTheDocument();
		});

		// 四个热键输入框（placeholder F5/F6/F7/F12 定位）
		await fireEvent.input(screen.getByPlaceholderText('F5'), { target: { value: 'f8' } });
		await fireEvent.input(screen.getByPlaceholderText('F6'), { target: { value: 'f9' } });
		await fireEvent.input(screen.getByPlaceholderText('F7'), { target: { value: 'f10' } });
		await fireEvent.input(screen.getByPlaceholderText('F12'), { target: { value: 'f11' } });

		await fireEvent.click(screen.getByRole('button', { name: '保存' }));
		await waitFor(() => {
			const saved = get(profiles)[0];
			expect(saved.hotkeys.start).toEqual(['F8']);
			expect(saved.hotkeys.stop).toEqual(['F9']);
			expect(saved.hotkeys.pause).toEqual(['F10']);
			expect(saved.hotkeys.emergencyStop).toEqual(['F11']);
		});
	});
});

describe('设置页：新建与导入边界', () => {
	it('新建对话框 Enter 提交创建配置', async () => {
		render(Page);
		await fireEvent.click(screen.getByRole('button', { name: '新建配置' }));
		const input = screen.getByPlaceholderText('配置名称');
		await fireEvent.input(input, { target: { value: '回车建配置' } });
		await fireEvent.keyDown(input, { key: 'Enter' });

		await waitFor(() => {
			expect(screen.getByText('回车建配置')).toBeInTheDocument();
		});
		expect(get(logs).some(e => e.message === '已创建配置: 回车建配置')).toBe(true);
	});

	it('剪贴板空内容时导入直接返回，不产生日志', async () => {
		const clip = installClipboardStub();
		clip.setNextRead('');
		render(Page);

		await fireEvent.click(screen.getByRole('button', { name: '导入' }));
		await new Promise(r => setTimeout(r, 100));
		expect(get(logs).some(e => e.message.includes('导入成功') || e.message.includes('导入失败'))).toBe(false);
	});
});
