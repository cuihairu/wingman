import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { get } from 'svelte/store';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import App from '../src/App.svelte';
import TopBar from '$lib/components/layout/TopBar.svelte';
import Page from '../src/routes/macros/+page.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { profiles } from '$lib/stores/profiles';
import { macros } from '$lib/stores/macros';
import { installCanvasStub, installPointerCaptureStub, installTolerantStyleAttrStub, uninstallTolerantStyleAttrStub } from './helpers/test-utils';

/**
 * 布局第三组：TopBar 远程链路状态全类型、配置下拉空描述回退、
 * 宏页录制中横幅。
 */

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	installCanvasStub();
	logs.clear();
	for (const p of get(profiles)) await profiles.remove(p.id);
	await connection.connect();
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	vi.restoreAllMocks();
});

describe('TopBar：远程链路状态徽标', () => {
	it.each([
		['connected', '远程在线'],
		['reconnecting', '远程重连'],
		['connecting', '远程连接中'],
		['error', '远程异常'],
		['disconnected', '远程离线'],
	])('远程状态 %s 显示 %s', async (state, text) => {
		connection.setRemoteState(state as any, '');
		render(TopBar);
		expect(screen.getByText(text)).toBeInTheDocument();
	});
});

describe('TopBar：配置下拉空描述回退', () => {
	it('无描述的配置显示 id 作为副标题', async () => {
		await profiles.importFromJson(JSON.stringify({
			id: 'nodesc',
			name: '无描述配置',
			description: '',
			version: '1.0',
			window: { title: '', className: '', processName: '', exactMatch: false, fullscreen: false },
			colors: [], images: [], triggers: [], scripts: [],
			hotkeys: { start: ['F5'], stop: ['F6'], pause: ['F7'], emergencyStop: ['F12'] },
			settings: {},
		}));
		const { container } = render(TopBar);
		await fireEvent.click(container.querySelector('.profile-btn')!);
		await waitFor(() => {
			expect(screen.getByText('nodesc')).toBeInTheDocument();
		});
	});
});

describe('宏页：录制状态横幅', () => {
	it('dev 模式空闲横幅不点亮', async () => {
		render(Page);
		// dev 无 invoke：record 是 no-op，横幅保持空闲态
		expect(document.querySelector('.recording-banner')!.className).not.toContain('active');
		expect(get(macros).recording).toBe(false);
	});
});

describe('App：外壳渲染', () => {
	it('dev 模式挂载外壳渲染内容区', async () => {
		const { container } = render(App);
		await waitFor(() => {
			expect(container.querySelector('.content-area')).toBeTruthy();
		});
		expect(get(logs).some(e => e.message === '开发模式已启动')).toBe(true);
	});
});
