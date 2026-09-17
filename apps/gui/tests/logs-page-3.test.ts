import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/svelte';
import Page from '../src/routes/logs/+page.svelte';
import { logs } from '$lib/stores/logs';

/**
 * 日志页第三组：事件缓冲溢出横幅、来源筛选、runtime 日志的 RT 标记、
 * 自动滚动关闭时的跳转按钮抑制。
 * 智能滚动见 logs-page-2，筛选/导出见 logs-page。
 */

beforeEach(() => {
	delete (window as any).__TAURI_INVOKE__;
	logs.clear();
	logs.notifyDropped(0);
});

afterEach(() => {
	vi.restoreAllMocks();
});

function stubGeometry(el: HTMLElement, scrollHeight: number, clientHeight: number) {
	Object.defineProperty(el, 'scrollHeight', { value: scrollHeight, configurable: true });
	Object.defineProperty(el, 'clientHeight', { value: clientHeight, configurable: true });
}

describe('日志页：事件缓冲溢出横幅', () => {
	it('dropped 计数大于 0 时显示丢弃横幅', async () => {
		logs.add('一条', 'info');
		render(Page);
		expect(screen.queryByText(/累计丢弃/)).not.toBeInTheDocument();

		logs.notifyDropped(12);
		await waitFor(() => {
			expect(screen.getByText(/累计丢弃 12 条事件/)).toBeInTheDocument();
		});
	});
});

describe('日志页：来源标记与筛选', () => {
	it('runtime 日志显示 RT 标记，来源筛选仅保留 runtime', async () => {
		logs.add('gui 日志', 'info');
		logs.addRuntime('runtime 日志', 'info');
		render(Page);

		// 两种来源并存：GUI 与 RT 标记（span.log-source）
		expect(document.querySelector('.log-source.src-gui')).toBeInTheDocument();
		expect(document.querySelector('.log-source.src-runtime')).toBeInTheDocument();

		// 来源筛选（按钮组切到 Runtime）
		const group = screen.getByRole('group', { name: '日志来源筛选' });
		await fireEvent.click(within(group).getByRole('button', { name: 'Runtime' }));
		await waitFor(() => {
			expect(screen.getByText('runtime 日志')).toBeInTheDocument();
		});
		expect(screen.queryByText('gui 日志')).not.toBeInTheDocument();
	});
});

describe('日志页：关闭自动滚动时无跳转按钮', () => {
	it('autoScroll 关闭后即使上滚也不出现跳转按钮', async () => {
		logs.add('初始', 'info');
		const { container } = render(Page);
		const logBox = container.querySelector('.log-container') as HTMLElement;
		stubGeometry(logBox, 1000, 200);

		const auto = screen.getByRole('checkbox', { name: '自动滚动' }) as HTMLInputElement;
		await fireEvent.click(auto);

		logBox.scrollTop = 0;
		await fireEvent.scroll(logBox);
		logs.add('离屏日志', 'info');
		await new Promise(r => setTimeout(r, 50));
		// {#if autoScroll && userScrolled && newCount > 0}：autoScroll=false → 按钮不出现
		expect(screen.queryByText(/条新日志/)).not.toBeInTheDocument();
	});
});

describe('日志页：滚动到底部时未上滚的常规滚动', () => {
	it('容器已在底部且未上滚时滚动事件为空操作，不产生跳转按钮', async () => {
		logs.add('初始', 'info');
		const { container } = render(Page);
		const logBox = container.querySelector('.log-container') as HTMLElement;
		// 默认 scrollTop=0 且 scrollHeight-clientHeight=0 → isAtBottom 为 true，
		// userScrolled 仍为 false → handleScroll 内 if (userScrolled) 的 else 空侧
		await fireEvent.scroll(logBox);
		await new Promise(r => setTimeout(r, 50));
		expect(screen.queryByText(/条新日志/)).not.toBeInTheDocument();
		expect(screen.getByText('初始')).toBeInTheDocument();
	});
});
