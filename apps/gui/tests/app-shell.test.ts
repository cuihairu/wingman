import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/svelte';
import { get } from 'svelte/store';
import App from '../src/App.svelte';
import { router } from '$lib/router.svelte';
import { connection } from '$lib/stores/connection';
import { logs } from '$lib/stores/logs';
import { scripts } from '$lib/stores/scripts';
import { triggers } from '$lib/stores/triggers';
import { profiles } from '$lib/stores/profiles';
import { screen as screenStore } from '$lib/stores/screen';
import {
	installCanvasStub,
	installPointerCaptureStub,
	installTolerantStyleAttrStub,
	uninstallTolerantStyleAttrStub,
} from './helpers/test-utils';

beforeEach(async () => {
	delete (window as any).__TAURI_INVOKE__;
	localStorage.removeItem('wingman-settings');
	localStorage.removeItem('wingman.theme');
	installPointerCaptureStub();
	installTolerantStyleAttrStub();
	installCanvasStub();
	logs.clear();
	await connection.disconnect();
	for (const s of get(scripts)) await scripts.stop(s.name);
	triggers.set([]);
	for (const p of get(profiles)) await profiles.remove(p.id);
	screenStore.clear();
	router.navigate('dashboard');
});

afterEach(() => {
	uninstallTolerantStyleAttrStub();
	screenStore.clear();
});

describe('App 外壳（dev 模式）', () => {
	it('挂载加载开发数据并渲染侧边栏与仪表板', async () => {
		const { container } = render(App);
		expect(screen.getAllByText('Wingman').length).toBeGreaterThan(0); // Sidebar 品牌
		// 页面标题与侧边栏导航文本重复，限定内容区查询
		const content = within(container.querySelector('.content-area')!);
		expect(content.getByText('系统状态')).toBeInTheDocument(); // Dashboard 区块

		// dev 数据已加载
		await waitFor(() => {
			expect(get(profiles).some(p => p.id === 'game1')).toBe(true);
		});
		expect(get(logs).some(e => e.message === '开发模式已启动')).toBe(true);
	});

	it('暂停横幅出现并可恢复', async () => {
		const { container } = render(App);
		expect(container.querySelector('.pause-banner')).toBeNull();

		connection.togglePause();
		const banner = await waitFor(() => {
			const el = container.querySelector('.pause-banner')!;
			expect(el).not.toBeNull();
			return within(el);
		});
		expect(banner.getByText(/所有功能已暂停/)).toBeInTheDocument();
		// TopBar 也有"恢复运行"按钮，限定横幅内点击
		await fireEvent.click(banner.getByRole('button', { name: '恢复运行' }));
		await waitFor(() => {
			expect(container.querySelector('.pause-banner')).toBeNull();
		});
	});

	it('路由切换渲染对应页面', async () => {
		const { container } = render(App);
		const content = () => within(container.querySelector('.content-area')!);

		router.navigate('logs');
		await waitFor(() => {
			expect(content().getByText('系统日志')).toBeInTheDocument();
		});

		router.navigate('settings');
		await waitFor(() => {
			expect(content().getByText('连接设置')).toBeInTheDocument();
		});

		router.navigate('triggers');
		await waitFor(() => {
			expect(content().getByText('管理和配置自动化触发器')).toBeInTheDocument();
		});

		router.navigate('macros');
		await waitFor(() => {
			// dev 挂载已 connect，宏页可用
			expect(content().getByText('准备就绪')).toBeInTheDocument();
		});

		router.navigate('screen');
		await waitFor(() => {
			expect(content().getByText(/截图预览、拖拽框选区域/)).toBeInTheDocument();
		});

		router.navigate('scripts');
		await waitFor(() => {
			expect(content().getByText('脚本管理')).toBeInTheDocument();
		});

		router.navigate('dashboard');
		await waitFor(() => {
			expect(content().getByText('系统状态')).toBeInTheDocument();
		});

		// 多轮跨页切换：覆盖 else-if 链上 triggers 之后的分支分发路径
		for (const [route, marker] of [
			['triggers', '管理和配置自动化触发器'],
			['macros', '准备就绪'],
			['triggers', '管理和配置自动化触发器'],
			['dashboard', '系统状态'],
		] as const) {
			router.navigate(route);
			await waitFor(() => {
				expect(content().getByText(marker)).toBeInTheDocument();
			});
		}
	});
});
